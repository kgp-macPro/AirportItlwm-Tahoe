#!/usr/bin/env python3
"""Actual frontend functions with a mock of the UUID-matched framework contract.
The framework mock is a binary-derived reconstruction, NOT recovered Apple code.
No IORegistry, physical driver, DHCP, settings or network operation is executed.
"""
from pathlib import Path
import re,json,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2]
G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
C=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip()
def fn(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:bool|IOReturn)\s+AirportItlwm::\s*'+n+r'\(',m);assert x,n
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
s=(R/'AirportItlwm/AirportItlwm.cpp').read_text();h=(R/'AirportItlwm/AirportItlwm.hpp').read_text()
request=re.search(r'    struct LinkStatusRequest \{.*?\n    \};',h,re.S)[0]
fields='\n'.join(re.findall(r'^    (?:UInt32 currentStatus;|bool fLinkState\w+;)',h,re.M))
assert len(fields.splitlines())==4
new='\n'.join(fn(s,n)for n in ['setLinkStatus','setLinkStatusGated','setLinkStatusInGate'])
oldsrc=subprocess.check_output([G,'show','b9f300720eccc1001777c0619d6da8f4753eaeab:AirportItlwm/AirportItlwm.cpp'],cwd=R,text=True)
old='\n'.join(fn(oldsrc,n)for n in ['setLinkStatus','setLinkStateGated']).replace('AirportItlwm','OldAirportItlwm')
fixture=json.loads((R/'tools/host_tests/fixtures/BE200_P5_CONNECTED_NOIP.json').read_text())
assert fixture['loaded_uuid']=='E52DFFDF-3CB3-3D6A-8C57-004F22FCB75B'and fixture['configd_link_active']is False
assert fixture['MLD_step']==16 and fixture['rsn_done']and fixture['tx_publications']==fixture['tx_retires']==4
good=json.loads((R/'tools/host_tests/fixtures/BE200_P5_OFFON_DHCP.json').read_text())
assert good['loaded_uuid']==fixture['loaded_uuid'] and good['boot_uuid']==fixture['boot_uuid']
assert good['link_active']and good['dhcp_type']=='ACK'and good['generation']!=fixture['generation']
assert not re.search(r'"(?:en[0-9]+|ARPT|CNVW)"',new), 'new frontend must use objects, not registry/BSD names'
good=json.loads((R/'tools/host_tests/fixtures/BE200_P5_OFFON_DHCP.json').read_text())
assert good['loaded_uuid']==fixture['loaded_uuid'] and good['boot_uuid']==fixture['boot_uuid']
assert good['link_active']and good['dhcp_type']=='ACK'and good['generation']!=fixture['generation']
assert not re.search(r'"(?:en[0-9]+|ARPT|CNVW)"',new), 'new frontend must use objects, not registry/BSD names'
pre=r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>
using UInt32=uint32_t;using UInt64=uint64_t;using IOReturn=int;
constexpr IOReturn kIOReturnSuccess=0,kIOReturnError=-1,kIOReturnBadArgument=-2;
constexpr unsigned kIONetworkLinkValid=1,kIONetworkLinkActive=2,kIONetworkLinkNoNetworkChange=0x100;
enum IO80211LinkState {kIO80211NetworkLinkDown=1,kIO80211NetworkLinkUp=2};
struct OSObject {virtual ~OSObject()=default;};struct IONetworkMedium{};struct OSData{};
#define OSDynamicCast(C,p) dynamic_cast<C*>(p)
struct Loop {std::recursive_mutex mutex;static thread_local Loop *held;bool inGate(){return held==this;}};
thread_local Loop *Loop::held=nullptr;
struct Gate {Loop *loop;OSObject*owner;bool fail=false;unsigned calls=0;using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);
 IOReturn runAction(Action a,void*b=nullptr,void*c=nullptr,void*d=nullptr,void*e=nullptr){std::lock_guard<std::recursive_mutex>lock(loop->mutex);++calls;if(fail)return kIOReturnError;auto*saved=Loop::held;Loop::held=loop;auto r=a(owner,b,c,d,e);Loop::held=saved;return r;}};
struct Queue {unsigned starts=0,stops=0,flushes=0;};struct _ifnet {Queue if_snd;};
struct Controller {struct{_ifnet ac_if;}ic_ac;Queue ic_mgtq;unsigned ic_deauth_reason=37;};
struct Hal {Controller controller;Controller*get80211Controller(){return &controller;}};
void ifq_start_pull(Queue*q){++q->starts;}void ifq_stop_pull(Queue*q){++q->stops;}void ifq_flush(Queue*q){++q->flushes;}void mq_purge(Queue*q){++q->flushes;}
struct Base;
struct Interface {Base*controller;unsigned state=0;bool armed=false,failBefore=false,failAfter=false,skipCallback=false;unsigned transitions=0,starts=0,stops=0,flushes=0,quality=0;
 bool setLinkState(IO80211LinkState,unsigned);void startOutputThread(){++starts;}void stopOutputThread(){++stops;}void flushOutputQueue(){++flushes;}void setLinkQualityMetric(unsigned x){quality=x;}};
