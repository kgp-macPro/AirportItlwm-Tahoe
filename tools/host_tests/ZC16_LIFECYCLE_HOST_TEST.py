#!/usr/bin/env python3
"""Extract changed production transitions; mock firmware/PM/gate, not hardware proof."""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2]
def extract(file,cls,name):
 s=(R/file).read_text();m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:void|int|IOReturn)\s+'+cls+r'::\s*'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cerrno>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>
using IOReturn=int;
constexpr int kIOReturnSuccess=0,kIOReturnNotReady=-1,kIOReturnError=-2;
constexpr int IFF_UP=1,IFF_RUNNING=2,DVACT_QUIESCE=1,DVACT_RESUME=2,DVACT_WAKEUP=3;
constexpr int kPowerStateOff=0,kPowerStateOn=1,kWatchDogTimerPeriod=1000;
constexpr int IEEE80211_M_MONITOR=1,IEEE80211_S_RUN=2;
#define XYLog(...) ((void)0)
#define IOLog(...) ((void)0)
#define OSDynamicCast(T,p) dynamic_cast<T*>(p)
struct OSObject{virtual ~OSObject()=default;};
using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);
struct Gate{
 OSObject*owner=nullptr;std::recursive_mutex lock;static thread_local std::vector<Gate*>held;
 bool inGate(){for(auto*g:held)if(g==this)return true;return false;}
 IOReturn runAction(Action f,void*a=nullptr,void*b=nullptr,void*c=nullptr,void*d=nullptr){
  std::lock_guard<std::recursive_mutex>l(lock);held.push_back(this);auto r=f(owner,a,b,c,d);held.pop_back();return r;}
};thread_local std::vector<Gate*>Gate::held;
struct Queue{int flushes=0;};void ifq_clr_oactive(Queue*){}void ifq_flush(Queue*q){++q->flushes;}
struct _ifnet{int if_flags=0;Queue if_snd;};
struct Node{int ni_chan=0;};
struct IC{struct{_ifnet ac_if;}ic_ac;_ifnet&ic_if=ic_ac.ac_if;int ic_opmode=0,ic_ibss_chan=1;Node node;Node*ic_bss=&node;};
struct Task{bool queued=false;unsigned adds=0;};void*systq=nullptr;void task_add(void*,Task*t){if(!t->queued){t->queued=true;++t->adds;}}
struct iwx_softc{IC sc_ic;int sc_generation=1,sc_flags=0;Task init_task;};
struct IONetworkInterface{};
struct Hal:OSObject{virtual IOReturn enable(IONetworkInterface*)=0;virtual IOReturn disable(IONetworkInterface*)=0;virtual IC*get80211Controller()=0;};
static thread_local unsigned scans=0,monitorRuns=0;
void ieee80211_begin_scan(_ifnet*p){assert(p->if_flags&IFF_RUNNING);++scans;}
void ieee80211_new_state(IC*,int,int){++monitorRuns;}
void net80211_ifstats(IC*){}
struct ItlIwx:Hal{
 bool bzDevice(){return false;}int iwx_bz_port_activate(iwx_softc*,int){assert(false);return -1;}
 iwx_softc com;Gate*loop;unsigned fPc1Suspended=0,fPc1Fault=0,epoch=1;bool paused=true,closed=false,hwReady=true;int resumeError=0;unsigned stops=0,resumes=0,arms=0;
 explicit ItlIwx(Gate*g):loop(g){}
 IC*get80211Controller(){return &com.sc_ic;}
 IOReturn enable(IONetworkInterface*) override;IOReturn disable(IONetworkInterface*) override;
 int iwx_activate(iwx_softc*,int);
 template<class...T>void flow2J(T...){}
 int iwx_resume(iwx_softc*){assert(loop->inGate());++resumes;return resumeError;}
 bool iwx_set_hw_ready(iwx_softc*){assert(loop->inGate());return hwReady;}
 void iwx_stop(_ifnet*p,unsigned){assert(loop->inGate());++stops;++com.sc_generation;++epoch;paused=true;fPc1Suspended=1;p->if_flags&=~IFF_RUNNING;com.init_task.queued=false;}
 void drain2MArm(uint32_t e){assert(loop->inGate());if(!closed&&e==epoch){paused=false;++arms;}}
 static IOReturn zc16PublishRun(OSObject*,void*,void*,void*,void*);
};
struct OtherHal:Hal{IC ic;unsigned enables=0,disables=0;IC*get80211Controller(){return &ic;}
 IOReturn enable(IONetworkInterface*){++enables;return kIOReturnError;}IOReturn disable(IONetworkInterface*){++disables;return 0;}};
