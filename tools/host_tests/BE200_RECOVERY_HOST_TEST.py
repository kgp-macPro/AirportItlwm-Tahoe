#!/usr/bin/env python3
"""Actual P6/P7 state task, P7 predicates/stop/wait and scan functions.
Gate/firmware callbacks are mocks; saved P6 hardware history is checked separately.
"""
from pathlib import Path
import re,subprocess,tempfile,sys,json
R=Path(__file__).resolve().parents[2];G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
def fn(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S);x=re.search(r'(?:void|int|bool|IOReturn)\s+ItlIwx::\s*'+n+r'\(',m);assert x,n;a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
cpp=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();tr=(R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text();bz=(R/'itlwm/hal_iwx/ItlIwxBz.inc').read_text()
functions='\n'.join(fn(tr,n)for n in ['bzCommandReady','bzStateCurrent','bzWaitForScan'])+'\n'+'\n'.join(fn(cpp,n)for n in ['iwx_newstate_task','pc1StopEndAction','iwx_stop','iwx_init'])+'\n'+fn(bz,'iwx_bz_port_scan')
commandPrefix=fn(cpp,'iwx_send_cmd');commandPrefix=commandPrefix[:commandPrefix.index('    struct iwx_tx_ring *ring')]+'    ++commands;return 0;\n}\n'
functions+='\n'+commandPrefix
old=subprocess.check_output([G,'show','832a02cc779258d0e007c782d592a4ece907f076:itlwm/hal_iwx/ItlIwx.cpp'],cwd=R,text=True);oldfn=fn(old,'iwx_newstate_task').replace('iwx_newstate_task','oldState')+'\n'+fn(old,'iwx_init').replace('iwx_init(','oldInit(')
fixture=json.loads((R/'tools/host_tests/fixtures/BE200_P6_OFFON.json').read_text());errors=[e for h in fixture['histories']for e in h['events']if e['event']==1];assert any(e['a']==30 and e['b']==35 for e in errors);first=fixture['flow']['firstFailure'];assert(first['code'],first['a'],first['status'])==(0x11e,8,35)
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <functional>
#include <vector>
#include <iostream>
#include <mutex>
using UInt32=uint32_t;using IOReturn=int;constexpr int kIOReturnSuccess=0,kIOReturnNotReady=-1;
struct OSObject{virtual~OSObject()=default;};
constexpr int IFF_UP=1,IFF_RUNNING=2,IFF_DEBUG=4,IFM_AUTO=0;
enum ieee80211_state{IEEE80211_S_INIT=0,IEEE80211_S_SCAN,IEEE80211_S_AUTH,IEEE80211_S_ASSOC,IEEE80211_S_RUN};
constexpr unsigned IWX_FLAG_SCANNING=4,IWX_FLAG_BGSCAN=0x200,IWX_FLAG_SHUTDOWN=0x100,IWX_FLAG_MAC_ACTIVE=8,IWX_FLAG_BINDING_ACTIVE=16,IWX_FLAG_STA_ACTIVE=32,IWX_FLAG_TE_ACTIVE=64,IWX_FLAG_HW_ERR=128,IWX_FLAG_TXFLUSH=0x400,IWX_FLAG_MLD_TXQ_ACTIVE=0x800;
constexpr int IWX_DEVICE_FAMILY_AX210=2,IEEE80211_MODE_AUTO=0;
constexpr int PCATCH=0,IEEE80211_M_MONITOR=1,IWX_INVALID_QUEUE=255;
#define ARRAY_SIZE(a) nitems(a)
#define SEC_TO_NSEC(n) ((n)*1000000000ULL)
static int tsleep_nsec(void*,int,const char*,uint64_t){return EWOULDBLOCK;}
constexpr int IWX_GATE_WAIT_INIT_SCAN=5,IEEE80211_CHAN_WIDTH_20_NOHT=0,IEEE80211_NODE_HT=1;
struct ItlIwx;struct iwx_host_cmd{unsigned id=0x11e;void*resp_pkt=(void*)uintptr_t(1);};struct iwx_softc;struct Queue{};struct Task{bool pending=false;};
struct _ifnet{unsigned if_flags=IFF_UP|IFF_RUNNING;void*if_softc=nullptr;Queue if_snd;unsigned if_timer=0;const char*if_xname="mock";};
struct Node{unsigned ni_chw=0;int ni_chan=0;};struct iwx_node{Node in_ni;void*in_phyctxt=nullptr;char in_macaddr[6]={};};
struct Media{unsigned ifm_media=0;};struct ieee80211com{_ifnet ic_if;iwx_node node;Node*ic_bss=&node.in_ni;int ic_opmode=0,ic_ibss_chan=0;ieee80211_state ic_state=IEEE80211_S_RUN;struct{Media m;Media*ifm_cur=&m;}ic_media;};
struct iwx_rxba_data{};struct Provider{template<class...A>void setProperty(A...){}};
struct iwx_softc{unsigned sc_device_family=3;struct{int qid=0;}sc_tid_data[8];struct{bool sku_cap_11n_enable=true,sku_cap_11ac_enable=true,sku_cap_11ax_enable=true;}sc_nvm;ItlIwx*owner=nullptr;ieee80211com sc_ic;int sc_generation=1;unsigned sc_flags=0,ns_arg=0;ieee80211_state ns_nstate=IEEE80211_S_INIT;Task init_task,newstate_task,ba_task,mac_ctxt_task,chan_ctxt_task;void*sc_nswq=nullptr;struct{int refs=1;}task_refs;void*sc_cmd_resp_pkt[4]={};unsigned sc_cmd_resp_len[4]={};unsigned sc_rx_ba_sessions=0,sc_tx_timer=0;struct{unsigned start_tidmask=0,stop_tidmask=0;}ba_rx,ba_tx;iwx_rxba_data sc_rxba_data[4]={};uint32_t sc_mld_key_flags[8]={};uint8_t sc_mld_txq_pending=0,sc_mld_txq_failed=0;uint32_t sc_scan_builds=0;uint64_t sc_scan_start_ns=0;Provider p;Provider*sc_pcitag=&p;int(*sc_newstate)(ieee80211com*,ieee80211_state,int)=nullptr;};
#define container_of(p,t,m) ((p)->owner)
#define IC2IFP(ic) (&(ic)->ic_if)
#define XYLog(...) ((void)0)
#define DEVNAME(sc) "mock"
#define KASSERT(x,...) assert(x)
#define nitems(x) (sizeof(x)/sizeof((x)[0]))
#define IFM_MODE(x) (x)
#define IEEE80211_ADDR_COPY(a,b) std::memcpy(a,b,6)
static char etheranyaddr[6]={};static void*systq=nullptr;
static int splnet(){return 1;}static void splx(int){}
static void task_add(void*,Task*t){t->pending=true;}static void task_del(void*,Task*t){t->pending=false;}
static void ifq_clr_oactive(Queue*){}static void ifq_flush(Queue*){}
static void ieee80211_setmode(ieee80211com*,int){}static void ieee80211_set_link_state(ieee80211com*,int){}constexpr int LINK_STATE_DOWN=0;
static void ieee80211_node_cleanup(ieee80211com*,Node*){}static uint64_t iwx_boot_time_ns(){return 1;}
static int original(ieee80211com*ic,ieee80211_state n,int){ic->ic_state=n;return 0;}
struct Gate{OSObject*owner;bool held=false,fail=false;unsigned wakes=0;using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);
 template<class F>IOReturn runAction(F f,void*a=nullptr,void*b=nullptr,void*c=nullptr,void*d=nullptr){if(fail)return -1;bool saved=held;held=true;auto r=f(owner,a,b,c,d);held=saved;return r;}
 bool inGate(){return held;}void commandWakeup(void*){++wakes;}};
