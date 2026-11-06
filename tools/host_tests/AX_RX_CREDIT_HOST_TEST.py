#!/usr/bin/env python3
"""Execute real qualified AX RX service versus frozen ZC28, mocked KPIs only.
Check normal hardware credit values and callback/lifetime ordering. BZ is tested
separately with its tagged ledger; no unqualified-AX/physical stability claims.
"""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2];G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip();C=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip();SDK=subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
P='itlwm/hal_iwx/ItlIwx.cpp';src=(R/P).read_text();oldsrc=subprocess.check_output([G,'show','483940988c502379fb6d3dcde885dca83f00b7aa:'+P],cwd=R,text=True)
def fn(s,name="iwx_notif_intr"):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S);a=m.index(name+'(struct');a=m.index('{',a);b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[s.rfind('void ItlIwx::',0,a):b]
new=fn(src);old=fn(oldsrc).replace('iwx_notif_intr(', 'old_notif(',1)
native=fn(src,'dexterOriginal_iwx_notif_intr').replace('dexterOriginal_iwx_notif_intr(', 'native_notif(',1)
oldnative=fn(oldsrc,'dexterOriginal_iwx_notif_intr').replace('dexterOriginal_iwx_notif_intr(', 'old_native_notif(',1)
pre=r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>
#include <iostream>
#define IWX_RX_MQ_RING_COUNT 512
#define IWX_DEVICE_FAMILY_AX210 22000
#define IWX_RFH_Q0_FRBDCB_WIDX_TRG 0x123
#define IWX_FLAG_SHUTDOWN 0x200
#define nitems(a) (sizeof(a)/sizeof((a)[0]))
#define le16toh(x) (x)
#define DPRINTFN(...) ((void)0)
#define IWX_WRITE(sc,reg,v) ((sc)->owner->write(v))
struct mbuf_list{unsigned count=0;};
#define MBUF_LIST_INITIALIZER() {}
unsigned ml_len(mbuf_list*m){return m->count;}
struct Loop{bool held=true;bool inGate(){return held;}};
struct ItlIwx;
struct iwx_rx_data{unsigned tag=0;};
struct iwx_rb_status{uint16_t closed_rb_num;};
struct iwx_softc {ItlIwx*owner;unsigned sc_generation=1,sc_flags=0,sc_device_family=IWX_DEVICE_FAMILY_AX210;struct{unsigned cur=0;uint16_t*stat;struct{unsigned size=2;void*map=nullptr;}stat_dma;iwx_rx_data data[512];}rxq;struct{ItlIwx*ic_if;}sc_ic;};
static void if_input(ItlIwx**,mbuf_list*);
struct ItlIwx{
 iwx_softc com;Loop loop;uint16_t producer=0;bool leased=true,isBz=false,fRxZcRunning=true,fault=false,fRxZcClosing=false;
 unsigned fPc1Fault=0,fPc1ResetEpoch=1,fRxZcGeneration=1,fRxZcEpoch=1,fRxZcNotif=0,fRx2GHw=0,fRxZcBatchQueued=0,fRxZcBatchMax=0;
 unsigned parseCount=0,repostCount=0,credits=0,reports=0,barriers=0,legacy=0,bz=0,inputs=0;std::vector<unsigned>values,tags;std::function<void()> onInput,onPacket;
 ItlIwx(){com.owner=this;com.rxq.stat=&producer;com.sc_ic.ic_if=this;for(unsigned i=0;i<512;++i)com.rxq.data[i].tag=i;}
 bool dma5Mapped(){return leased;}bool dma5Leased(){return leased;}bool bzDevice(){return isBz;}bool rxZcFailed(){return fault;}
 Loop*getMainWorkLoop(){return &loop;}
 void alive2FEvent(unsigned){}void dma5Report(){}void rxZcReport(unsigned){++reports;}
 void iwx_dma_queue_publish_barrier(){++barriers;__atomic_thread_fence(__ATOMIC_SEQ_CST);}
 void write(unsigned v){++credits;values.push_back(v);}
 void iwx_rx_pkt(iwx_softc*,iwx_rx_data*d,mbuf_list*m){++parseCount;tags.push_back(d->tag);++m->count;if(onPacket){auto f=onPacket;onPacket={};f();}}
 bool rxZcRepost(unsigned){++repostCount;return fRxZcRunning&&!fault&&!fPc1Fault&&fRxZcGeneration==com.sc_generation&&fRxZcEpoch==fPc1ResetEpoch;}
 void bzNotif(iwx_softc*){++bz;}
 void dexterOriginal_iwx_notif_intr(iwx_softc*s){++legacy;native_notif(s);}
 void iwx_notif_intr(iwx_softc*);void old_notif(iwx_softc*);void native_notif(iwx_softc*);void old_native_notif(iwx_softc*);
};
static void if_input(ItlIwx**p,mbuf_list*m){auto*t=*p;++t->inputs;if(t->onInput){auto f=t->onInput;t->onInput={};f();}m->count=0;}
'''
test=r'''
int main(){unsigned cases=0;
 // Exhaust the complete physical coordinate domain: the final credit value
 // and parsed slot sequence must be identical in every no-callback case.
 for(unsigned start=0;start<512;++start)for(unsigned span:{0U,1U,7U,8U,31U,32U,127U,511U}){
  ItlIwx a,b;a.com.rxq.cur=b.com.rxq.cur=start;a.producer=b.producer=(start+span)&511;
  a.old_notif(&a.com);b.iwx_notif_intr(&b.com);assert(a.values==b.values&&a.tags==b.tags&&a.parseCount==b.parseCount&&a.com.rxq.cur==b.com.rxq.cur&&a.repostCount==b.repostCount);
  assert(b.barriers==1&&b.inputs==1);++cases;
 }
 // Reproduce old stale-lifetime MMIO; new code has already published ready
 // credits and never writes after the stack callback changes the lifetime.
 for(unsigned transition=0;transition<4;++transition){ItlIwx a,b;a.producer=b.producer=64;
  auto change=[&](ItlIwx&t){assert(t.parseCount==64);if(transition==0)++t.com.sc_generation;else if(transition==1)++t.fPc1ResetEpoch;else if(transition==2)t.fRxZcClosing=true;else t.fRxZcRunning=false;};
  a.onInput=[&]{assert(a.credits==0);change(a);};b.onInput=[&]{assert(b.credits==1&&b.values[0]==56);change(b);};
  a.old_notif(&a.com);b.iwx_notif_intr(&b.com);assert(a.credits==1&&b.credits==1&&b.values[0]==56);++cases;
 }
 // Same-lifetime nested notification advances farther than the outer batch.
 // Its newer credit must survive the outer callback return unchanged.
 {ItlIwx a,b;a.producer=b.producer=64;a.onInput=[&]{a.producer=96;a.old_notif(&a.com);};b.onInput=[&]{b.producer=96;b.iwx_notif_intr(&b.com);};a.old_notif(&a.com);b.iwx_notif_intr(&b.com);assert(a.values==std::vector<unsigned>({88,56}));assert(b.values==std::vector<unsigned>({56,88}));++cases;}
 for(unsigned why=0;why<3;++why){ItlIwx b;b.producer=64;b.onPacket=[&]{if(why==0)++b.com.sc_generation;else if(why==1)++b.fPc1ResetEpoch;else b.fault=true;};b.iwx_notif_intr(&b.com);assert(b.credits==0&&b.parseCount==1);++cases;}
 {ItlIwx x;x.leased=false;x.iwx_notif_intr(&x.com);assert(x.legacy==1&&x.credits==1&&!x.parseCount);++cases;}
 {ItlIwx x;x.isBz=true;x.iwx_notif_intr(&x.com);assert(x.bz==1&&!x.credits&&!x.parseCount);++cases;}
 for(unsigned mode=0;mode<3;++mode){ItlIwx x;if(mode==0)x.fRxZcClosing=true;if(mode==1)x.fPc1Fault=1;if(mode==2)x.fault=true;x.producer=8;x.iwx_notif_intr(&x.com);assert(!x.credits&&!x.parseCount);++cases;}

 for(unsigned start=0;start<512;++start)for(unsigned span:{0U,1U,7U,8U,31U,32U,127U,511U}){ItlIwx a,b;a.leased=b.leased=false;a.com.rxq.cur=b.com.rxq.cur=start;a.producer=b.producer=(start+span)&511;a.old_native_notif(&a.com);b.native_notif(&b.com);assert(a.values==b.values&&a.tags==b.tags&&a.parseCount==b.parseCount&&a.com.rxq.cur==b.com.rxq.cur);++cases;}
 {ItlIwx a,b;a.producer=b.producer=64;a.onInput=[&]{a.producer=96;a.old_native_notif(&a.com);};b.onInput=[&]{b.producer=96;b.native_notif(&b.com);};a.old_native_notif(&a.com);b.native_notif(&b.com);assert(a.values==std::vector<unsigned>({88,56}));assert(b.values==std::vector<unsigned>({56,88}));++cases;}
 for(unsigned mode=0;mode<3;++mode){ItlIwx b;b.producer=64;b.onPacket=[&]{if(mode==0)++b.com.sc_generation;else if(mode==1)++b.fPc1ResetEpoch;else b.com.sc_flags|=IWX_FLAG_SHUTDOWN;};b.native_notif(&b.com);assert(b.credits==0&&b.parseCount==1);++cases;}
 std::cout<<"PASS "<<cases<<" actual AX RX credit differential/reentry/reset/failure cases. Mock stack callbacks are counterexamples, not established physical reachability or throughput proof.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='ax-rx-credit-')as td:
 p=Path(td);(p/'test.cpp').write_text(pre+old+oldnative+native+new+test)
 cmd=[C,'-isysroot',SDK,'-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
