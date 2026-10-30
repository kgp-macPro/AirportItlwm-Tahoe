#!/usr/bin/env python3
"""Exercise extracted ZC5 TX ownership/allocator code with explicit KPI mocks.
No model result establishes physical DMA coherency, kernel linking or throughput.
"""
from pathlib import Path
import re, subprocess, tempfile, sys
root=Path(__file__).resolve().parents[2]
cpp=(root/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
hpp=(root/'itlwm/hal_iwx/ItlIwx.hpp').read_text()
def mask(t):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),t,flags=re.S)
def extract(name):
 m=mask(cpp);x=re.search(r'(?:static )?(?:bool|void|int|IOReturn)\s+(?:ItlIwx::\s*)?'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return cpp[x.start():b]
names=['dma5Select','dma5Terminal','tx5RingReleasable','tx5Return','tx5Release','tx2IAllocate','tx2IPrepare','tx2IPublished','tx2IRetire','tx2IFree','tx2JEnter','tx2JLeave','pc1RecoverAction']
fields=hpp[hpp.index('    enum { kTx2ICount'):hpp.index('    struct Tx2JScope')]
mock=r'''
#include <cassert>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
using UInt64=uint64_t;using UInt32=uint32_t;using IOReturn=int;
constexpr int kIOReturnSuccess=0,kIOReturnError=1,kIOReturnNoMemory=2,kIOReturnBadArgument=3,kIOReturnAborted=4,kIOReturnNotReady=5;
constexpr unsigned PAGE_SIZE=4096,IWX_DEVICE_FAMILY_AX210=22500,IWX_DQA_CMD_QUEUE=4;
constexpr int kIODirectionInOut=3,kIODirectionOut=2,kIOMemoryPhysicallyContiguous=0x10,kIOMemoryHostPhysicallyContiguous=0x10000,kIOMemoryMapperNone=0x800,kIOMapInhibitCache=0x100000,kIODMACommandOutputHost64=64;
#define DMA_BIT_MASK(n) UINT64_MAX
#define MIN(a,b) std::min(a,b)
void *kernel_task=reinterpret_cast<void*>(1);
static thread_local bool gated=false;
static unsigned maps=0,retirements=0,syncs=0,liveMD=0,liveCMD=0,copyCalls=0;
static int failStage=0,cleanupFailure=0;
static bool bounce=false,badSegment=false,badCopy=false;
class OSObject {std::atomic<unsigned> refs{1};public:virtual ~OSObject(){}void retain(){++refs;}void release(){assert(refs);if(--refs==0)delete this;}};
struct WorkLoop {std::mutex mutex;bool inGate()const{return gated;}};
struct Gate {WorkLoop&w;bool old;Gate(WorkLoop&x):w(x),old(gated){assert(!old);w.mutex.lock();gated=true;}~Gate(){gated=old;w.mutex.unlock();}};
#define OSDynamicCast(T,p) dynamic_cast<T*>(p)
class OSData:public OSObject {public:uint32_t value=0;unsigned length=4;unsigned getLength(){return length;}void*getBytesNoCopy(){return &value;}};
class IOMapper:public OSObject {public:static IOMapper*gSystem;unsigned page=4096;bool deviceType=false;uint64_t getPageSize(){return page;}void*metaCast(const char*n){return !strcmp(n,deviceType?"AppleVTDDeviceMapper":"AppleVTD")?this:nullptr;}};
IOMapper*IOMapper::gSystem=nullptr;
class IOPCIDevice:public OSObject{public:OSObject *parent=nullptr,*policy=nullptr;OSObject*copyProperty(const char*n){auto*p=!strcmp(n,"iommu-parent")?parent:policy;if(p)p->retain();return p;}template<class...T>bool setProperty(T...){return true;}};
struct IOBufferMemoryDescriptor:OSObject {
 std::vector<uint8_t> bytes;bool pinned=false;int opts=0;
 IOBufferMemoryDescriptor(size_t n,int o):bytes(n),opts(o){++liveMD;}
 ~IOBufferMemoryDescriptor(){assert(!pinned);--liveMD;}
 static IOBufferMemoryDescriptor*inTaskWithPhysicalMask(void*,int opts,size_t n,uint64_t){assert(!gated);if(failStage==1)return nullptr;assert((opts&kIOMemoryMapperNone)||(opts&kIOMemoryPhysicallyContiguous));return new IOBufferMemoryDescriptor(n,opts);}
 int prepare(){assert(!gated);if(failStage==2)return 1;assert(!pinned);pinned=true;return 0;}
 int complete(){assert(!gated);if(cleanupFailure==3)return 1;assert(pinned);pinned=false;return 0;}
 void*getBytesNoCopy(){return bytes.data();}
};
struct IODMACommand:OSObject {
 enum{kMapped=1};struct Segment64 {uint64_t fIOVMAddr=0,fLength=0;};
 IOBufferMemoryDescriptor *md=nullptr;IOMapper*mapper=nullptr;bool prepared=false;uint64_t address=0;
 IODMACommand(IOMapper*m):mapper(m){if(m)m->retain();++liveCMD;}
 ~IODMACommand(){assert(!prepared&&!md);if(mapper)mapper->release();--liveCMD;}
 static IODMACommand*withSpecification(int,unsigned,int,int mode,int,unsigned,IOMapper*m){assert(!gated&&mode==kMapped);if(failStage==3)return nullptr;assert(!(reinterpret_cast<uintptr_t>(IOMapper::gSystem)&3));return new IODMACommand(m?m:IOMapper::gSystem);}
 int setMemoryDescriptor(IOBufferMemoryDescriptor*m){assert(!gated);if(failStage==4)return 1;md=m;md->retain();prepared=true;address=0x100000000ULL+(++maps)*8192;return 0;}
 int gen64IOVMSegments(UInt64*off,Segment64*s,UInt32*n){assert(!gated&&prepared);if(failStage==5)return 1;*n=1;*off=md->bytes.size();s->fLength=*off;s->fIOVMAddr=address+(badSegment?1:0);return 0;}
 IOBufferMemoryDescriptor*getIOMemoryDescriptor(){return bounce?nullptr:md;}
 int synchronize(int dir){assert(!gated&&prepared&&dir==kIODirectionOut);++syncs;return failStage==7?1:0;}
 int complete(bool=true,bool=true){assert(!gated&&prepared);if(cleanupFailure==1)return 1;prepared=false;++retirements;return 0;}
 int clearMemoryDescriptor(bool autoComplete=true){assert(!gated);if(cleanupFailure==2)return 1;if(prepared){assert(autoComplete);complete();}if(md){md->release();md=nullptr;}return 0;}
};
struct iwx_dma_info {IOBufferMemoryDescriptor*buffer=nullptr;IODMACommand*cmd=nullptr;void*vaddr=nullptr;uint64_t paddr=0;size_t size=0;};
struct Mbuf {std::vector<uint8_t> bytes;explicit Mbuf(size_t n):bytes(n,0xa5){}};using mbuf_t=Mbuf*;
size_t mbuf_pkthdr_len(mbuf_t m){return m->bytes.size();}
int mbuf_copydata(mbuf_t m,size_t off,size_t n,void*p){assert(gated);++copyCalls;if(badCopy)return 1;assert(off+n<=m->bytes.size());memcpy(p,m->bytes.data()+off,n);return 0;}
struct iwx_tx_data {uint16_t tx2I_token=0;mbuf_t m=nullptr;};
struct iwx_tx_ring {unsigned ring_count=0;iwx_tx_data*data=nullptr;};
struct IOPhysicalSegment {uint64_t location=0,length=0;};
void clock_get_uptime(uint64_t*out){*out=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());}
class IOInterruptEventSource;
class ItlIwx:public OSObject {public:
 WorkLoop gate;WorkLoop*getMainWorkLoop(){return &gate;}
 struct {IOPCIDevice*pa_tag=nullptr;}pci;
 struct {unsigned sc_device_family=IWX_DEVICE_FAMILY_AX210,sc_generation=1;struct{bool uc_ok=true;}sc_uc;iwx_tx_ring txq[32];}com;
 uint32_t fPc1Fault=0,fPc1Suspended=0,fPc1TerminalEpoch=0,fPc1ResetEpoch=0,fPc1ExpectedGeneration=1,fPc1QuarantineCount=0,fPc1LastRefusal=0,fPc1Recoveries=0,fCmd2KFrozen=0;
 bool fPc1LastMaster=false;
 void pc1Report(){}
 static IOReturn pc1RecoverAction(OSObject*,void*,void*,void*,void*);
 void tx2IFree();
 void cleanupSelection(){if(fDma5Mapper)fDma5Mapper->release();if(fDma5Provider)fDma5Provider->release();fDma5Mapper=nullptr;fDma5Provider=nullptr;}
'''
stubs=r'''
};
void ItlIwx::boot5Mark(unsigned stage,const char*,int){fBoot5Stage=stage;}
void ItlIwx::tx2IReport(unsigned){}
void ItlIwx::tx2IError(unsigned token,unsigned stage,uint32_t result){uint32_t expected=0;if(__atomic_compare_exchange_n(&fTx2IStats[16],&expected,2U,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){fTx2IStats[17]=token;fTx2IStats[18]=stage;fTx2IStats[19]=result;}}
'''
test=r'''
static void balanced(){assert(!liveCMD&&!liveMD);assert(maps==retirements);}
static void terminal(ItlIwx&d){d.fPc1Suspended=1;d.fPc1LastMaster=true;d.fPc1ResetEpoch=d.fPc1TerminalEpoch=1;d.fPc1Fault=0;}
static void select(ItlIwx&d,IOPCIDevice&p,bool mapped){static IOMapper mapper;static OSData policy;IOMapper::gSystem=mapped?&mapper:nullptr;p.policy=mapped?&policy:nullptr;p.parent=nullptr;d.pci.pa_tag=&p;assert(d.dma5Select());assert(d.dma5Mapped()==mapped);}
static void conservation(ItlIwx&d){assert(d.fTx5Telemetry[0]==d.fTx5Telemetry[2]+d.fTx5Telemetry[14]+d.fTx5Telemetry[4]);}
static void send(ItlIwx&d,iwx_tx_data&slot,Mbuf&m,unsigned q=0){IOPhysicalSegment seg[32]={};assert(d.tx2JEnter());int n=d.tx2IPrepare(&slot,&m,q,0,seg);assert(n>0&&n<=4);auto&b=d.fTx2I[slot.tx2I_token-1];assert(!memcmp(b.dma.vaddr,m.bytes.data(),m.bytes.size()));uint64_t len=0;for(int i=0;i<n;++i){assert(seg[i].location==b.dma.paddr+len);assert(seg[i].length<=4092);len+=seg[i].length;}assert(len==m.bytes.size());d.tx2IPublished(&slot);d.tx2JLeave();}
int main(){
 IOPCIDevice provider;
 {ItlIwx d;select(d,provider,false);assert(d.tx2IAllocate());assert(!liveMD&&!liveCMD);terminal(d);d.fPc1ExpectedGeneration=d.com.sc_generation;assert(d.pc1RecoverAction(&d,reinterpret_cast<void*>(uintptr_t(d.com.sc_generation)),nullptr,nullptr,nullptr)==0);Gate g(d.gate);assert(d.tx2JEnter());d.tx2JLeave();iwx_tx_data x;Mbuf m(1);IOPhysicalSegment s[8];assert(!d.tx2IPrepare(&x,&m,0,0,s));d.cleanupSelection();}
 // Pending global states, opaque device requirements and incomplete PCI policy fail closed.
 for(uintptr_t state:{1U,2U,3U}){IOMapper::gSystem=reinterpret_cast<IOMapper*>(state);ItlIwx d;d.pci.pa_tag=&provider;assert(!d.dma5Select()&&!d.fDma5Selected);}
 {OSData opaque;IOMapper::gSystem=nullptr;provider.parent=&opaque;ItlIwx d;d.pci.pa_tag=&provider;assert(!d.dma5Select());provider.parent=nullptr;}
 {IOMapper system;OSData policy;IOMapper::gSystem=&system;provider.policy=nullptr;ItlIwx d;d.pci.pa_tag=&provider;assert(!d.dma5Select());provider.policy=&policy;policy.length=3;assert(!d.dma5Select());policy.length=4;policy.value=32;assert(!d.dma5Select());policy.value=2;assert(!d.dma5Select());policy.value=0;assert(d.dma5Select()&&d.dma5Mapped()&&!d.fDma5Mapper);d.cleanupSelection();provider.policy=nullptr;}
 {IOMapper system,device;OSData policy;device.deviceType=true;policy.value=3;IOMapper::gSystem=&system;provider.policy=&policy;provider.parent=&device;ItlIwx d;d.pci.pa_tag=&provider;assert(d.dma5Select()&&d.dma5Mapped()&&d.fDma5Mapper==&device);d.cleanupSelection();provider.parent=provider.policy=nullptr;}

 for(int fail=1;fail<=7;++fail){if(fail==6)continue;failStage=fail;ItlIwx d;select(d,provider,true);assert(!d.tx2IAllocate());assert(d.dma5Mapped());d.tx2IFree();d.cleanupSelection();balanced();}failStage=0;
 for(bool effective:{false,true}){ItlIwx d;select(d,provider,true);bounce=effective;badSegment=!effective;assert(!d.tx2IAllocate());d.tx2IFree();d.cleanupSelection();balanced();}bounce=badSegment=false;
 {
  ItlIwx d;select(d,provider,true);assert(d.tx2IAllocate());unsigned startMaps=maps,startSync=syncs;Mbuf m(1500);
  std::vector<iwx_tx_data>slots(1024);
  {Gate g(d.gate);for(unsigned i=0;i<1024;++i)send(d,slots[i],m,i%8);assert(!d.fTx5FreeCount);iwx_tx_data full;IOPhysicalSegment s[8];assert(!d.tx2IPrepare(&full,&m,1,1,s));assert(!d.fTx2IBlocked);assert(d.fTx5Telemetry[6]==1);}
  for(unsigned i=0;i<100000;++i){Gate g(d.gate);unsigned j=i%1024;d.tx2IRetire(&slots[j],true);send(d,slots[j],m,i%8);conservation(d);}
  {Gate g(d.gate);for(auto&x:slots)d.tx2IRetire(&x,true);assert(d.fTx5FreeCount==1024);conservation(d);assert(d.fTx5Telemetry[5]==1024);assert(d.fTx5Telemetry[4]==0);}
  assert(maps==startMaps&&syncs==startSync);assert(d.fTx5Telemetry[10]==1024);assert(d.fTx5Telemetry[9]==d.fTx5Telemetry[1]);
  // Independent producer/completion clients serialize exactly at the driver gate.
  std::vector<std::thread>workers;for(unsigned q=0;q<8;++q)workers.emplace_back([&,q]{iwx_tx_data s;for(unsigned j=0;j<3000;++j){Gate g(d.gate);send(d,s,m,q);d.tx2IRetire(&s,true);}});for(auto&t:workers)t.join();conservation(d);
  {Gate g(d.gate);iwx_tx_data s;Mbuf large(8192);send(d,s,large);d.tx2IRetire(&s,true);badCopy=true;IOPhysicalSegment seg[8];assert(!d.tx2IPrepare(&s,&m,0,0,seg));badCopy=false;assert(d.fTx5FreeCount==1024);}
  d.tx2IFree();d.cleanupSelection();balanced();
 }
 {
  ItlIwx d;select(d,provider,true);assert(d.tx2IAllocate());d.fDma5Published=true;Mbuf m(800);std::vector<iwx_tx_data>slots(64);
  {Gate g(d.gate);for(auto&s:slots)send(d,s,m);}
  // Reset/normal completion contention: one token winner, no duplicate free-list.
  std::thread complete([&]{for(auto&s:slots){Gate g(d.gate);d.tx2IRetire(&s,true);}});
  std::thread reset([&]{for(auto&s:slots){Gate g(d.gate);d.tx2IRetire(&s,false);}});complete.join();reset.join();conservation(d);
  d.fPc1QuarantineCount=0;for(auto&b:d.fTx2I)d.fPc1QuarantineCount+=b.state==d.Tx2IQuarantined;
  terminal(d);d.fPc1ExpectedGeneration=2;d.com.sc_generation=1;
  assert(d.pc1RecoverAction(&d,reinterpret_cast<void*>(1),nullptr,nullptr,nullptr)!=0);
  d.com.sc_generation=2;assert(d.pc1RecoverAction(&d,reinterpret_cast<void*>(2),nullptr,nullptr,nullptr)==0);
  assert(d.fTx5FreeCount==1024-d.fPc1QuarantineCount); // Checked recovery does not recycle old quarantine.
  {Gate g(d.gate);iwx_tx_data s;send(d,s,m);d.tx2IRetire(&s,false);}conservation(d);
  d.fPc1Fault=2;d.tx2IFree();assert(liveCMD>0); // Uncertain lifetime retained.
  // Simulated test-rig hardware terminal, never a driver timeout assumption.
  terminal(d);d.tx2IFree();d.cleanupSelection();balanced();
 }
 {ItlIwx d;select(d,provider,false);d.fDma5Published=true;iwx_tx_data s;Mbuf m(5);s.m=&m;iwx_tx_ring r{1,&s};assert(!d.tx5RingReleasable(&r));terminal(d);assert(d.tx5RingReleasable(&r));d.fPc1Fault=2;assert(!d.tx5RingReleasable(&r));d.cleanupSelection();}
 for(int stage=1;stage<=3;++stage){
  ItlIwx d;select(d,provider,true);assert(d.tx2IAllocate());cleanupFailure=stage;d.tx2IFree();assert(liveMD>0);assert(d.fTx5Telemetry[22]==1024);cleanupFailure=0;
  // Harness cleanup of deliberately retained SDK objects after their failure.
  for(auto&b:d.fTx2I){if(b.dma.cmd){if(b.dma.cmd->prepared)b.dma.cmd->complete();b.dma.cmd->clearMemoryDescriptor(false);b.dma.cmd->release();}if(b.dma.buffer){if(b.dma.buffer->pinned)b.dma.buffer->complete();b.dma.buffer->release();}}
  d.cleanupSelection();balanced();
 }
 std::cout<<"PASS extracted TX ZC5: 100000 delayed-turnover + 24000 concurrent queue submissions; mapped/conventional routing; no hot-path prepare/sync; positive completion; reset/generation quarantine; clean/uncertain/error teardown; conservation\n";
}
'''
code=mock+fields+stubs+extract('allocatePacketBacking2H')+'\n'+'\n'.join(extract(n) for n in names)+test
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc5-tx-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fno-omit-frame-pointer','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('HOST COMMAND:', ' '.join(cmd),flush=True)
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