struct Loop{Gate*gate;bool inGate(){return gate->inGate();}template<class F>IOReturn runAction(F f,OSObject*target,void*a=nullptr,void*b=nullptr,void*c=nullptr,void*d=nullptr){if(gate->fail)return -1;bool saved=gate->held;gate->held=true;auto r=f(target,a,b,c,d);gate->held=saved;return r;}};
struct ItlIwx:OSObject{
 iwx_softc com;Gate gate;Loop loop;bool bz=true,fBzHardwareOwned=true,fBzScanStarted=false,master=true;
 uint32_t fPc1ResetEpoch=1,fBzFault=0,fBzError=0,fTx2IBlocked=0,fPc1Fault=0,fPc1TerminalEpoch=0,fPc1ExpectedGeneration=0;
 uint32_t fBzScanGeneration=0,fBzScanEpoch=0,fBzScanBeacons=0,fBzScanProbes=0;
 unsigned runStops=0,deauths=0,auths=0,runs=0,scans=0,publishes=0,stops=0,commands=0,events=0,sleeps=0,failures=0;
 std::function<void()>onLeave,onRunStop,onDeauth,onAuth,onRun,onScan,onSleep;
 unsigned errAt=0;int sleepError=0;bool scanError=false;
 ItlIwx():gate{this},loop{&gate}{com.owner=this;com.sc_ic.ic_if.if_softc=&com;com.sc_newstate=original;gate.held=true;}
 bool bzDevice(){return bz;}Loop*getMainWorkLoop(){return &loop;}Gate*getMainCommandGate(){return &gate;}
 int iwx_send_cmd(iwx_softc*,iwx_host_cmd*);bool bzCommandReady(iwx_softc*);bool bzStateCurrent(iwx_softc*,int,uint32_t);int bzWaitForScan(iwx_softc*,int,uint32_t);
 static void iwx_newstate_task(void*);static void oldState(void*);void iwx_stop(_ifnet*,unsigned);int iwx_bz_port_scan(iwx_softc*);
 static IOReturn pc1StopEndAction(OSObject*,void*,void*,void*,void*);
 int iwx_init(_ifnet*);int oldInit(_ifnet*);
 static IOReturn pc1RecoverAction(OSObject*o,void*g,void*,void*,void*){auto*t=static_cast<ItlIwx*>(o);return unsigned(uintptr_t(g))==unsigned(t->com.sc_generation)?kIOReturnSuccess:kIOReturnNotReady;}
 static IOReturn zc16PublishRun(OSObject*o,void*g,void*,void*,void*){auto*t=static_cast<ItlIwx*>(o);t->com.sc_ic.ic_state=IEEE80211_S_RUN;t->fBzScanStarted=true;t->fBzScanGeneration=unsigned(uintptr_t(g));t->fBzScanEpoch=t->fPc1ResetEpoch;return kIOReturnSuccess;}
 int iwx_init_hw(iwx_softc*){fBzHardwareOwned=true;return 0;}
 void iwx_setup_ht_rates(iwx_softc*){}void iwx_setup_vht_rates(iwx_softc*){}void iwx_setup_he_rates(iwx_softc*){}
 void pc1Report(){}uint32_t drain2MPause(){return 42;}bool tx2IStop(unsigned){return true;}template<class...A>void flow2J(A...){}void flow2JPublish(){}void tx2JStopEnd(bool,unsigned){}
 void bzConnectionEvent(unsigned,uint64_t,uint64_t,uint64_t){++events;}
 void iwx_del_task(iwx_softc*,void*,Task*t){t->pending=false;}void iwx_clear_reorder_buffer(iwx_softc*,iwx_rxba_data*){}
 void iwx_stop_device(iwx_softc*sc,unsigned){++stops;++fPc1ResetEpoch;if(master){fBzHardwareOwned=false;fPc1TerminalEpoch=fPc1ResetEpoch;fPc1ExpectedGeneration=sc->sc_generation+1;sc->sc_flags&=~IWX_FLAG_MLD_TXQ_ACTIVE;sc->sc_mld_txq_pending=sc->sc_mld_txq_failed=0;std::memset(sc->sc_mld_key_flags,0,sizeof(sc->sc_mld_key_flags));}else{fPc1Fault=2;fBzFault=7;fTx2IBlocked=1;}}
 void reset(){++com.sc_generation;++fPc1ResetEpoch;fBzHardwareOwned=false;com.sc_flags=IWX_FLAG_MLD_TXQ_ACTIVE;com.sc_ic.ic_state=IEEE80211_S_INIT;com.sc_ic.ic_if.if_flags&=~IFF_RUNNING;}
 void iwx_mld_send_leave(iwx_softc*,ieee80211_state){if(onLeave)onLeave();}
 int iwx_run_stop(iwx_softc*){++runStops;if(onRunStop)onRunStop();++commands;if(!fBzHardwareOwned){fBzFault=30;fBzError=35;return EWOULDBLOCK;}if(errAt==1){fBzFault=30;fBzError=35;return EWOULDBLOCK;}return 0;}
 int iwx_deauth(iwx_softc*){++deauths;if(onDeauth)onDeauth();return errAt==2?EIO:0;}
 int iwx_auth(iwx_softc*){++auths;if(onAuth)onAuth();return 0;}
 int iwx_run(iwx_softc*){++runs;if(onRun)onRun();return 0;}
 int iwx_scan(iwx_softc*){++scans;return 0;}int iwx_rs_init(iwx_softc*,iwx_node*,bool){return 0;}
 int iwx_scan_abort(iwx_softc*){return 0;}int iwx_umac_scan(iwx_softc*,int){if(onScan)onScan();return scanError?EIO:0;}
 int iwx_gate_sleep(iwx_softc*,void*,uint32_t*budget,int){++sleeps;if(onSleep)onSleep();if(*budget>0)--*budget;return sleepError?sleepError:(*budget?0:EWOULDBLOCK);}
};
'''
tests=r'''
static unsigned cases=0;
#define CASE(x) do{x;++cases;}while(0)
int main(){
 CASE({ItlIwx t;iwx_host_cmd c;t.fBzHardwareOwned=false;assert(t.iwx_send_cmd(&t.com,&c)==ENXIO&&!t.commands&&!c.resp_pkt);});
 CASE({ItlIwx t;iwx_host_cmd c;t.gate.held=false;assert(!t.iwx_send_cmd(&t.com,&c)&&t.commands==1&&!t.gate.held);});
 CASE({ItlIwx t;t.com.sc_ic.ic_state=IEEE80211_S_INIT;assert(t.oldInit(&t.com.sc_ic.ic_if)==EWOULDBLOCK&&t.stops==1);});
 CASE({ItlIwx t;t.com.sc_ic.ic_state=IEEE80211_S_INIT;t.gate.held=false;assert(!t.iwx_init(&t.com.sc_ic.ic_if)&&!t.stops&&!t.sleeps&&!t.gate.held);});
 CASE({ItlIwx t;t.com.sc_ic.ic_if.if_flags=0;assert(t.iwx_init(&t.com.sc_ic.ic_if)==ENXIO&&!t.stops);});
 CASE({ItlIwx t;t.onLeave=[&]{t.reset();};ItlIwx::oldState(&t.com);assert(t.runStops==1&&t.fBzFault==30&&t.fBzError==35);});
 CASE({ItlIwx t;t.onLeave=[&]{t.reset();};ItlIwx::iwx_newstate_task(&t.com);assert(!t.runStops&&!t.deauths&&!t.com.init_task.pending&&!t.fBzFault);});
 CASE({ItlIwx t;ItlIwx::iwx_newstate_task(&t.com);assert(t.runStops==1&&t.deauths==1&&t.com.sc_ic.ic_state==IEEE80211_S_INIT&&!t.com.init_task.pending);});
 CASE({ItlIwx t;t.onRunStop=[&]{t.reset();};ItlIwx::iwx_newstate_task(&t.com);assert(t.runStops==1&&!t.deauths&&!t.com.init_task.pending);});
 CASE({ItlIwx t;t.onDeauth=[&]{t.reset();};ItlIwx::iwx_newstate_task(&t.com);assert(t.com.sc_ic.ic_state==IEEE80211_S_INIT&&!t.com.init_task.pending);});
 CASE({ItlIwx t;t.errAt=1;ItlIwx::iwx_newstate_task(&t.com);assert(t.com.init_task.pending&&t.fBzFault==30);});
 CASE({ItlIwx t;t.fBzHardwareOwned=false;ItlIwx::iwx_newstate_task(&t.com);assert(!t.commands&&!t.runStops&&!t.com.init_task.pending);});
 CASE({ItlIwx t;t.com.sc_ic.ic_if.if_flags=0;ItlIwx::iwx_newstate_task(&t.com);assert(!t.runStops);});
 for(int op=0;op<2;++op){ItlIwx t;t.com.sc_ic.ic_state=IEEE80211_S_SCAN;t.com.ns_nstate=op?IEEE80211_S_RUN:IEEE80211_S_AUTH;t.onAuth=[&]{t.reset();};t.onRun=[&]{t.reset();};ItlIwx::iwx_newstate_task(&t.com);assert(t.com.sc_ic.ic_state==IEEE80211_S_INIT&&!t.com.init_task.pending);++cases;}
 CASE({ItlIwx t;t.gate.held=false;ItlIwx::iwx_newstate_task(&t.com);assert(t.runStops==1&&!t.gate.held);});
 CASE({ItlIwx t;t.fBzScanStarted=true;t.fBzScanGeneration=1;t.fBzScanEpoch=1;t.com.sc_ic.ic_state=IEEE80211_S_RUN;assert(!t.bzWaitForScan(&t.com,1,1)&&!t.sleeps);});
 CASE({ItlIwx t;t.onSleep=[&]{t.fBzScanStarted=true;t.fBzScanGeneration=1;t.fBzScanEpoch=1;t.com.sc_ic.ic_state=IEEE80211_S_AUTH;};t.sleepError=EWOULDBLOCK;assert(!t.bzWaitForScan(&t.com,1,1)&&t.sleeps==1);});
 CASE({ItlIwx t;t.onSleep=[&]{t.reset();};t.sleepError=EWOULDBLOCK;assert(t.bzWaitForScan(&t.com,1,1)==ENXIO);});
 CASE({ItlIwx t;t.sleepError=EWOULDBLOCK;assert(t.bzWaitForScan(&t.com,1,1)==EWOULDBLOCK);});
 CASE({ItlIwx t;assert(t.bzWaitForScan(&t.com,1,1)==EWOULDBLOCK&&t.sleeps==2000);});
 CASE({ItlIwx t;t.fBzScanStarted=true;t.fBzScanGeneration=0;t.fBzScanEpoch=1;t.sleepError=EWOULDBLOCK;assert(t.bzWaitForScan(&t.com,1,1)==EWOULDBLOCK);});
 CASE({ItlIwx t;t.fBzFault=30;t.fBzError=35;t.fBzScanStarted=true;t.fBzScanGeneration=t.fBzScanEpoch=1;assert(t.bzWaitForScan(&t.com,1,1)==35&&!t.sleeps);});
 CASE({ItlIwx t;assert(t.bzCommandReady(&t.com));t.fBzHardwareOwned=false;assert(!t.bzCommandReady(&t.com));t.fBzHardwareOwned=true;t.fBzFault=30;assert(!t.bzCommandReady(&t.com));});
 CASE({ItlIwx t;t.com.sc_flags=IWX_FLAG_SHUTDOWN;assert(!t.bzCommandReady(&t.com));t.com.sc_flags=0;t.gate.held=false;assert(!t.bzCommandReady(&t.com));});
 CASE({ItlIwx t;t.com.sc_ic.ic_state=IEEE80211_S_INIT;assert(!t.iwx_bz_port_scan(&t.com));assert(t.fBzScanStarted&&t.fBzScanGeneration==1&&t.fBzScanEpoch==1&&t.gate.wakes==1);});
 CASE({ItlIwx t;t.onScan=[&]{t.reset();};assert(t.iwx_bz_port_scan(&t.com)==ENXIO&&!t.fBzScanStarted);});
 CASE({ItlIwx t;t.scanError=true;assert(t.iwx_bz_port_scan(&t.com)==EIO&&!t.fBzScanStarted);});
 CASE({ItlIwx t;t.gate.held=false;t.com.sc_flags=IWX_FLAG_MLD_TXQ_ACTIVE;t.com.sc_mld_key_flags[0]=123;t.iwx_stop(&t.com.sc_ic.ic_if,5);assert(t.com.sc_generation==2&&t.fPc1ExpectedGeneration==3&&t.fPc1TerminalEpoch==2);assert(!t.gate.held&&!t.fBzHardwareOwned&&!t.com.sc_mld_key_flags[0]&&!(t.com.sc_flags&IWX_FLAG_MLD_TXQ_ACTIVE));assert(t.com.sc_ic.ic_state==IEEE80211_S_INIT);});
 CASE({ItlIwx t;t.master=false;t.com.sc_flags=IWX_FLAG_MLD_TXQ_ACTIVE;t.com.sc_mld_key_flags[0]=123;t.iwx_stop(&t.com.sc_ic.ic_if,5);assert(t.fBzHardwareOwned&&t.fPc1Fault==2&&t.com.sc_mld_key_flags[0]==123&&(t.com.sc_flags&IWX_FLAG_MLD_TXQ_ACTIVE));assert(!t.bzCommandReady(&t.com));});
 CASE({ItlIwx t;t.gate.held=false;t.gate.fail=true;t.iwx_stop(&t.com.sc_ic.ic_if,5);assert(!t.stops&&t.fTx2IBlocked&&t.fPc1Fault);});
 CASE({ItlIwx t;t.bz=false;t.iwx_stop(&t.com.sc_ic.ic_if,5);assert(t.com.sc_generation==2&&t.fPc1ExpectedGeneration==3);});
 std::cout<<"PASS "<<cases<<" actual state-task/stop/scan/wait/command-admission cases. Old P6 post-leave stopped-firmware RUN_STOP fault reproduced at mock command boundary. No hardware or physical recovery proof.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-recovery-')as tmp:
 t=Path(tmp);p=t/'test.cpp';p.write_text(pre+functions+'\n'+oldfn+'\n'+tests)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-fsanitize='+san,str(p),'-o',str(t/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(t/'test')],check=True,timeout=60)
print('Saved P6 hardware trace independently checked: first fault30/35,TXPATH_FLUSH0x11e/8bytes. Mock interleaving is not a recovered live call stack.')
