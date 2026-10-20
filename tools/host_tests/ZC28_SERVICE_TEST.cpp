// Host software-scheduling model. No device, DMA transaction, IRQ timing or
// throughput is modeled. All changed queue methods, both start functions,
// completion restart and the unchanged Apple frontend are actual source.
M*chain(unsigned n){M*h=nullptr;while(n--){auto m=new M;m->next=h;h=m;}return h;}
void dispose(M*m){while(m){auto next=m->next;m->next=nullptr;mbuf_freem(m);m=next;}}
static std::function<void(_ifnet*)> startHook;
void startDispatch(_ifnet*f){if(startHook)startHook(f);}
struct Fixture {
 IONetworkInterface*i=new IONetworkInterface;Stats stats;Hal hal;AirportItlwm driver{&hal};
 _ifnet&f=hal.ic.ic_ac.ac_if;KgpOutputQueue*q;
 explicit Fixture(unsigned cap):q(KgpOutputQueue::create(&f,cap)){
  f.iface=i;f.netStat=&stats;f.if_start=startDispatch;f.if_snd.queue=q;q->openPull();
 }
 ~Fixture(){i->onSignal=nullptr;startHook=nullptr;while(!i->input.empty()){mbuf_freem(i->input.front());i->input.pop_front();}if(f.if_snd.queue)ifq_destroy(&f.if_snd);i->release();assert(packets==0&&objects==0);}
 void full(unsigned n){assert(q->lockEnqueue(chain(n)));ifq_reservation t{};assert(!ifq_reserve_batch(&f.if_snd,&t,32));}
 void snapshot(uint64_t(&s)[8]){ifq_service_snapshot(&f.if_snd,s);}
};
void amplification(){
 // Identical queue+producer behavior with depth zero reproduces the old
 // per-freed-slot wake. Synchronous callback is a stronger reentry model
 // than the real asynchronous output-thread signal, not a timing claim.
 for(bool scoped:{false,true}){
  Fixture x(64);x.full(64);unsigned wakes=0;
  x.i->onSignal=[&]{++wakes;ifq_reservation t{};auto n=ifq_reserve_batch(&x.f.if_snd,&t,32);assert(n);assert(ifq_commit(&t,chain(n)));assert(!ifq_reserve_batch(&x.f.if_snd,&t,32));};
  ifq_service scope{};if(scoped)ifq_begin_service(&x.f.if_snd,&scope);
  for(unsigned n=0;n<32;++n){mbuf_freem(ifq_dequeue(&x.f.if_snd));if(scoped)assert(wakes==0);}
  if(scoped)ifq_end_service(&scope);
  assert(wakes==(scoped?1U:32U));assert(x.q->getSize()==64);
  uint64_t s[8];x.snapshot(s);assert(s[0]==unsigned(scoped)&&s[1]==unsigned(scoped)&&s[5]==0);
  assert(s[2]==(scoped?32U:0U)&&s[3]==wakes&&s[4]==unsigned(scoped));
 }
}
void scopesAndLifecycle(){
 Fixture x(4);ifq_service a{},b{};uint64_t s[8]{};ifq_reservation t{};
 x.full(4);unsigned signals=x.i->signals;
 ifq_begin_service(&x.f.if_snd,&a);ifq_begin_service(&x.f.if_snd,&b);
 mbuf_freem(ifq_dequeue(&x.f.if_snd));x.snapshot(s);assert(s[0]==2&&s[1]==0&&s[4]==2&&s[5]==2);
 ifq_end_service(&b);assert(x.i->signals==signals);ifq_end_service(&a);assert(x.i->signals==signals+1);
 ifq_end_service(&a);assert(x.i->signals==signals+1);x.snapshot(s);assert(s[1]==2&&s[5]==0);
 // Scope token is consumed before a signal can synchronously reenter service.
 assert(x.q->lockEnqueue(new M));assert(!ifq_reserve(&x.f.if_snd,&t));ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));
 bool reentered=false;x.i->onSignal=[&]{assert(!a.queue);ifq_end_service(&a);ifq_service inner{};ifq_begin_service(&x.f.if_snd,&inner);ifq_end_service(&inner);reentered=true;};
 ifq_end_service(&a);assert(reentered);x.i->onSignal=nullptr;
 // Closing/flush cancels the old waiter. Reservation remains counted until
 // cancelled; a reopened epoch cannot commit a stale chain.
}
void lifecycle(){
 Fixture x(4);ifq_service a{};ifq_reservation old{},failed{};
 assert(ifq_reserve_batch(&x.f.if_snd,&old,2)==2);assert(x.q->lockEnqueue(chain(2)));assert(!ifq_reserve(&x.f.if_snd,&failed));
 ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));unsigned signals=x.i->signals;
 uint64_t before[8],after[8];x.snapshot(before);assert(before[5]==1);
 ifq_flush(&x.f.if_snd);ifq_start_pull(&x.f.if_snd);auto*m=chain(2);assert(!ifq_commit(&old,m));dispose(m);
 ifq_end_service(&a);assert(x.i->signals==signals&&x.q->getSize()==0);
 x.snapshot(after);assert(after[0]==before[0]&&after[1]==before[1]+1&&after[5]==0); // cumulative across epoch
 // Return capacity is not enough if ordinary pull is closed. EAPOL remains
 // admitted through the same bounded queue without a new service policy.
 ifq_stop_pull(&x.f.if_snd);ifq_begin_service(&x.f.if_snd,&a);
 assert(!ifq_reserve(&x.f.if_snd,&failed));assert(x.q->lockEnqueue(new M));mbuf_freem(ifq_dequeue(&x.f.if_snd));ifq_end_service(&a);assert(x.i->signals==signals);
 ifq_start_pull(&x.f.if_snd);x.full(4);ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));
 assert(ifq_reserve(&x.f.if_snd,&old));ifq_flush(&x.f.if_snd);ifq_destroy(&x.f.if_snd); // two private tokens retain detached queue
 assert(!x.f.if_snd.queue);ifq_end_service(&a);assert(!a.queue&&x.i->signals==signals);ifq_cancel(&old);assert(!old.queue);
 // Absent queue is a no-op snapshot and service, no allocation or callback.
 uint64_t s[8]={9,9,9,9,9,9,9,9};assert(!ifq_service_snapshot(&x.f.if_snd,s));for(auto v:s)assert(v==9);
 ifq_begin_service(&x.f.if_snd,&a);assert(!a.queue);ifq_end_service(&a);
}
void reentrantDetach(){
 Fixture x(2);x.full(2);ifq_service a{};ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));
 bool called=false;x.i->onSignal=[&]{assert(!a.queue);ifq_destroy(&x.f.if_snd);ifq_end_service(&a);called=true;};
 ifq_end_service(&a);assert(called&&!x.f.if_snd.queue&&packets==0);x.i->onSignal=nullptr;
}
void newWaiterAcrossEpoch(){
 Fixture x(4);x.full(4);ifq_service outer{},inner{};ifq_begin_service(&x.f.if_snd,&outer);
 ifq_flush(&x.f.if_snd);ifq_start_pull(&x.f.if_snd);x.full(4);auto signals=x.i->signals.load();
 ifq_begin_service(&x.f.if_snd,&inner);mbuf_freem(ifq_dequeue(&x.f.if_snd));ifq_end_service(&inner);assert(x.i->signals==signals);
 ifq_end_service(&outer);assert(x.i->signals==signals+1);uint64_t s[8];x.snapshot(s);assert(s[0]==2&&s[1]==2&&s[5]==0&&s[6]>=2&&s[7]==0);
}
void signalSleepOrders(){
 Fixture x(2);ifq_service a{};ifq_reservation t{};
 x.full(2);x.i->begin();ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));ifq_end_service(&a);assert(!x.i->sleepAfterNoResources());
 assert(x.q->lockEnqueue(new M));assert(!ifq_reserve(&x.f.if_snd,&t));x.i->begin();assert(x.i->sleepAfterNoResources());
 ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));auto signals=x.i->signals.load();assert(x.i->flags==1);ifq_end_service(&a);assert(x.i->signals==signals+1&&x.i->flags==3);
 // A competing producer consuming all returned capacity prevents final wake;
 // cancelling that reservation later satisfies the still-owned obligation.
 assert(x.q->lockEnqueue(new M));assert(!ifq_reserve(&x.f.if_snd,&t));ifq_begin_service(&x.f.if_snd,&a);mbuf_freem(ifq_dequeue(&x.f.if_snd));assert(ifq_reserve(&x.f.if_snd,&t));signals=x.i->signals;ifq_end_service(&a);assert(x.i->signals==signals);ifq_cancel(&t);assert(x.i->signals==signals+1);
}
void actualStartRoutes(){
 for(bool mapped:{false,true})for(unsigned branch=0;branch<11;++branch){
  auto*i=new IONetworkInterface;Stats stats;ItlIwx hw;hw.mapped=mapped;
  auto&f=hw.com.sc_ic.ic_ac.ac_if;f.iface=i;f.netStat=&stats;f.if_start=startDispatch;f.if_softc=&hw.com;
  auto*q=KgpOutputQueue::create(&f,3);f.if_snd.queue=q;q->openPull();auto*initial=chain(3);if(branch==8){initial->len=0;initial->pullupFail=true;}if(branch==9)initial->encapFail=true;assert(q->lockEnqueue(initial));ifq_reservation t{};assert(!q->reserve(&t));
  if(branch==1)f.if_flags&=~IFF_RUNNING;
  if(branch==2)f.if_snd.ifq_oactive=1;
  if(branch==3)hw.com.qfullmsk=1U<<2;
  if(branch==4)hw.com.sc_flags=IWX_FLAG_TXFLUSH;
  if(branch==5)hw.com.sc_ic.ic_xflags=IEEE80211_F_TX_MGMT_ONLY;
  if(branch==6){hw.beforeTx=[&]{ifq_flush(&f.if_snd);ifq_start_pull(&f.if_snd);};}
  if(branch==7)hw.txError=5;
  if(branch==10)hw.loop.gated=false;
  auto signals=i->signals.load();
  hw.beforeTx=branch==0?std::function<void()>([&]{assert(i->signals==signals);}):hw.beforeTx;
  Start2LContext c;auto result=ItlIwx::_iwx_start_task(nullptr,&f,&c,nullptr,nullptr);assert(c.entered);
  uint64_t s[8];ifq_service_snapshot(&f.if_snd,s);assert(s[0]==unsigned(branch!=10)&&s[1]==unsigned(branch!=10)&&s[4]==unsigned(branch!=10)&&s[5]==0);
  if(branch==1||branch==2){assert(result==kIOReturnError&&hw.submissions==0&&i->signals==signals);}
  else {assert(result==kIOReturnSuccess);if(branch>=3&&branch<=5)assert(hw.submissions==0&&i->signals==signals);
   else if(branch==6)assert(hw.submissions==1&&q->getSize()==0&&i->signals==signals);
   else assert(hw.submissions==((branch==8||branch==9)?2U:3U)&&q->getSize()==0&&i->signals==signals+1);}
  if(branch==7)assert(stats.outputErrors==3&&stats.outputPackets==0);
  if(branch==8||branch==9)assert(stats.outputErrors==1&&stats.outputPackets==2);
  ifq_destroy(&f.if_snd);i->release();assert(packets==0&&objects==0);
 }
 // Verify small final producer batches are serviced immediately in both
 // routes, never held waiting for a chosen batching threshold.
 for(bool mapped:{false,true}){
  auto*i=new IONetworkInterface;Stats stats;ItlIwx hw;hw.mapped=mapped;
  auto&f=hw.com.sc_ic.ic_ac.ac_if;f.iface=i;f.netStat=&stats;f.if_start=startDispatch;f.if_softc=&hw.com;
  auto*q=KgpOutputQueue::create(&f,64);f.if_snd.queue=q;q->openPull();
  Hal hal;hal.external=&hw.com.sc_ic;AirportItlwm driver{&hal};
  startHook=[&](_ifnet*fp){assert(ItlIwx::_iwx_start_task(nullptr,fp,nullptr,nullptr,nullptr)==kIOReturnSuccess);};
  for(unsigned n=0;n<67;++n)i->input.push_back(new M);
  assert(driver.outputStart(i,0)==kIOReturnSuccess);assert(i->pulls==4&&hw.submissions==67&&q->getSize()==0);
  uint64_t s[8];ifq_service_snapshot(&f.if_snd,s);assert(s[0]==3&&s[1]==3&&s[5]==0);
  // Unchanged frontend refuses to dequeue under oactive. Software capacity
  // wake is allowed to be redundant; a later positive low-water completion
  // still signals the host, then the same frontend makes forward progress.
  assert(q->lockEnqueue(chain(64)));ifq_reservation t{};assert(!q->reserve(&t));
  ifq_service scope{};ifq_begin_service(&f.if_snd,&scope);for(unsigned n=0;n<32;++n)mbuf_freem(ifq_dequeue(&f.if_snd));
  hw.com.qfullmsk=1U<<2;f.if_snd.ifq_oactive=1;auto signals=i->signals.load();ifq_end_service(&scope);assert(i->signals==signals+1);
  for(unsigned n=0;n<7;++n)i->input.push_back(new M);auto pulls=i->pulls;
  assert(driver.outputStart(i,0)==kIOReturnNoResources&&i->pulls==pulls&&i->input.size()==7);
  iwx_tx_ring ring;ring.queued=192;auto submits=hw.submissions;signals=i->signals;
  hw.iwx_clear_oactive(&hw.com,&ring);assert(hw.submissions==submits&&i->signals==signals&&f.if_snd.ifq_oactive);
  ring.queued=191;hw.beforeTx=[&]{hw.com.qfullmsk=1U<<2;};
  hw.iwx_clear_oactive(&hw.com,&ring);
  assert(f.if_snd.ifq_oactive&&hw.submissions==submits+1&&q->getSize()==31&&i->signals==signals+1); // old software waiter was already signalled; hardware signal remains
  assert(driver.outputStart(i,0)==kIOReturnNoResources&&i->pulls==pulls&&i->input.size()==7);
  signals=i->signals;hw.iwx_clear_oactive(&hw.com,&ring);
  assert(!f.if_snd.ifq_oactive&&hw.submissions==submits+32&&i->signals==signals+1);
  assert(driver.outputStart(i,0)==kIOReturnSuccess&&hw.submissions==submits+39&&i->input.empty());
  // When a genuinely armed software-capacity waiter exists during hardware
  // refill, retain both distinct wake obligations: service end + completion.
  assert(q->lockEnqueue(chain(64)));assert(!q->reserve(&t));
  f.if_snd.ifq_oactive=1;hw.com.qfullmsk=1U<<2;signals=i->signals;submits=hw.submissions;
  hw.beforeTx=[&]{hw.com.qfullmsk=1U<<2;};hw.iwx_clear_oactive(&hw.com,&ring);
  assert(f.if_snd.ifq_oactive&&hw.submissions==submits+1&&q->getSize()==63&&i->signals==signals+2);
  // A different full ring prevents clearing global oactive, but the existing
  // below-low completion still signals; no new hardware admission bypass.
  f.if_snd.ifq_oactive=1;hw.com.qfullmsk=(1U<<2)|(1U<<3);signals=i->signals;submits=hw.submissions;
  hw.iwx_clear_oactive(&hw.com,&ring);assert(f.if_snd.ifq_oactive&&hw.com.qfullmsk==(1U<<3)&&hw.submissions==submits&&i->signals==signals+1);
  startHook=nullptr;ifq_destroy(&f.if_snd);i->release();assert(packets==0&&objects==0);
 }
}
void concurrent(){
 Fixture x(67);std::atomic<unsigned> done{0},accepted{0},consumed{0},flushed{0};
 auto producer=[&](unsigned limit){for(unsigned n=0;n<18000;++n){ifq_reservation t{};if(!x.q->reserve(&t,limit)){std::this_thread::yield();continue;}auto count=1+n%t.count;auto*m=chain(count);if(x.q->finish(&t,m))accepted+=count;else dispose(m);}++done;};
 std::thread a(producer,32),b(producer,7),internal([&]{for(unsigned n=0;n<18000;++n){auto*m=new M;if(x.q->lockEnqueue(m))++accepted;else mbuf_freem(m);}++done;}),
 consumer([&]{while(done!=3||x.q->getSize()){ifq_service t{};ifq_begin_service(&x.f.if_snd,&t);for(unsigned n=0;n<34;++n){auto*m=ifq_dequeue(&x.f.if_snd);if(!m)break;++consumed;mbuf_freem(m);}ifq_end_service(&t);std::this_thread::yield();}}),
 reset([&]{for(unsigned n=0;n<600;++n){flushed+=x.q->lockFlush();x.q->openPull();std::this_thread::yield();}}),
 observer([&]{uint64_t prev[8]={};while(done!=3){uint64_t s[8];x.snapshot(s);for(unsigned n=0;n<5;++n)assert(s[n]>=prev[n]);assert(s[1]<=s[0]&&s[5]==s[0]-s[1]&&s[5]<=s[4]);std::copy(s,s+8,prev);std::this_thread::yield();}});
 a.join();b.join();internal.join();consumer.join();reset.join();observer.join();flushed+=x.q->lockFlush();
 uint64_t s[8];x.snapshot(s);assert(s[5]==0&&s[0]==s[1]&&x.q->getSize()==0&&accepted==consumed+flushed&&packets==0);
 std::cout<<"Concurrent conservation "<<accepted<<"="<<consumed<<"+"<<flushed<<"; scopes "<<s[0]<<", deferred checks "<<s[2]<<", capacity wakes "<<s[3]<<"\n";
}
int main(){amplification();scopesAndLifecycle();lifecycle();reentrantDetach();newWaiterAcrossEpoch();signalSleepOrders();actualStartRoutes();concurrent();assert(packets==0&&objects==0);
 std::cout<<"PASS actual ZC28 service queue, both full IWX start functions, unchanged frontend and completion: per-slot vs service-end wake, retained/reentrant/nested tokens, final batch, oactive low-water handoff, lifecycle/epoch cancellation, immediate unscoped behavior, locked snapshots and concurrent ownership; no callback or packet free under queue lock. No physical or DMA claim.\n";
}
