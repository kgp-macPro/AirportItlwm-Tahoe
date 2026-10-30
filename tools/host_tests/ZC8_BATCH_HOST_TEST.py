#!/usr/bin/env python3
"""ZC8 production-code ownership/batch models; no network or throughput measurement."""
from pathlib import Path
import ast,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2]
old=ast.parse((r/'tools/host_tests/ZC7_ADMISSION_HOST_TEST.py').read_text())
mock=next(ast.literal_eval(n.value) for n in old.body if isinstance(n,ast.Assign) and any(isinstance(x,ast.Name) and x.id=='mock' for x in n.targets))
mock=mock.replace('#include <vector>','#include <vector>\n#include <functional>\n#include <algorithm>')
mock=mock.replace('struct M {M*next=nullptr;','struct M {M*next=nullptr;int flags=1,type=1,id=0;')
mock=mock.replace('struct IONetworkInterface:Obj {',r"""
using IOReturn=int; using IOOptionBits=unsigned;
constexpr int kIOReturnSuccess=0,kIOReturnNoResources=1,kIOReturnNoFrames=2,kIOReturnOutputSuccess=0,kIOReturnOutputDropped=3;
constexpr int MBUF_TYPE_FREE=0,MBUF_PKTHDR=1,IEEE80211_S_RUN=4;
int mbuf_type(M*m){return m->type;} int mbuf_flags(M*m){return m->flags;}
struct IONetworkInterface:Obj {
 std::deque<M*> input;unsigned pulls=0;std::function<void()> before;int error=0;bool overReturn=false;
 IOReturn dequeueOutputPackets(uint32_t count,M**head){
  ++pulls;*head=nullptr;if(before){auto f=before;before=nullptr;f();}
  if(error)return error;if(input.empty())return kIOReturnNoFrames;
  if(overReturn)++count;M*tail=nullptr;
  while(count--&&!input.empty()){auto m=input.front();input.pop_front();m->next=nullptr;
   if(tail)tail->next=m;else *head=m;tail=m;}return kIOReturnSuccess;
 }
""")
mock=mock.replace('struct _ifnet{IONetworkInterface*iface;};',r"""
class IOPacketQueue;
struct _ifqueue{unsigned ifq_oactive=0;IOPacketQueue*queue=nullptr;};
struct Stats{unsigned outputErrors=0;};
struct _ifnet{IONetworkInterface*iface;_ifqueue if_snd;Stats *netStat;void(*if_start)(_ifnet*);};
""")
q=(r/'itl80211/openbsd/sys/_ifq.cpp').read_text()
body=q[q.index('// ZC7 ADMISSION BEGIN'):q.index('void ifq_init(')]
cpp=(r/'AirportItlwm/AirportItlwm.cpp').read_text()
start=cpp[cpp.index('IOReturn AirportItlwm::outputStart('):cpp.index('\n#endif',cpp.index('IOReturn AirportItlwm::outputStart('))]
batch=cpp[cpp.index('UInt32 AirportItlwm::outputReservedBatch('):cpp.index('// ZC8 BATCH END')]
front=r"""
unsigned ifq_is_oactive(_ifqueue*q){return q->ifq_oactive;}
struct Controller{struct{_ifnet ac_if;}ic_ac;int ic_state=IEEE80211_S_RUN;};
struct Hal{Controller ic;Controller*get80211Controller(){return &ic;}};
struct AirportItlwm{Hal*fHalService;void freePacket(M*m){mbuf_freem(m);}
 IOReturn outputStart(IONetworkInterface*,IOOptionBits);
 UInt32 outputReservedBatch(mbuf_t,ifq_reservation*);
};
"""
tests=r"""
static unsigned starts=0;static std::vector<int> delivered;
void consume(_ifnet*f){++starts;while(auto m=f->if_snd.queue->lockDequeue()){delivered.push_back(m->id);mbuf_freem(m);}}
M*chain(unsigned n){M*h=nullptr;while(n--){auto m=new M;m->next=h;h=m;}return h;}
void dispose(M*m){while(m){auto n=m->next;m->next=nullptr;mbuf_freem(m);m=n;}}
int main(){
 auto*i=new IONetworkInterface;Stats stats;Hal hal;auto&f=hal.ic.ic_ac.ac_if;
 f.iface=i;f.netStat=&stats;f.if_start=consume;AirportItlwm driver{&hal};
 auto*q=KgpOutputQueue::create(&f,64);f.if_snd.queue=q;q->openPull();
 // 67 packets require 3 accepted batches; no wait for 32 packets at low load.
 for(int n=0;n<67;++n){auto m=new M;m->id=n;i->input.push_back(m);}
 assert(driver.outputStart(i,0)==kIOReturnSuccess);assert(starts==3&&i->pulls==4&&delivered.size()==67);
 for(int n=0;n<67;++n)assert(delivered[n]==n);
 assert(q->getSize()==0&&packets==0);
 // Full capacity prevents all macOS dequeue calls; return wakes outside lock.
 assert(q->lockEnqueue(chain(64)));auto pulls=i->pulls;
 i->input.push_back(new M);i->begin();assert(driver.outputStart(i,0)==kIOReturnNoResources);
 assert(i->pulls==pulls);assert(i->sleepAfterNoResources());auto signals=i->signals.load();
 consume(&f);assert(i->signals>signals);assert(driver.outputStart(i,0)==kIOReturnSuccess);assert(packets==0);
 // Partial capacity limits dequeue; a bulk producer and EAPOL cannot steal reserved slots.
 ifq_reservation t{};assert(ifq_reserve_batch(&f.if_snd,&t,32)==32);assert(q->lockEnqueue(chain(32)));
 auto m=new M;assert(!q->lockEnqueue(m));mbuf_freem(m);
 assert(ifq_commit(&t,chain(3)));assert(q->getSize()==35);consume(&f);assert(packets==0);
 // Stop/rearm during macOS dequeue: epoch rejects chain, one disposal per packet.
 for(int n=0;n<7;++n)i->input.push_back(new M);
 i->before=[&]{q->lockFlush();q->openPull();};auto before=starts;
 assert(driver.outputStart(i,0)==kIOReturnNoResources);assert(starts==before&&q->getSize()==0&&packets==0);
 // Invalid metadata and oversized API results fail closed; no partial commit/replay.
 for(int n=0;n<3;++n)i->input.push_back(new M);i->input[1]->flags=0;
 assert(driver.outputStart(i,0)==kIOReturnNoResources);assert(packets==0&&starts==before);
 i->overReturn=true;for(int n=0;n<33;++n)i->input.push_back(new M);
 assert(driver.outputStart(i,0)==kIOReturnNoResources);i->overReturn=false;assert(packets==0&&starts==before);
 // Error/no-frames and unused capacity leave no reservations behind.
 i->error=kIOReturnNoFrames;assert(driver.outputStart(i,0)==kIOReturnSuccess);i->error=0;
 assert(ifq_reserve_batch(&f.if_snd,&t,100)==64);ifq_cancel(&t);
 // In-flight batch keeps detached queue alive until cancellation, with no wake.
 assert(ifq_reserve_batch(&f.if_snd,&t,32)==32);q->closePull(true);q->release();ifq_cancel(&t);
 assert(packets==0);f.if_snd.queue=nullptr;
 // Concurrent reservation sizes, internal producers, consumer and reset/flush.
 q=KgpOutputQueue::create(&f,67);f.if_snd.queue=q;q->openPull();
 std::atomic<unsigned> done{0},accepted{0},consumed{0},flushed{0};
 auto producer=[&](unsigned limit){for(unsigned n=0;n<8000;++n){ifq_reservation t{};
   if(!q->reserve(&t,limit)){std::this_thread::yield();continue;}
   unsigned count=1+n%t.count;auto m=chain(count);
   if(q->finish(&t,m))accepted+=count;else dispose(m);
  }++done;};
 std::thread a(producer,32),b(producer,7),internal([&]{for(unsigned n=0;n<8000;++n){auto m=new M;
   if(q->lockEnqueue(m))++accepted;else mbuf_freem(m);}++done;}),
 consumer([&]{while(done!=3||q->getSize()){if(auto m=q->lockDequeue()){++consumed;mbuf_freem(m);}else std::this_thread::yield();}}),
 reset([&]{for(unsigned n=0;n<300;++n){flushed+=q->lockFlush();q->openPull();std::this_thread::yield();}});
 a.join();b.join();internal.join();reset.join();consumer.join();flushed+=q->lockFlush();
 assert(accepted==consumed+flushed);assert(packets==0);q->closePull(true);q->release();i->release();assert(objects==0);
 std::cout<<"PASS extracted production batch/frontend: bounded reservations, FIFO, short batches, no dequeue when full, exactly-once ownership, stop epochs, detach, invalid-chain rejection, capacity/wake races; concurrent conservation "<<accepted<<"="<<consumed<<"+"<<flushed<<"\n";
}
"""
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc8-batch-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+body+front+start+batch+tests)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=180);print('PASS',san)
