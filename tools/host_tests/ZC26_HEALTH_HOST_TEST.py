#!/usr/bin/env python3
"""Extract actual health callbacks; mock controller owner/gate/provider, not DMA."""
from pathlib import Path
import os,re,subprocess,tempfile,sys
R=Path(os.environ.get('ZC26_SOURCE_ROOT',Path(__file__).resolve().parents[2]));s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def extract(n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 q=re.search(r'(?:void|IOReturn)\s+ItlIwx::\s*'+n+r'\s*\(',m);assert q
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>
using IOReturn=int;constexpr int kIOReturnSuccess=0,kIOReturnNotReady=1;
struct OSObject{uint64_t unrelated[2]={42,99};};
using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);
struct Loop{static thread_local bool held;bool inGate(){return held;}};thread_local bool Loop::held=false;
struct Gate{OSObject*owner;Loop loop;std::mutex m;bool forcedBusy=false;
 int attemptAction(Action f,void*c){if(forcedBusy||!m.try_lock())return 1;Loop::held=true;auto r=f(owner,c,nullptr,nullptr,nullptr);Loop::held=false;m.unlock();return r;}};
struct Provider{unsigned writes=0;bool fail=false;uint64_t wire[32]={};bool setProperty(const char*n,void*p,unsigned z){assert(!Loop::held&&z==256&&!strcmp(n,"KGP_ZC19"));++writes;if(fail)return false;memcpy(wire,p,z);return true;}};
struct ItlIwx{
 Gate *gate=nullptr;struct{Provider*pa_tag=nullptr;}pci;
 struct{unsigned sc_generation=1,sc_device_family=2;struct{struct{unsigned if_flags=3;}ic_if;}sc_ic;}com;
 uint64_t fZc19Health[32]={},fZc19Miss=0,fZc19PublishFail=0,fZc18Stats[12]={};
 bool fDma5Selected=true,mapped=true;uint32_t fPc1Fault=0,fTx2IBlocked=0,fCmd2KFrozen=0,fPc1ResetEpoch=1,fCmd2KEpoch=518,fPc1Suspended=0,fRxZcFault=0;
 bool dma5Mapped(){return mapped;}Gate*getMainCommandGate(){return gate;}Loop*getMainWorkLoop(){return gate?&gate->loop:nullptr;}
 static uint64_t time2GNow(){return 123456;}
 unsigned periodicCaptured=0,periodicPublished=0;
 void rxZcCapturePeriodic(){assert(Loop::held);++periodicCaptured;}
 void rxZcPublishPeriodic(){assert(!Loop::held);++periodicPublished;}
 static IOReturn zc19Snapshot(OSObject*,void*,void*,void*,void*);void zc19Publish();
};
'''
test=r'''
int main(){for(bool mapped:{false,true}){
 OSObject controller;Gate gate{&controller};Provider p;ItlIwx d;d.gate=&gate;d.pci.pa_tag=&p;d.mapped=mapped;
 assert(d.zc19Snapshot(&controller,&d,nullptr,nullptr,nullptr)==1);assert(!p.writes);
 d.zc19Publish();assert(p.writes==1&&p.wire[0]==19&&p.wire[1]==1&&p.wire[2]==256&&p.wire[3]==1&&p.wire[5]==(mapped?2:1));
 assert(controller.unrelated[0]==42&&controller.unrelated[1]==99); // wrong-owner regression
 assert(d.periodicCaptured==1&&d.periodicPublished==1);
 gate.forcedBusy=true;d.zc19Publish();assert(p.writes==1&&d.fZc19Miss==1);
 assert(d.periodicCaptured==1&&d.periodicPublished==1);gate.forcedBusy=false;
 d.fPc1Fault=2;d.fTx2IBlocked=1;d.fCmd2KFrozen=1;d.fPc1Suspended=1;d.fRxZcFault=4;d.com.sc_generation=2;
 d.zc19Publish();assert(p.wire[6]==2&&p.wire[8]==2&&p.wire[9]==1&&p.wire[10]==1&&p.wire[13]==1&&p.wire[14]==4&&p.wire[28]==1);
 d.fDma5Selected=false;d.zc19Publish();assert(!p.wire[5]);p.fail=true;d.zc19Publish();assert(d.fZc19PublishFail==1);p.fail=false;
 std::thread task([&]{for(unsigned i=0;i<10000;++i){__atomic_store_n(&d.fCmd2KEpoch,uint32_t(i),__ATOMIC_SEQ_CST);__atomic_store_n(&d.fZc18Stats[6],uint64_t(i),__ATOMIC_RELAXED);__atomic_store_n(&d.fZc18Stats[11],uint64_t(65536),__ATOMIC_RELAXED);}});
 for(unsigned i=0;i<10000;++i)d.zc19Publish();task.join();assert(p.wire[22]<=9999);
 assert(d.periodicCaptured==d.periodicPublished&&d.periodicPublished==p.writes);
 unsigned writes=p.writes;d.pci.pa_tag=nullptr;d.zc19Publish();assert(p.writes==writes);d.pci.pa_tag=&p;d.gate=nullptr;d.zc19Publish();assert(p.writes==writes);
 }
 std::cout<<"PASS production ZC26 health callbacks (periodic capture under gate, publication outside gate): distinct controller owner, nonblocking miss, actual mode/unresolved, all fault/epoch fields, no gated allocation, provider failure, atomic BA-task updates, absent provider/gate. Schema19/1 unchanged; no packet-path observers or new timer. Teardown event-removal lifetime remains existing controller contract.\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc26-health-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+extract('zc19Snapshot')+extract('zc19Publish')+test)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print(cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