struct AWDL {unsigned calls=0;void setEnabledBySystem(bool){}void setLinkState(IO80211LinkState,unsigned){++calls;}};
struct Base:OSObject {Loop loop;Gate gate;Interface interface;Hal hal;AWDL awdl;bool noLoop=false,noGate=false,baseFail=false;
 unsigned published=0,upEvents=0,downEvents=0,baseCalls=0;const IONetworkMedium*lastMedium=nullptr;UInt64 lastSpeed=0;OSData*lastData=nullptr;
 Base():gate{&loop,this},interface{this}{}virtual bool setLinkStatus(UInt32 s,const IONetworkMedium*m=nullptr,UInt64 speed=0,OSData*d=nullptr){
 assert(loop.inGate());++baseCalls;if(baseFail)return false;lastMedium=m;lastSpeed=speed;lastData=d;
 if((s&3)!=(published&3)){if(interface.armed){interface.armed=false;if(s&2)++upEvents;else ++downEvents;}}
 published=s;return true;}
 Loop*getWorkLoop(){return noLoop?nullptr:&loop;}Gate*getCommandGate(){return noGate?nullptr:&gate;}Interface*getNetworkInterface(){return &interface;}};
// Reconstructed contract: same state succeeds without callback; transition arms
// inputEvent before synchronous controller callback. A consumed/unarmed event
// cannot reach the mock BSD side. No claim about firmware/hardware is modeled.
bool Interface::setLinkState(IO80211LinkState s,unsigned){assert(controller->loop.inGate());if(state==(unsigned)s)return true;if(failBefore)return false;
 state=s;++transitions;armed=true;if(!skipCallback)controller->setLinkStatus(s==2?3:1,nullptr,s==2?260000000:0,nullptr);return !failAfter;}
'''
decl=r'''
    using super=Base;
    Interface *fNetIf=&interface;Hal*fHalService=&hal;AWDL*fAWDLInterface=&awdl;
    bool setLinkStatus(UInt32,const IONetworkMedium*=nullptr,UInt64=0,OSData*=nullptr) override;
