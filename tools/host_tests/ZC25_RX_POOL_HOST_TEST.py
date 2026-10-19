#!/usr/bin/env python3
"""Compile extracted RX implementation with ownership/ONCE-worker KPI mocks.
This verifies driver state transitions, not XNU scheduling performance or physical DMA.
"""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[2]
cpp=(root/'itlwm/hal_iwx/ItlIwx.cpp').read_text();hpp=(root/'itlwm/hal_iwx/ItlIwx.hpp').read_text()
block=cpp[cpp.index('// B1-ZC5 BEGIN:'):cpp.index('// B1-ZC5 END')]
cut=block.index('bool ItlIwx::rxZcSetup')
storage,methods=block[:cut],block[cut:]
header=hpp[hpp.index('    // B1-ZC4:'):hpp.index('    // PC1 control is serialized')]
mock=r'''
#include <cassert>
#include <algorithm>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
using UInt64=uint64_t;using UInt32=uint32_t;using IOReturn=int;using mach_vm_address_t=uintptr_t;
constexpr unsigned IWX_RX_MQ_RING_COUNT=512,IWX_RBUF_SIZE=4096,PAGE_SIZE=4096;
constexpr int kIOMemoryMapperNone=0x800;
#define MIN(a,b) std::min(a,b)
constexpr int MBUF_EXT=1,MBUF_PKTHDR=2,MBUF_TYPE_DATA=1,MBUF_DONTWAIT=0,MBUF_WAITOK=1;
constexpr int kIODirectionInOut=3,kIODirectionIn=1,kIODirectionOut=2,kIOReturnSuccess=0,kIOReturnNotReady=-1,kIODMACommandOutputHost64=0;
void *kernel_task=reinterpret_cast<void*>(1);
static std::atomic<int> livePackets{0},liveClusters{0},liveObjects{0},liveMD{0},liveCMD{0},callbacks{0};
static std::atomic<unsigned> prepares{0},completes{0},lateCopies{0},syncIns{0};
static thread_local bool unsafeGate=false;
static thread_local unsigned simpleLocks=0;
static void outside(){assert(!unsafeGate&&!simpleLocks);}
static std::atomic<int> failStage{0},cleanupFail{0},failAfter{0},failCalls{0};
static bool badSegment=false,badDMAMemory=false,badView=false,failSync=false;
static bool fail(int n){return failStage==n && failCalls++>=failAfter;}
class OSObject {
 std::atomic<unsigned> refs{1};
public:
 OSObject(){++liveObjects;} virtual ~OSObject(){--liveObjects;}
 virtual bool init(){return true;}
 template<class...T>bool setProperty(T...){outside();return true;}
 void retain(){assert(refs.fetch_add(1)>0);}
 void release(){assert(refs.load()>0);if(refs.fetch_sub(1,std::memory_order_acq_rel)==1)free();}
 virtual void free(){delete this;}
};
using IOService=OSObject;
class IOMapper:public OSObject{};
#define OSDeclareDefaultStructors(C)
#define OSDefineMetaClassAndStructors(C,B)
struct Cluster {char*data;int pins=0;};
static std::mutex clustersLock;
static std::unordered_map<void*,Cluster*>clusters;
static int mbuf_alloccluster(int,size_t*n,char**p){
 outside();if(fail(1))return 1;void*q=nullptr;assert(!posix_memalign(&q,4096,*n));
 auto*c=new Cluster;c->data=static_cast<char*>(q);
 std::lock_guard<std::mutex>g(clustersLock);clusters[q]=c;*p=c->data;++liveClusters;return 0;
}
static void mbuf_freecluster(char*p,size_t n){
 assert(!simpleLocks);assert(n==4096);std::lock_guard<std::mutex>g(clustersLock);auto*c=clusters.at(p);assert(c->pins==0);
 clusters.erase(p);free(p);delete c;--liveClusters;
}
struct Lease{std::atomic<unsigned>refs{1};char*bytes;void(*callback)(char*,unsigned,char*);char*arg;std::atomic<bool>sticky{false};};
struct Mbuf{Lease*l;char*data;size_t len=0,pklen=0;Mbuf*next=nullptr;unsigned flags=MBUF_EXT|MBUF_PKTHDR;};
using mbuf_t=Mbuf*;
static int mbuf_attachcluster(int how,int,mbuf_t*out,char*p,void(*cb)(char*,unsigned,char*),size_t n,char*arg){
 assert(how==MBUF_DONTWAIT&&*out==nullptr&&n==4096);if(fail(8))return 1;
 auto*l=new Lease;l->bytes=p;l->callback=cb;l->arg=arg;auto*m=new Mbuf;m->l=l;m->data=p;*out=m;++livePackets;
 if(badView)++m->data;return 0;
}
static void mbuf_freem(mbuf_t m){
 if(!m)return;auto*l=m->l;
 if(l->refs.fetch_sub(1,std::memory_order_acq_rel)==1){
  ++callbacks;l->callback(l->bytes,4096,l->arg);delete l;
 }
 delete m;--livePackets;
}
static mbuf_t clone(mbuf_t m){auto*n=new Mbuf(*m);n->l->refs.fetch_add(1);n->l->sticky=true;++livePackets;return n;}
static int mbuf_mclhasreference(mbuf_t m){return m->l->sticky.load();} // Deliberately sticky, as in XNU.
static void*mbuf_data(mbuf_t m){return m->data;}
static size_t mbuf_len(mbuf_t m){return m->len;}
static mbuf_t mbuf_next(mbuf_t m){return m->next;}
static unsigned mbuf_flags(mbuf_t m){return m->flags;}
static void mbuf_setlen(mbuf_t m,size_t n){m->len=n;}
static void mbuf_pkthdr_setlen(mbuf_t m,size_t n){m->pklen=n;}
static void*IOMalloc(size_t n){return calloc(1,n);}
static void IOFree(void*p,size_t){free(p);}
struct IOMemoryDescriptor{
 Cluster*c;bool prepared=false,owns=false;
 static IOMemoryDescriptor*withAddressRange(uintptr_t p,size_t n,int,void*t){
  outside();assert(n==4096&&t==kernel_task);if(fail(2))return nullptr;
  auto*d=new IOMemoryDescriptor;std::lock_guard<std::mutex>g(clustersLock);d->c=clusters.at(reinterpret_cast<void*>(p));++liveMD;return d;
 }
 int prepare(){outside();if(fail(3))return 1;assert(!prepared);prepared=true;++c->pins;return 0;}
 int complete(){outside();assert(prepared);if(cleanupFail==3)return 1;prepared=false;--c->pins;return 0;}
 void release(){outside();assert(!prepared);if(owns)mbuf_freecluster(c->data,4096);--liveMD;delete this;}
};
struct IODMACommand{
 enum{kMapped=0};struct Segment64{uint64_t fIOVMAddr,fLength;};
 IOMemoryDescriptor*md=nullptr;bool prepared=false;
 static IODMACommand*withSpecification(int,int bits,int,int map,int,int align,IOMapper* mapper=nullptr){outside();assert(bits==64&&map==kMapped&&align==4096);if(fail(4))return nullptr;++liveCMD;return new IODMACommand;}
 int setMemoryDescriptor(IOMemoryDescriptor*m,bool aut){outside();assert(!aut);if(fail(5))return 1;md=m;return 0;}
 int prepare(){outside();if(fail(6))return 1;assert(md&&!prepared);prepared=true;++prepares;return 0;}
 int gen64IOVMSegments(uint64_t*o,Segment64*s,uint32_t*c){outside();assert(prepared);if(fail(7))return 1;*o=4096;*c=1;s->fLength=4096;s->fIOVMAddr=reinterpret_cast<uintptr_t>(md->c->data)+0x100000000000ULL+(badSegment?1:0);return 0;}
 IOMemoryDescriptor*getIOMemoryDescriptor(){return badDMAMemory?nullptr:md;}
 int synchronize(int dir){assert(prepared);if(failSync)return 1;if(dir==1)++syncIns;return 0;}
 int complete(bool,bool sync){outside();assert(prepared);if(cleanupFail==1)return 1;prepared=false;++completes;if(sync)++lateCopies;return 0;}
 int clearMemoryDescriptor(bool aut){outside();assert(!aut&&!prepared);if(cleanupFail==2)return 1;md=nullptr;return 0;}
 void release(){assert(!prepared);--liveCMD;delete this;}
};

struct IOSimpleLock{std::mutex mutex;};
static IOSimpleLock*IOSimpleLockAlloc(){return new IOSimpleLock;}
static void IOSimpleLockFree(IOSimpleLock*p){delete p;}
static void IOSimpleLockLock(IOSimpleLock*p){p->mutex.lock();++simpleLocks;}
static void IOSimpleLockUnlock(IOSimpleLock*p){--simpleLocks;p->mutex.unlock();}
using thread_call_param_t=void*;
struct Call{void(*fn)(void*,void*);void*arg;bool pending=false,running=false,freed=false;};
using thread_call_t=Call*;
constexpr int THREAD_CALL_PRIORITY_KERNEL=1,THREAD_CALL_OPTIONS_ONCE=1;
static std::mutex callLock;static std::condition_variable callChanged;static std::deque<Call*>calls;
static thread_local Call*currentCall=nullptr;
static std::atomic<unsigned> liveCalls{0},delayedCalls{0};
static thread_call_t thread_call_allocate_with_options(void(*f)(void*,void*),void*a,int p,int o){
 outside();assert(p==1&&o==1);++liveCalls;return new Call{f,a};}
static bool thread_call_enter(Call*c){std::lock_guard<std::mutex>g(callLock);assert(!c->freed);bool old=c->pending;
 c->pending=true;if(!old&&!c->running)calls.push_back(c);return old;}
static void clock_interval_to_deadline(unsigned a,unsigned b,uint64_t*d){assert(a==10&&b==1000000);*d=10000000;}
static bool thread_call_enter_delayed(Call*c,uint64_t d){assert(d==10000000);++delayedCalls;return thread_call_enter(c);}
static bool cancelLocked(Call*c){bool old=c->pending;c->pending=false;calls.erase(std::remove(calls.begin(),calls.end(),c),calls.end());return old;}
static bool thread_call_cancel(Call*c){std::lock_guard<std::mutex>g(callLock);return cancelLocked(c);}
static bool thread_call_cancel_wait(Call*c){outside();std::unique_lock<std::mutex>g(callLock);assert(currentCall!=c);bool old=cancelLocked(c);
 callChanged.wait(g,[&](){return !c->running;});return old;}
static bool thread_call_free(Call*c){std::lock_guard<std::mutex>g(callLock);assert(!c->pending&&!c->freed);
 if(c->running&&currentCall!=c)return false;c->freed=true;if(!c->running){--liveCalls;delete c;}return true;}
static bool pumpOne(){Call*c=nullptr;{std::lock_guard<std::mutex>g(callLock);if(calls.empty())return false;c=calls.front();calls.pop_front();
 assert(c->pending&&!c->running&&!c->freed);c->pending=false;c->running=true;}
 bool saved=unsafeGate;unsafeGate=false;currentCall=c;c->fn(c->arg,nullptr);currentCall=nullptr;unsafeGate=saved;
 {std::lock_guard<std::mutex>g(callLock);c->running=false;if(c->freed){assert(!c->pending);--liveCalls;delete c;}
 else if(c->pending)calls.push_back(c);callChanged.notify_all();}return true;}
static void pump(unsigned limit=10000){for(unsigned n=0;n<limit&&pumpOne();++n){} }
struct WorkLoop{bool gated=false,failGate=false;
 bool inGate(){return gated;}
 int runAction(int(*f)(OSObject*,void*,void*,void*,void*),OSObject*o){if(failGate)return -1;
 bool old=gated,oldUnsafe=unsafeGate;gated=true;unsafeGate=true;int r=f(o,nullptr,nullptr,nullptr,nullptr);gated=old;unsafeGate=oldUnsafe;return r;}};
struct iwx_dma_info{IOMemoryDescriptor*buffer=nullptr;uint64_t paddr=0;void*vaddr=nullptr;size_t size=0;void*bmd=nullptr;IODMACommand*cmd=nullptr;};
struct RxData{mbuf_t m=nullptr;};
struct iwx_rx_ring{RxData data[512];unsigned cur=0;iwx_dma_info free_desc_dma,stat_dma,used_desc_dma;void*desc=nullptr,*stat=nullptr;};
using RxRing=iwx_rx_ring;struct Softc{RxRing rxq;unsigned sc_generation=0;};
struct Provider:public OSObject{unsigned publications=0;uint64_t wire[64]={},pressure[24]={},recovery[24]={};
 bool setProperty(const char*name,void*p,size_t n){assert(n==sizeof(wire)||n==sizeof(pressure)||n==160);++publications;
 if(!strcmp(name,"KGP_RX_ZC4"))memcpy(wire,p,n);
 else if(!strcmp(name,"KGP_RX_ZC4_FIRST_PRESSURE"))memcpy(pressure,p,n);
 else if(!strcmp(name,"KGP_RX_ZC4_LAST_RECOVERY"))memcpy(recovery,p,n);return true;}};
STORAGE
class ItlIwx:public OSObject{
public:
 IOMapper *fDma5Mapper=nullptr; IOService *fDma5Provider=nullptr;
 WorkLoop loop;Softc com;Provider provider;struct {Provider*pa_tag;}pci{&provider};
 uint32_t fPc1Fault=0,fPc1ResetEpoch=0,fPc1TerminalEpoch=0,fPc1Suspended=0,fPc1LastMaster=0;
 uint64_t now=1;uint64_t time2GNow(){return now;}
 WorkLoop*getMainWorkLoop(){return &loop;}
 bool dma5Mapped() const { return true; } // This inherited pool harness models mapped RX only.
 uint64_t descriptors[512]={};
 void iwx_update_rx_desc(Softc*,RxRing*,int s){descriptors[s]=fRxZcPool->entry[fRxZcSlot[s]].address;}
HEADER
};
METHODS
static void gate(ItlIwx&d,bool on){d.loop.gated=on;unsafeGate=on;}
static void empty(){assert(!livePackets&&!liveClusters&&!liveMD&&!liveCMD&&!liveCalls);assert(calls.empty());}
static void initial(ItlIwx&d){gate(d,false);assert(d.rxZcSetup());for(unsigned s=0;s<512;++s)assert(d.rxZcReplace(s));gate(d,true);assert(d.rxZcArm());}
static void verify(ItlIwx&d){auto*p=d.fRxZcPool;IOSimpleLockLock(p->lock);
 unsigned counts[7]={};std::vector<bool>seen(p->count);for(unsigned i=0;i<p->count;++i)++counts[p->entry[i].state];
 assert(counts[p->Posted]+counts[p->Complete]==512);assert(counts[p->Ready]==p->readyCount);assert(counts[p->Retired]==p->retireCount);assert(counts[p->Working]==p->working);
 for(unsigned s=0;s<512;++s){unsigned i=d.fRxZcSlot[s];assert(i<p->count&&!seen[i]);seen[i]=true;assert(d.descriptors[s]==p->entry[i].address);}
 assert(liveCMD<=4096);assert(p->cacheCount<=p->cacheLimit);IOSimpleLockUnlock(p->lock);
}
static mbuf_t receive(ItlIwx&d,unsigned s,bool success=true){auto m=d.com.rxq.data[s].m;assert(m->l->refs==1);
 std::memset(m->data,0xa5,4096);unsigned n=prepares;assert(d.rxZcSync(s,m));bool got=d.rxZcReplace(s);assert(got==success);assert(d.rxZcRepost(s));
 // Concurrent worker may prepare, but prepare/complete assert outside RX gate per thread.
 (void)n;if(!got){assert(d.com.rxq.data[s].m==m);return nullptr;}assert(d.com.rxq.data[s].m!=m);return m;}
static void terminal(ItlIwx&d){gate(d,true);++d.fPc1ResetEpoch;d.fPc1TerminalEpoch=0;d.fPc1LastMaster=1;d.fPc1Suspended=1;d.rxZcTerminal();assert(d.fRxZcTerminal);d.fPc1TerminalEpoch=d.fPc1ResetEpoch;}
static void cleanup(ItlIwx&d,bool async=false){terminal(d);gate(d,async);assert(d.rxZcFree());assert(!d.fRxZcPool&&!d.fRxZcLife);gate(d,false);pump();}
static void poweroff(KgpRxZcPool*p){ // Harness only: device power removed and worker stopped.
 assert(!p->call);assert(p->releaseResources());p->release();}
static void makeRingDMA(ItlIwx&d){gate(d,false);for(auto*x:{&d.com.rxq.free_desc_dma,&d.com.rxq.stat_dma,&d.com.rxq.used_desc_dma}){
 char*b=nullptr;size_t n=4096;assert(!mbuf_alloccluster(0,&n,&b));auto*md=IOMemoryDescriptor::withAddressRange(uintptr_t(b),4096,3,kernel_task);md->owns=true;assert(!md->prepare());
 auto*c=IODMACommand::withSpecification(0,64,0,0,0,4096);assert(!c->setMemoryDescriptor(md,false));assert(!c->prepare());x->buffer=md;x->cmd=c;x->vaddr=b;}gate(d,true);}
int main(){
 {ItlIwx d;gate(d,true);assert(!d.rxZcSetup());gate(d,false);empty();}
 for(int after:{0,7})for(int s=1;s<=8;++s){ItlIwx d;failCalls=0;failAfter=after;failStage=s;assert(!d.rxZcSetup());failStage=0;failAfter=0;assert(!d.fRxZcLife);empty();}
 for(int mode=0;mode<3;++mode){ItlIwx d;badSegment=mode==0;badDMAMemory=mode==1;badView=mode==2;assert(!d.rxZcSetup());badSegment=badDMAMemory=badView=false;empty();}
 {ItlIwx d;assert(d.rxZcSetup());assert(d.rxZcFree());pump();empty();}
 // No final callbacks for 20000 packets: supply must not depend on their return.
 {ItlIwx d;initial(d);std::vector<mbuf_t>held;for(unsigned k=0;k<20000;++k){auto*m=receive(d,k%512);if(k%7==0){auto*c=clone(m);mbuf_freem(m);m=c;}held.push_back(m);if(k%128==127)pump();if(k%997==0)verify(d);}
  assert(!d.fRxZcStarved&&d.fRxZcLife->detached>14000&&liveClusters>20000);verify(d);pump();
  assert(d.fRxZcPool->cacheCount==d.fRxZcPool->cacheLimit);
  assert(d.fRxZcPool->cacheStats[9]>0); // Full mapped cache degrades to ZC4 renewal.
  auto*p=d.fRxZcPool;
  std::thread returns([&]{for(auto m:held)mbuf_freem(m);});
  gate(d,false);for(int k=0;k<64;++k)p->shrinkReturned();returns.join();
  while(p->cacheCount>p->cacheWarm)p->shrinkReturned();
  assert(p->cacheStats[10]>0&&p->cacheCount==p->cacheWarm);
  gate(d,true);cleanup(d);empty();
 }
 // Normal returns reuse prepared storage; multibatch delayed returns still scale.
 {ItlIwx d;initial(d);std::deque<mbuf_t>held;
  for(unsigned k=0;k<100000;++k){auto*m=receive(d,k%512);held.push_back(m);if(held.size()>5000){for(unsigned j=0;j<2500;++j){mbuf_freem(held.front());held.pop_front();}}
   if(k%128==127)pump();if(k%2001==0)verify(d);}
  assert(!d.fRxZcStarved&&d.fRxZcPool->renewed>2048);for(auto m:held)mbuf_freem(m);pump();
  auto renewed=d.fRxZcPool->renewed;for(unsigned k=0;k<10000;++k){mbuf_freem(receive(d,k%512));if(k%128==127)pump();}
  assert(d.fRxZcPool->reused>9000&&d.fRxZcPool->renewed==renewed);cleanup(d);empty();
 }
 // True allocation/mapping/header pressure: drop+repost, then scheduled retry restores supply with NO additional RX.
 for(int stage=1;stage<=8;++stage){ItlIwx d;initial(d);std::vector<mbuf_t>held;for(unsigned k=0;k<1536;++k)held.push_back(receive(d,k%512));
  failCalls=0;failStage=stage;pump(1);receive(d,0,false);assert(d.fRxZcStarved==1&&delayedCalls>0&&d.fRxZcPool->retries);verify(d);
  failStage=0;pump();assert(d.fRxZcPool->readyCount==1536);mbuf_freem(receive(d,0));assert(d.fRxZcRecoveries==1);
  for(auto m:held)mbuf_freem(m);cleanup(d);empty();
 }
 // Clean teardown while stack holds original packet + clone; mappings gone, storage alive until final lease.
 for(bool async:{false,true}){mbuf_t held,copy;{ItlIwx d;initial(d);makeRingDMA(d);held=receive(d,0);copy=clone(held);cleanup(d,async);
  assert(!liveCMD&&!liveMD&&liveClusters==1);assert((unsigned char)held->data[0]==0xa5);assert(!d.rxZcArm());}
  mbuf_freem(held);assert(liveClusters==1);mbuf_freem(copy);empty();}
 // Ordinary clean OFF/ON and full teardown/reinitialization, including old stack release after new pool creation.
 {ItlIwx d;initial(d);auto*held=receive(d,0);auto*firstLife=d.fRxZcLife;firstLife->retain();
  for(int n=0;n<10;++n){terminal(d);unsigned ep=d.fPc1TerminalEpoch;d.fPc1TerminalEpoch=0;assert(!d.rxZcArm());d.fPc1TerminalEpoch=ep;++d.com.sc_generation;assert(d.rxZcArm());d.fPc1Suspended=0;mbuf_freem(receive(d,n+1));pump();}
  terminal(d);assert(d.rxZcFree());assert(!d.fRxZcPool);gate(d,false);assert(d.rxZcSetup());assert(d.fRxZcLife!=firstLife);pump();
  assert(firstLife->closeState==2);mbuf_freem(held);assert(firstLife->live==0);firstLife->release();for(unsigned s=0;s<512;++s)assert(d.rxZcReplace(s));
  gate(d,true);assert(d.rxZcArm());cleanup(d);empty();
 }
 // Real worker/RX/callback concurrency; intentionally unbounded scheduling jitter permits safe drops.
 for(unsigned round=0;round<8;++round){ItlIwx d;initial(d);std::atomic<bool>stop{false};std::mutex qlock;std::deque<mbuf_t>held;
  std::thread worker([&](){while(!stop){if(!pumpOne())std::this_thread::yield();}});
  std::thread stack([&](){for(;;){mbuf_t m=nullptr;{std::lock_guard<std::mutex>l(qlock);if(!held.empty()){m=held.front();held.pop_front();}else if(stop)break;}
    if(m)mbuf_freem(m);else std::this_thread::yield();}});
  for(unsigned k=0;k<4000;++k){auto m=d.com.rxq.data[k%512].m;assert(d.rxZcSync(k%512,m));if(d.rxZcReplace(k%512)){std::lock_guard<std::mutex>l(qlock);held.push_back(m);}assert(d.rxZcRepost(k%512));}
  terminal(d);gate(d,round%2);assert(d.rxZcFree());gate(d,false);stop=true;worker.join();stack.join();pump();empty();
 }
 // No terminal proof: stop worker, preserve all DMA-visible ring entries/resources, prohibit restart.
 for(int mode=0;mode<4;++mode){ItlIwx d;initial(d);auto*p=d.fRxZcPool;
  if(mode==0)failSync=true;if(mode==1)++d.com.sc_generation;if(mode==2){++d.fPc1ResetEpoch;d.fPc1Fault=2;}
  if(mode<3){assert(!d.rxZcSync(0,d.com.rxq.data[0].m));assert(!d.rxZcRepost(0));assert(!d.rxZcReplace(0));}
  gate(d,mode%2);unsigned n=completes;assert(!d.rxZcFree());gate(d,false);pump();assert(completes==n&&!d.fRxZcPool);assert(!d.rxZcSetup());
  failSync=false;poweroff(p);empty();
 }
 // Cleanup failures preserve the failed prepared resource rather than retrying or destroying it.
 for(int mode=1;mode<=3;++mode){ItlIwx d;initial(d);auto*p=d.fRxZcPool;terminal(d);gate(d,false);cleanupFail=mode;assert(d.rxZcFree());pump();assert(p->failedCleanup());assert(p->life->closeState==4);cleanupFail=0;poweroff(p);empty();}
 // Partial successful retirement followed by mapping pressure does not lose stack ownership.
 {ItlIwx d;initial(d);auto*m=receive(d,0);auto*c=clone(m);mbuf_freem(m);cleanup(d);mbuf_freem(c);empty();}
 // Rolling telemetry, first-pressure and last-recovery survive 120 publication budget and no hot-path text.
 {ItlIwx d;initial(d);unsigned n=d.provider.publications;for(int i=0;i<100;++i)d.rxZcReport(0);assert(d.provider.publications==n);
  for(int i=0;i<200;++i){d.now+=1000000001ULL;d.rxZcReport(0);}assert(d.fRxZcSnapshots==201);
  std::vector<mbuf_t>h;for(unsigned k=0;k<1536;++k)h.push_back(receive(d,k%512));receive(d,0,false);pump();mbuf_freem(receive(d,0));d.now+=1000000001ULL;d.rxZcReport(0);
  assert(d.provider.wire[1]==202&&d.provider.wire[19]==1&&d.provider.wire[20]==1&&d.provider.wire[21]==0);
  assert(d.provider.pressure[1]==1&&d.provider.recovery[1]==2);for(auto m:h)mbuf_freem(m);cleanup(d);empty();}
 {ItlIwx d;d.fDma5Provider=&d.provider;initial(d);uint64_t wire[40]={5};
  d.rxZcTelemetry(wire);assert(d.fRxZcPool->telemetryPending);pump();
  assert(!d.fRxZcPool->telemetryPending);
  auto*p=d.fRxZcPool;p->retain();
  std::thread reporter([&]{for(unsigned i=0;i<10000;++i)p->queueTelemetry(wire);});
  terminal(d);assert(d.rxZcFree());gate(d,false);pump();reporter.join();pump();
  p->release();empty();}
 // Bounded retained mappings: warm cache hits, saturation fallback, shrink,
 // and delayed stack release after clean close (the final callback never sees HAL).
 {ItlIwx d;initial(d);std::deque<mbuf_t>held;
  for(unsigned k=0;k<30000;++k){held.push_back(receive(d,k%512));
   if(held.size()>1400){for(unsigned n=0;n<700;++n){mbuf_freem(held.front());held.pop_front();}}
   if(k%128==127)pump();}
  auto*p=d.fRxZcPool;assert(p->cacheStats[6]>0);assert(p->cacheStats[2]<=p->cacheLimit);
  std::cout<<"cache workload: replacements="<<d.fRxZcReplaced<<" cache hits="<<p->cacheStats[6]<<" fresh="<<p->renewed<<" mapped="<<liveCMD<<std::endl;
  assert(p->cacheStats[0]-p->cacheStats[1]==unsigned(liveCMD));
  for(auto m:held)mbuf_freem(m);cleanup(d);empty();}
 assert(!liveObjects&&lateCopies==0);
 std::cout<<"PASS extracted ZC5: 130000 deterministic completions + 32000 concurrent RX attempts; 20000 held packets/no final returns; bounded maps; delayed returns; retry without RX; allocator/map/header failure stages; callback/worker/teardown races; deferred ring cleanup from gated/ungated callers; late stack release; OFF/ON + new-instance lifecycle; fail-closed uncertainty; rolling telemetry; no gated mapping or payload copy"<<std::endl;
}
'''
text=mock.replace('STORAGE',storage).replace('HEADER',header).replace('METHODS',methods)
clang='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
sdk='/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk'
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc5-host-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(text)
 cmd=[clang,'-isysroot',sdk,'-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND:',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=300)
 print('PASS:',san,flush=True)