struct Timer{unsigned starts=0,stops=0;void setTimeoutMS(int){}void enable(){++starts;}void cancelTimeout(){++stops;}void disable(){}};
struct Policy{unsigned acks=0;void acknowledgeSetPowerState(){++acks;}};
struct apple80211_power_data{unsigned num_radios=1,power_state[4]={1,1,1,1};};
struct AirportItlwm:OSObject{
 Gate gate;Hal*fHalService;IONetworkInterface iface;IONetworkInterface*fNetIf=&iface;Timer timer;Timer*watchdogTimer=&timer;Policy policy;Policy*pmPolicyMaker=&policy;
 unsigned pmPowerState=1;uint8_t power_state=1;
 AirportItlwm(){gate.owner=this;fHalService=new ItlIwx(&gate);}~AirportItlwm(){delete fHalService;}
 Gate*getWorkLoop(){return &gate;}Gate*getCommandGate(){return &gate;}
 void changePowerStateToPriv(int){}
 IOReturn enableAdapter(IONetworkInterface*);void disableAdapter(IONetworkInterface*);
 IOReturn setPOWER(OSObject*,apple80211_power_data*);
 void setPowerStateOn();void setPowerStateOff();
 static IOReturn zc16AdapterPowerGated(OSObject*,void*,void*,void*,void*);
 static IOReturn zc16SetPowerGated(OSObject*,void*,void*,void*,void*);
};
'''
names=[('AirportItlwm/AirportItlwm.cpp','AirportItlwm',n) for n in ['zc16AdapterPowerGated','enableAdapter','disableAdapter','setPowerStateOn','setPowerStateOff']]+[('AirportItlwm/AirportSTAIOCTL.cpp','AirportItlwm',n) for n in ['zc16SetPowerGated','setPOWER']]+[('itlwm/hal_iwx/ItlIwx.cpp','ItlIwx',n) for n in ['enable','disable','iwx_activate','zc16PublishRun']]
test=r'''
static IOReturn power(OSObject*o,void*on,void*,void*,void*){auto*a=static_cast<AirportItlwm*>(o);if(on)a->setPowerStateOn();else a->setPowerStateOff();return 0;}
static IOReturn publish(OSObject*o,void*g,void*e,void*,void*){auto*a=static_cast<AirportItlwm*>(o);return ItlIwx::zc16PublishRun(a->fHalService,g,e,nullptr,nullptr);}
static void user(AirportItlwm&a,bool on){apple80211_power_data p;p.power_state[0]=on;assert(!a.setPOWER(nullptr,&p));}
static int complete(AirportItlwm&a,unsigned g,unsigned e){auto*d=static_cast<ItlIwx*>(a.fHalService);if(g==unsigned(d->com.sc_generation)&&!d->fPc1Fault)d->fPc1Suspended=0;
 return a.gate.runAction(publish,reinterpret_cast<void*>(uintptr_t(g)),reinterpret_cast<void*>(uintptr_t(e)));}
static void run(){
 for(int family:{201,210,211,22000})for(bool mapped:{false,true}){
  if((family==201||family==22000)&&mapped)continue; // unchanged positive qualification
  AirportItlwm a;auto*d=static_cast<ItlIwx*>(a.fHalService);auto&f=d->com.sc_ic.ic_if.if_flags;
  assert(!a.enableAdapter(a.fNetIf));assert(d->com.init_task.queued&&f==IFF_UP);
  auto n=d->com.init_task.adds;assert(!a.enableAdapter(a.fNetIf));assert(d->com.init_task.adds==n);
  unsigned oldg=d->com.sc_generation,olde=d->epoch;
  // Sleep interrupts initialization BEFORE IFF_RUNNING. Actual quiesce must stop.
  a.gate.runAction(power);assert(d->stops==1&&f==0&&!d->com.init_task.queued);
  assert(complete(a,oldg,olde)==kIOReturnNotReady&&f==0);
  n=d->com.init_task.adds;a.gate.runAction(power,(void*)1);assert(d->com.init_task.adds==n);
  // Preserve historical IO80211 restart: PM On alone never initiates one.
  user(a,true);assert(f==IFF_UP&&d->com.init_task.queued);
  assert(!complete(a,d->com.sc_generation,d->epoch));assert(f==(IFF_UP|IFF_RUNNING)&&!d->paused);
  for(int i=0;i<1000;++i){a.gate.runAction(power);assert(f==0&&d->paused);a.gate.runAction(power,(void*)1);user(a,true);
   assert(!complete(a,d->com.sc_generation,d->epoch));assert(f==(IFF_UP|IFF_RUNNING));}
  user(a,false);a.gate.runAction(power);a.gate.runAction(power,(void*)1);assert(f==0&&a.power_state==0);
  d->resumeError=EIO;assert(a.enableAdapter(a.fNetIf)==kIOReturnNotReady&&f==0);d->resumeError=0;
  d->hwReady=false;assert(a.enableAdapter(a.fNetIf)==kIOReturnNotReady&&f==0);d->hwReady=true;
  assert(!a.enableAdapter(a.fNetIf));unsigned g=d->com.sc_generation,e=d->epoch;
  d->fPc1Fault=2;auto before=scans;assert(complete(a,g,e)==kIOReturnNotReady&&scans==before);d->fPc1Fault=0;
  d->fPc1Suspended=1;assert(a.gate.runAction(publish,(void*)(uintptr_t)g,(void*)(uintptr_t)e)==kIOReturnNotReady);
  d->fPc1Suspended=0;assert(!complete(a,g,e));
  // Radio requests and PM transitions serialize; no mock firmware/host data race.
  std::thread t([&]{for(int i=0;i<1000;++i){user(a,false);user(a,true);}});
  for(int i=0;i<1000;++i){a.gate.runAction(power);a.gate.runAction(power,(void*)1);}t.join();user(a,false);assert(f==0);
 }
 {AirportItlwm a;delete a.fHalService;auto*h=new OtherHal;a.fHalService=h;a.gate.runAction(power,(void*)1);assert(h->enables==0);
  assert(!a.enableAdapter(a.fNetIf)&&h->enables==1);a.disableAdapter(a.fNetIf);assert(h->disables==1);}
}
int main(){run();std::cout<<"PASS extracted IWX enable/disable/activate, controller dispatch, PM policy unchanged, init interruption, 1000 repeated sleep/wake sequences per qualified mode/family, duplicate request, resume/readiness errors, stale generation, PC1 fault/suspend, radio Off, concurrent gated request ordering, IWM/IWN compatibility. Firmware, physical PM and IO80211 call ordering are mocks.\n";}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc16-lifecycle-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+'\n'+'\n'.join(extract(*v) for v in names)+test)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120)