'''.replace('Medium*=','Medium* =').replace('OSData*=','OSData* =')
classes='class AirportItlwm:public Base {public:\n'+decl+request+'\n'+fields+r'''
    AirportItlwm(){currentStatus=0;fLinkStateHandoff=false;fLinkStateCallbackSeen=false;fLinkStateCallbackOK=false;}
    static IOReturn setLinkStatusGated(OSObject*,void*,void*,void*,void*);
    bool setLinkStatusInGate(UInt32,const IONetworkMedium*,UInt64,OSData*);
};
class OldAirportItlwm:public Base {public:
'''+decl+r'''
    UInt32 currentStatus=0;
    static IOReturn setLinkStateGated(OSObject*,void*,void*,void*,void*);
};
'''
tests=r'''
static unsigned cases=0;
#define CASE(x) do{ x; ++cases; }while(0)
int main(){
 // Actual P5 code reproduces UI-up/controller-status3 but lost BSD LinkUp.
 CASE({OldAirportItlwm c;std::lock_guard<std::recursive_mutex>l(c.loop.mutex);Loop::held=&c.loop;assert(c.setLinkStatus(3));Loop::held=nullptr;assert(c.interface.state==2&&c.published==3&&c.upEvents==0);});
 CASE({OldAirportItlwm c;std::lock_guard<std::recursive_mutex>l(c.loop.mutex);Loop::held=&c.loop;assert(c.setLinkStatus(3));assert(c.upEvents==0&&c.interface.armed);assert(c.setLinkStatus(1));assert(c.downEvents==1&&c.interface.armed);assert(c.setLinkStatus(3));assert(c.upEvents==1&&c.interface.armed);Loop::held=nullptr;});
 CASE({AirportItlwm c;assert(c.setLinkStatus(1));assert(c.downEvents==1&&c.currentStatus==1&&!c.fLinkStateHandoff);assert(c.setLinkStatus(3));assert(c.upEvents==1&&c.currentStatus==3&&c.published==3&&!c.interface.armed);assert(c.interface.quality==100&&c.awdl.calls==2);});
 CASE({AirportItlwm c;assert(c.setLinkStatus(3));assert(c.setLinkStatus(1));assert(c.upEvents==1&&c.downEvents==1&&c.published==1);assert(c.hal.controller.ic_ac.ac_if.if_snd.flushes==1&&c.hal.controller.ic_mgtq.flushes==1);});
 CASE({AirportItlwm c;assert(c.setLinkStatus(3));auto b=c.baseCalls;auto t=c.interface.transitions;assert(c.setLinkStatus(3));assert(c.baseCalls==b&&c.interface.transitions==t&&c.upEvents==1);});
 CASE({AirportItlwm c;c.noGate=true;assert(!c.setLinkStatus(3)&&c.baseCalls==0&&c.interface.transitions==0);});
 CASE({AirportItlwm c;c.noLoop=true;assert(!c.setLinkStatus(3)&&c.baseCalls==0);});
 CASE({AirportItlwm c;c.gate.fail=true;assert(!c.setLinkStatus(3)&&c.interface.transitions==0&&c.baseCalls==0);});
 CASE({AirportItlwm c;c.fNetIf=nullptr;assert(c.setLinkStatus(1));assert(c.currentStatus==1&&c.interface.transitions==0&&c.baseCalls==1);});
 CASE({AirportItlwm c;c.fHalService=nullptr;assert(!c.setLinkStatus(3)&&c.baseCalls==0);});
 CASE({AirportItlwm c;c.interface.failBefore=true;assert(!c.setLinkStatus(3));assert(!c.fLinkStateHandoff&&!c.fLinkStateCallbackSeen&&c.currentStatus==0&&c.baseCalls==0&&c.awdl.calls==0);});
 CASE({AirportItlwm c;c.baseFail=true;assert(!c.setLinkStatus(3));assert(!c.fLinkStateHandoff&&c.fLinkStateCallbackSeen&&!c.fLinkStateCallbackOK&&c.baseCalls==1&&c.currentStatus==0&&c.awdl.calls==0);});
 CASE({AirportItlwm c;c.interface.failAfter=true;assert(!c.setLinkStatus(3));assert(!c.fLinkStateHandoff&&c.currentStatus==3&&c.upEvents==1&&c.baseCalls==1&&c.awdl.calls==0);});
 // Framework-origin transition calls the actual override with its event armed.
 CASE({AirportItlwm c;std::lock_guard<std::recursive_mutex>l(c.loop.mutex);Loop::held=&c.loop;assert(c.interface.setLinkState(kIO80211NetworkLinkUp,0));Loop::held=nullptr;assert(c.upEvents==1&&c.baseCalls==1&&c.interface.transitions==1);});
 // Successful no-callback same-state case forwards supplied metadata exactly.
 CASE({AirportItlwm c;IONetworkMedium m;OSData d;c.interface.state=2;c.interface.armed=true;assert(c.setLinkStatus(3,&m,12345,&d));assert(c.upEvents==1&&c.lastMedium==&m&&c.lastSpeed==12345&&c.lastData==&d);});
 CASE({AirportItlwm c;assert(c.setLinkStatus(1|kIONetworkLinkNoNetworkChange));assert(c.interface.transitions==0&&c.hal.controller.ic_ac.ac_if.if_snd.flushes==0);});
 CASE({AirportItlwm c;c.fAWDLInterface=nullptr;assert(c.setLinkStatus(3)&&c.upEvents==1);});
 CASE({AirportItlwm c;assert(AirportItlwm::setLinkStatusGated(nullptr,nullptr,nullptr,nullptr,nullptr)==kIOReturnBadArgument);assert(AirportItlwm::setLinkStatusGated(&c,nullptr,nullptr,nullptr,nullptr)==kIOReturnBadArgument);});
 CASE({AirportItlwm c;assert(!c.setLinkStatusInGate(3,nullptr,0,nullptr));});
 CASE({AirportItlwm c;for(int i=0;i<1000;++i){assert(c.setLinkStatus(3));assert(c.setLinkStatus(1));}assert(c.upEvents==1000&&c.downEvents==1000&&!c.fLinkStateHandoff);});
 CASE({AirportItlwm c;std::vector<std::thread>threads;for(int i=0;i<4;++i)threads.emplace_back([&]{for(int j=0;j<100;++j){assert(c.setLinkStatus(3));assert(c.setLinkStatus(1));}});for(auto&t:threads)t.join();assert(!c.fLinkStateHandoff&&c.currentStatus==1);});
#ifdef __PRIVATE_SPI__
 CASE({AirportItlwm c;assert(c.setLinkStatus(3));assert(c.interface.starts==1&&c.hal.controller.ic_ac.ac_if.if_snd.starts==1);assert(c.setLinkStatus(1));assert(c.interface.stops==1&&c.interface.flushes==1&&c.hal.controller.ic_ac.ac_if.if_snd.stops==1);});
#else
 CASE({AirportItlwm c;assert(c.setLinkStatus(3));assert(c.interface.starts==0);assert(c.setLinkStatus(1));assert(c.interface.stops==0);});
#endif
 std::cout<<"PASS "<<cases<<" actual frontend cases; P5 unarmed-event failure reproduced; 1000 up/down cycles + serialized concurrent transitions. SIMULATED framework/BSD boundary only.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-link-host-')as tmp:
 t=Path(tmp);src=t/'test.cpp';src.write_text(pre+classes+new+'\n'+old+'\n'+tests)
 for private in [False,True]:
  exe=t/('test-private'if private else'test-public');cmd=[C,'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-fno-omit-frame-pointer','-fsanitize='+san,'-Werror=return-type',str(src),'-o',str(exe)]+(['-D__PRIVATE_SPI__=1']if private else[])
  subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
print('Saved P5 hardware fixture: RUN_READY16/RSNdone/4 TX retires, configd LinkActive=false, no IP. Not a mock-generated hardware result.')
