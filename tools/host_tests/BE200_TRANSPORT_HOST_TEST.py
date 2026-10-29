#!/usr/bin/env python3
"""Execute production BZ contracts + actual transport methods with mocked DMA/MMIO.
No firmware, actual IOKit mapper, cache attributes or physical Wi-Fi are simulated.
"""
from pathlib import Path
import re,subprocess,tempfile,sys,json
R=Path(__file__).resolve().parents[2]
s=(R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text()
def fn(n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:void|bool|int)\s+ItlIwx::'+n+r'\(',m);assert x,n
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <iostream>
#include <cerrno>
#include "itlwm/hal_iwx/ItlIwxBzContracts.hpp"
using IOReturn=int;constexpr int kIOReturnSuccess=0,kIODirectionOut=2,kIODirectionIn=1,IWX_RBUF_SIZE=4096;
constexpr unsigned IWX_CSR_GP_CNTRL=0,IWX_CSR_GP_CNTRL_REG_FLAG_BUS_MASTER_DISABLE_REQ=1U<<29;
constexpr unsigned IWX_HBUS_TARG_WRPTR=0x460;
#define IWX_HBUS_TARG_WRPTR_RX_Q(q) (0x80000000U | ((q)<<16))
#define IWX_READ(sc,reg) (this->mmioRead())
#define IWX_WRITE(sc,reg,v) (this->mmioWrite(reg,v))
#define IWX_SETBITS(sc,reg,v) (this->requested=true)
#define DELAY(x) ((void)0)
#define htole16(x) (x)
#define htole64(x) (x)
#define le16toh(x) (x)
struct Command {void*backing=nullptr;int status=0;unsigned syncs=0;void*getIOMemoryDescriptor(){return backing;}int synchronize(int){++syncs;return status;}};
struct DMA {void*vaddr=nullptr;size_t size=0;Command*cmd=nullptr;};
struct iwx_rx_transfer_desc {uint16_t rbid,reserved[3];uint64_t addr;};
struct Data {unsigned slot=0;};
struct iwx_rx_ring {DMA free_desc_dma,used_desc_dma,stat_dma;unsigned cur=0,write=0;void*stat=nullptr;Data data[512];};
struct mbuf_list {unsigned count=0;};
#define MBUF_LIST_INITIALIZER() {}
struct iwx_softc {uint32_t sc_fh_mask=~0U,sc_hw_mask=~0U,sc_flags=0;unsigned init_task=0;unsigned sc_generation=1;iwx_rx_ring rxq;struct{unsigned deliveries=0;}sc_ic;};
struct KgpRxZcPool {enum State {Posted,Complete};struct Entry {State state=Posted;Command*command=nullptr;void*memory=nullptr;uint64_t address=0;}entry[512];};
void *systq=nullptr;static void task_add(void*,unsigned*p){++*p;}
static std::function<void()> onInput;
static void if_input(void *p,mbuf_list*m){if(onInput){auto f=onInput;onInput={};f();}m->count=0;}
struct Loop {bool held=true;bool inGate(){return held;}};
struct ItlIwx {
 iwx_softc com;Loop loop;uint32_t fBzFault=0,fBzError=0,fPc1Fault=0,fPc1ResetEpoch=1;
 bool fBzHardwareOwned=false,fPc1LastMaster=false,fPc1Suspended=false,fRxZcRunning=true;
 unsigned fPc1TerminalEpoch=0,fRxZcGeneration=1,fRxZcEpoch=1;
 static constexpr unsigned kRxZcCount=512;
 unsigned fRxZcSlot[512]={};KgpRxZcPool pool;KgpRxZcPool*fRxZcPool=&pool;
 kgp_bz::RxLedger fBzRx;uint64_t fBzStats[32]={};unsigned accesses=0,doorbells=0,writes=0,rearms=0,rfkills=0;
 uint32_t lastDoorbell=0;bool requested=false,replace=true;std::vector<uint32_t> reads;size_t readIndex=0;
 iwx_rx_transfer_desc descriptors[512]={};kgp_bz::Completion completions[512]={};uint16_t producer=0;
 Command commands[515];std::vector<unsigned> received;
 Loop*getMainWorkLoop(){return &loop;}
 void bzFail(uint32_t reason,int e){if(!fBzFault){fBzFault=reason;fBzError=e;}fBzRx.close();}
 uint32_t mmioRead(){return reads.empty()?0:reads[readIndex<reads.size()?readIndex++:reads.size()-1];}
 void mmioWrite(unsigned r,uint32_t v){++writes;if(r==IWX_HBUS_TARG_WRPTR){++doorbells;lastDoorbell=v;}else if(r==IWX_CSR_MSIX_AUTOMASK_ST_AD){assert(v==1);++rearms;}}
 void iwx_check_rfkill(iwx_softc*){++rfkills;}
 int bzInterrupt(iwx_softc*);
 static void iwx_dma_queue_publish_barrier(){__atomic_thread_fence(__ATOMIC_SEQ_CST);}
 void iwx_rx_pkt(iwx_softc*,Data*d,mbuf_list*m){++accesses;received.push_back(d->slot);++m->count;}
 bool rxZcRepost(unsigned){return replace;}
 bool bzMasterStop(iwx_softc*);void bzResetRx(iwx_softc*);void bzUpdateRxDesc(iwx_softc*,iwx_rx_ring*,unsigned,unsigned);
 void bzPublishRx(iwx_softc*);void bzNotif(iwx_softc*);
 ItlIwx(){auto&r=com.rxq;r.free_desc_dma={descriptors,sizeof(descriptors),&commands[512]};
  r.used_desc_dma={completions,sizeof(completions),&commands[513]};r.stat_dma={&producer,sizeof(producer),&commands[514]};r.stat=&producer;
  for(unsigned i=0;i<512;++i){r.data[i].slot=i;fRxZcSlot[i]=i;commands[i].backing=&commands[i];pool.entry[i]={KgpRxZcPool::Posted,&commands[i],&commands[i],uint64_t(0x100000+i*4096)};}
 }
 void initial(){bzResetRx(&com);for(unsigned i=0;i<511;++i)bzUpdateRxDesc(&com,&com.rxq,i,i);assert(!fBzFault);
  fBzHardwareOwned=true;bzPublishRx(&com);assert(!fBzFault&&fBzRx.published==504);}
};
'''
pre=pre.replace('struct{unsigned deliveries=0;}sc_ic;','struct{unsigned ic_if=0;}sc_ic;')

reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()+(R/'itlwm/hal_iwx/if_iwxvar.h').read_text()
irq=fn('bzInterrupt');defs=''
for n in sorted((set(re.findall(r'\bIWX_[A-Z0-9_]+',irq)) | {'IWX_CSR_MSIX_BASE'}) - {'IWX_READ','IWX_WRITE'}):
 x=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',reg,re.M) or re.search(r'\b'+n+r'\s*=\s*([^,\n]+)',reg);assert x,n
 defs+='#define '+n+' '+x[1]+'\n'
pre=pre.replace('using IOReturn',defs+'\nusing IOReturn',1)

test=r'''
int main(){unsigned tests=0;
 assert(kgp_bz::deviceAllowed(0x8086,0x272b,0x47,2,0x112,2));
 for(unsigned x:{0x7740U,0x2725U,0x2723U})assert(!kgp_bz::deviceAllowed(0x8086,x,0x47,2,0x112,2));
 assert(!kgp_bz::deviceAllowed(0x8086,0x272b,0x46,2,0x112,2));tests+=5;
 assert(kgp_bz::msixAccepted(0,0,true,true));assert(!kgp_bz::msixAccepted(1,0,true,true));
 assert(!kgp_bz::msixAccepted(0,-1,true,true));assert(!kgp_bz::msixAccepted(0,0,false,true));assert(!kgp_bz::msixAccepted(0,0,true,false));tests+=5;
 assert(kgp_bz::dmaFits(0x1000,4096,4096));assert(!kgp_bz::dmaFits(0xfffff000,4096,4096));
 assert(!kgp_bz::dmaFits(UINT64_MAX-15,32,1));assert(!kgp_bz::dmaFits(0x1001,4096,4096));tests+=4;
 for(unsigned v:{0U,2U,99U,255U})assert(!kgp_bz::version(v,1));assert(kgp_bz::version(99,1,true));assert(!kgp_bz::version(99,2,true));tests+=6;
 {uint8_t b[12]={1,0,0,0,3,0,0,0,1,2,3,0};assert(kgp_bz::validTlvs(b,12));for(unsigned n=1;n<12;++n)assert(!kgp_bz::validTlvs(b,n));
 b[4]=255;assert(!kgp_bz::validTlvs(b,12));assert(!kgp_bz::validTlvs(nullptr,0));tests+=14;}
 {ItlIwx t;t.reads={0,0,1U<<28};assert(t.bzMasterStop(&t.com)&&t.requested&&t.readIndex==3);assert(!t.fBzFault);++tests;}
 for(uint32_t bad:{0U,0xffffffffU,0xa5a5a5a5U}){ItlIwx t;t.reads={bad};t.fBzHardwareOwned=true;assert(!t.bzMasterStop(&t.com));assert(t.fBzHardwareOwned&&t.fBzFault);++tests;}
 {ItlIwx t;t.initial();assert(t.descriptors[0].rbid==1&&t.descriptors[510].rbid==511&&t.descriptors[511].rbid==0);++tests;
  unsigned order[]={90,3,401,40,7,10,21,19};for(unsigned j=0;j<8;++j)t.completions[j]={uint16_t(order[j]+1),0,0};
  t.producer=8;t.bzNotif(&t.com);assert(!t.fBzFault&&t.accesses==8&&t.com.rxq.cur==8&&t.fBzRx.write==7);
  assert(t.lastDoorbell==IWX_HBUS_TARG_WRPTR_RX_Q(0));for(unsigned j=0;j<8;++j)assert(t.received[j]==order[j]);++tests;
 }
 for(unsigned bad:{0U,513U,65535U,512U}){ItlIwx t;t.initial();t.completions[0]={uint16_t(bad),0,0};t.producer=1;t.bzNotif(&t.com);
  assert(t.fBzFault&&t.accesses==0&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();t.completions[0]={10,0,0};t.completions[1]={10,0,0};t.producer=2;t.bzNotif(&t.com);assert(t.fBzFault&&t.accesses==1&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();t.completions[0]={10,1,0};t.producer=1;t.bzNotif(&t.com);assert(t.fBzFault&&t.accesses==0&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();t.fPc1ResetEpoch++;t.completions[0]={1,0,0};t.producer=1;t.bzNotif(&t.com);assert(!t.accesses&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();t.commands[513].status=EIO;t.producer=1;t.bzNotif(&t.com);assert(t.fBzFault&&!t.accesses&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();t.replace=false;t.completions[0]={1,0,0};t.producer=1;t.bzNotif(&t.com);assert(t.fBzFault&&t.accesses==1&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();t.completions[0]={1,0,0};t.producer=1;onInput=[&]{++t.fPc1ResetEpoch;t.fBzRx.close();};
  t.bzNotif(&t.com);assert(t.accesses==1&&t.doorbells==1);++tests;}
 {ItlIwx t;t.initial();auto cookie=t.fBzRx.begin(1,1);assert(cookie);t.bzNotif(&t.com);assert(!t.accesses);t.fBzRx.end(cookie);
  t.bzResetRx(&t.com);assert(t.fBzFault&&t.fBzHardwareOwned);++tests;}
 {kgp_bz::RxLedger l;assert(l.reset(1,1,false));for(unsigned i=0;i<511;++i)assert(l.prepare(i,1,1));unsigned db;
  assert(l.publish(1,1,&db)&&db==504);assert(!l.reset(2,2,false));assert(l.reset(2,2,true));
  assert(!l.claim(1,1,1,&db));auto a=l.begin(2,2);assert(a);l.close();assert(l.reset(3,3,true));auto b=l.begin(3,3);l.end(a);assert(l.servicing);l.end(b);assert(!l.servicing);++tests;}
 {kgp_bz::RxLedger l;assert(l.reset(1,1,false));for(unsigned i=0;i<511;++i)assert(l.prepare(i,1,1));unsigned db,idx;
  assert(l.publish(1,1,&db));for(unsigned batch=0;batch<5000;++batch){unsigned start=(batch*8)%504;
   for(unsigned j=0;j<8;++j){unsigned id=(start+j)%504;assert(l.claim(id+1,1,1,&idx)&&idx==id);assert(l.prepare(idx,1,1));}
   assert(l.publish(1,1,&db));}++tests;}

 {kgp_bz::CommandLedger l;auto a=l.begin(0,0x309,1,2,false);assert(a&&!l.available(0));
  assert(!l.complete(0,0x308,1,2));assert(!l.complete(0,0x309,2,2));
  assert(l.complete(0,0x309,1,2));assert(!l.available(0));assert(l.completed(0,a));
  assert(!l.complete(0,0x309,1,2));l.finish(0,a,true);assert(l.available(0));++tests;
  auto b=l.begin(0,0x518,1,2,true);assert(b!=a);l.finish(0,a,false);assert(!l.e[0].uncertain);
  assert(l.complete(0,0x518,1,2)&&l.available(0));++tests;
  auto c=l.begin(1,0x517,1,2,false);l.finish(1,c,false);assert(!l.available(1));assert(!l.reset(false));
  assert(!l.complete(1,0x517,1,2));assert(l.reset(true));assert(l.available(1));
  auto d=l.begin(1,0x517,2,3,false);l.finish(1,c,true);assert(!l.available(1));assert(!l.completed(1,c));
  assert(l.complete(1,0x517,2,3));l.finish(1,d,true);assert(l.available(1));++tests;
 }

 {ItlIwx t;t.initial();t.completions[0]={1,0,0};t.producer=1;
  onInput=[&]{t.fBzHardwareOwned=false;t.fPc1LastMaster=true;t.fPc1Suspended=true;
   ++t.com.sc_generation;++t.fPc1ResetEpoch;t.fRxZcGeneration=t.com.sc_generation;t.fRxZcEpoch=t.fPc1ResetEpoch;
   t.bzResetRx(&t.com);for(unsigned i=0;i<511;++i)t.bzUpdateRxDesc(&t.com,&t.com.rxq,i,i);
   t.fBzHardwareOwned=true;t.bzPublishRx(&t.com);};
  t.bzNotif(&t.com);assert(!t.fBzFault&&t.doorbells==2);assert(t.com.rxq.cur==0);++tests;
 }

 {ItlIwx t;t.bzResetRx(&t.com);for(unsigned i=0;i<511;++i)t.bzUpdateRxDesc(&t.com,&t.com.rxq,i,i);t.fBzHardwareOwned=true;
  t.completions[0]={1,0,0};t.producer=1;t.reads={IWX_MSIX_FH_INT_CAUSES_Q0,IWX_MSIX_HW_INT_CAUSES_REG_ALIVE};
  assert(t.bzInterrupt(&t.com)==1);assert(!t.fBzFault&&t.accesses==1&&t.doorbells==2&&t.rearms==1);++tests;}
 {ItlIwx t;t.initial();t.reads={IWX_MSIX_FH_INT_CAUSES_Q0,IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR_V2|IWX_MSIX_HW_INT_CAUSES_REG_ALIVE};
  t.completions[0]={1,0,0};t.producer=1;t.bzInterrupt(&t.com);assert(t.fBzFault&&!t.accesses&&t.doorbells==1&&!t.rearms&&t.com.init_task==1);++tests;}
 {ItlIwx t;t.reads={0,IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL};t.bzInterrupt(&t.com);assert(t.rfkills==1&&t.com.init_task==1&&t.rearms==1);++tests;}
 {ItlIwx t;t.reads={0xffffffffU,0};t.bzInterrupt(&t.com);assert(t.fBzFault&&!t.rearms&&!t.accesses);++tests;}
 {ItlIwx t;t.fPc1Fault=1;t.bzInterrupt(&t.com);assert(t.readIndex==0&&!t.rearms&&!t.accesses);++tests;}
 std::cout<<"PASS "<<tests<<" production-contract/transport scenarios; 8 nonsequential hardware IDs plus 40000 wrapped ledger returns. Mock DMA/MMIO; NOT hardware qualification.\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-transport-') as d:
 p=Path(d);source=pre+'\n'.join(fn(n) for n in ['bzMasterStop','bzResetRx','bzUpdateRxDesc','bzPublishRx','bzNotif','bzInterrupt'])+test
 (p/'test.cpp').write_text(source)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+san,'-I'+str(R),str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
