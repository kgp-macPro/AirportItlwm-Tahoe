#!/usr/bin/env python3
"""Production reply routing, TX response/reclaim, MLD auth and connection observer.
Hardware fixtures are saved properties, never invented rejected wire packets.
"""
from pathlib import Path
import re,json,subprocess,tempfile,sys,struct
R=Path(__file__).resolve().parents[2];G='/Applications/Xcode.app/Contents/Developer/usr/bin/git'
C='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++';SDK='/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk'
def fn(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:void|int|bool|kgp_bz::ReplyRoute)\s+ItlIwx::\s*'+n+r'\(',m);assert x,n
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
def body(s,n):
 x=re.search(r'\bstruct '+n+r'\s*\{',s);assert x,n;a=s.index('{',x.start());b=a+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[x.start():s.index(';',b)+1]
src=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();inc=(R/'itlwm/hal_iwx/ItlIwxBz.inc').read_text();trans=(R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text();reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()+(R/'itlwm/hal_iwx/if_iwxvar.h').read_text()
functions='\n'.join(fn(trans,n)for n in ['bzConnectionEvent','bzRouteReply','bzObserveMgmt'])+'\n'+'\n'.join(fn(src,n)for n in ['iwx_rx_tx_cmd','iwx_ampdu_txq_advance','iwx_rx_tx_cmd_single'])+'\n'+'\n'.join(fn(inc,n)for n in ['iwx_mld_probe','iwx_mld_auth'])
skip={'IWX_AGG_SSN_TO_TXQ_IDX','IWX_FC'};constants='';done=set();pending=set(re.findall(r'\bIWX_[A-Z0-9_]+',functions))-skip
while pending:
 n=pending.pop()
 if n in done:continue
 x=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',reg,re.M);assert x,n;done.add(n);constants+='#define '+n+' '+x[1]+'\n';pending|=set(re.findall(r'\bIWX_[A-Z0-9_]+',x[1]))-done-skip
wire='\n'.join(body(reg,n)for n in ['iwx_agg_tx_status','iwx_tx_resp'])
fixture=json.loads((R/'tools/host_tests/fixtures/BE200_P4_INTERFACE.json').read_text());q=struct.unpack('<48Q',bytes.fromhex(fixture['transport_hex']));assert q[8:10]==(3,100)and q[16+11]==46 and q[16+17]==0;assert fixture['properties']['ItlwmPnvmNotifySeen']==1 and fixture['properties']['ItlwmPnvmNotifyStatus']==0x50001
oldcpp=subprocess.check_output([G,'show','a3ff3547f4e9d89a2c2530ef5ced9c26d1a9e0ad:itlwm/hal_iwx/ItlIwx.cpp'],cwd=R,text=True)
a=oldcpp.index('        if (bzDevice() && !(qid & 0x80) && (qid != IWX_DQA_CMD_QUEUE')
b=oldcpp.index('\n',a)
oldguard=oldcpp[a:b].replace('break;','return;')
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cerrno>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <iostream>
#include "itlwm/hal_iwx/ItlIwxBzContracts.hpp"
#include "include/HAL/ItlBzTxReference.hpp"
#define __packed __attribute__((packed))
#define BIT(n) (1U<<(n))
#define __BIT(n) BIT(n)
#define nitems(a) (sizeof(a)/sizeof((a)[0]))
#define IWX_AGG_SSN_TO_TXQ_IDX(n,c) ((n)&((c)-1))
#define le16toh(x) (x)
#define le32toh(x) (x)
#define KASSERT(v,...) assert(v)
#define XYLog(...) ((void)0)
#define DPRINTFN(...) ((void)0)
#define DEVNAME(sc) "fixture"
using UInt32=uint32_t;
constexpr unsigned IEEE80211_S_RUN=4,IEEE80211_FC0_TYPE_MGT=0,IEEE80211_DUR_TU=1024,BUS_DMASYNC_POSTREAD=0;
struct Channel {unsigned ic_flags=0;};static Channel *const IEEE80211_CHAN_ANYC=(Channel*)uintptr_t(1);
struct Provider {std::map<std::string,uint64_t>v;void setProperty(const char*k,uint64_t n,unsigned){v[k]=n;}};
struct iwx_tx_data {void*m=nullptr;unsigned type=0;};struct iwx_tx_ring {unsigned qid=0,ring_count=256,tail=0,queued=0;iwx_tx_data data[512];};
struct NetStat {unsigned outputErrors=0;};struct _ifnet {NetStat stats;NetStat*netStat=&stats;};
struct ieee80211com {int ic_state=2;_ifnet ic_if;};
#define IC2IFP(ic) (&(ic)->ic_if)
struct Node {Channel*ni_chan=nullptr;unsigned ni_intval=100;};struct iwx_node {Node in_ni;};
struct iwx_softc {int sc_generation=1;unsigned sc_device_family=0,sc_flags=0,sc_mgmt_last_antenna_idx=0,sc_tx_timer=0;ieee80211com sc_ic;
 Provider p;Provider*sc_pcitag=&p;struct Commandiwx_tx_ring:iwx_tx_ring {struct{unsigned flags=0;}data[512];}cmdring;
 iwx_tx_ring txq[512];void*sc_dmat=nullptr;};
struct iwx_rx_data {void*map=nullptr;};struct Header{uint8_t cmd,group_id,qid,idx;};struct iwx_rx_packet {uint32_t length;Header hdr;uint8_t data[256];};
static unsigned iwx_rx_packet_payload_len(iwx_rx_packet*p){return p->length;}
static void bus_dmamap_sync(void*,void*,unsigned,unsigned,int){} // production compatibility routine is also a no-op; BZ RX sync occurs earlier.

struct Gate {bool held=true;bool inGate(){return held;}};
static uint64_t time2GNow(){static uint64_t ns=0;return ++ns;}
struct ItlIwx {iwx_softc com;Gate gate;unsigned fPc1ResetEpoch=1,fPc1Suspended=0,fPc1DroppedCompletion=0,fBzQueueOwned=0,fBzFault=0,fBzError=0,fTx2JAdmission=0,fTx2IBlocked=0;
 static constexpr unsigned kTx2JClosed=1;kgp_bz::ConnectionTrace fBzConnection;kgp_bz::CommandLedger fBzCommands;
 unsigned fBzScanBeacons=0,fBzScanProbes=0;uint64_t fBzStats[32]={};unsigned retired=0,freed=0,cleared=0,starts=0,antenna=0,failAt=0,calls=0;std::vector<unsigned>sequence;
 Gate*getMainWorkLoop(){return &gate;}bool bzDevice(){return true;}bool dma5Leased(){return true;}
 void bzFail(unsigned r,int e){if(!fBzFault){fBzFault=r;fBzError=e;}}
 int iwx_cmd_groupid(int code){return (code>>8)&0xff;}int iwx_cmd_opcode(int code){return code&0xff;}
 void bzConnectionEvent(unsigned,uint64_t,uint64_t,uint64_t);void bzObserveMgmt(const void*,size_t);kgp_bz::ReplyRoute bzRouteReply(iwx_softc*,unsigned,unsigned,int*,unsigned);
 void iwx_rx_tx_cmd(iwx_softc*,iwx_rx_packet*,iwx_rx_data*);void iwx_rx_tx_cmd_single(iwx_softc*,iwx_rx_packet*,iwx_tx_data*);void iwx_ampdu_txq_advance(iwx_softc*,iwx_tx_ring*,int,bool);
 void dexterOriginal_iwx_rx_tx_cmd(iwx_softc*,iwx_rx_packet*,iwx_rx_data*){assert(false);}void dexterOriginal_iwx_ampdu_txq_advance(iwx_softc*,iwx_tx_ring*,int){assert(false);}
 void zc28Aggregate(iwx_rx_packet*){assert(false);}void zc28Note(unsigned,unsigned,unsigned,unsigned){}
 void iwx_toggle_tx_ant(iwx_softc*,unsigned*){++antenna;}void iwx_clear_oactive(iwx_softc*,iwx_tx_ring*){++starts;}
 void tx2IRetire(iwx_tx_data*,bool positive){assert(positive);++retired;}void iwx_txd_done(iwx_softc*,iwx_tx_data*d){assert(d->m);d->m=nullptr;++freed;}
 void iwx_clear_tx_desc(iwx_softc*,iwx_tx_ring*,unsigned){++cleared;}
 int call(unsigned n){sequence.push_back(n);return ++calls==failAt?EIO:0;}
 void oldP4Guard(iwx_softc*,unsigned,unsigned);
 int iwx_mld_auth(iwx_softc*,iwx_node*);void iwx_mld_probe(iwx_softc*,uint32_t,int);
 int iwx_mld_mac_config_cmd(iwx_softc*,iwx_node*,uint32_t a,int){return call(a==1?1:20);}
 int iwx_disable_beacon_filter(iwx_softc*){return call(2);}
 int iwx_mld_link_config_cmd(iwx_softc*,iwx_node*,uint32_t a,uint32_t,int active){return call(a==1?3:(active?4:21));}
 int iwx_power_mac_update_mode(iwx_softc*,iwx_node*){return call(5);}
 int iwx_mld_sta_config_cmd(iwx_softc*,iwx_node*,int){return call(6);}
 int iwx_rs_init(iwx_softc*,iwx_node*,bool){return call(7);}
 int iwx_enable_mgmt_queue(iwx_softc*){return call(8);}
 int iwx_schedule_protect_session(iwx_softc*,iwx_node*,uint32_t){return call(9);}
 int iwx_mld_rm_sta(iwx_softc*,iwx_node*){return call(22);}
 unsigned ieee80211_chan2ieee(ieee80211com*,Channel*){return 100;}
};
'''
# flags used by actual narrow-command lookup belong to txq[].data.
pre=pre.replace('unsigned type=0;','unsigned type=0,flags=0;')
main=r'''
int main(){unsigned cases=0;using R=kgp_bz::ReplyRoute;
 {auto p=std::make_unique<ItlIwx>();p->oldP4Guard(&p->com,3,0);assert(p->fBzFault==3&&p->fBzError==EPROTO);++cases;}
 for(unsigned q:{0U,3U,31U,128U,255U})for(unsigned idx:{0U,127U,128U,255U}){auto storage=std::make_unique<ItlIwx>();auto &x=*storage;int c=IWX_TX_CMD;assert(x.bzRouteReply(&x.com,q,idx,&c,48)==R::PacketTx&&!x.fBzFault);++cases;}
 for(unsigned idx:{0U,127U}){auto storage=std::make_unique<ItlIwx>();auto &x=*storage;int c=0x207;assert(x.fBzCommands.begin(idx,c,1,1,false));assert(x.bzRouteReply(&x.com,0,idx,&c,12)==R::Command);++cases;}
 {auto storage=std::make_unique<ItlIwx>();auto &x=*storage;int c=0x104;x.com.txq[0].data[5].flags=IWX_TXDATA_FLAG_CMD_IS_NARROW;x.fBzCommands.begin(5,4,1,1,false);assert(x.bzRouteReply(&x.com,0,5,&c,12)==R::Command&&c==4);++cases;}
 for(unsigned mode=0;mode<5;++mode){auto storage=std::make_unique<ItlIwx>();auto &x=*storage;int c=0x207;unsigned q=mode==0?3:0,idx=mode==1?128:0;if(mode>=3)x.fBzCommands.begin(0,c,mode==3?2:1,mode==4?2:1,false);assert(x.bzRouteReply(&x.com,q,idx,&c,12)==R::Invalid&&x.fBzFault);++cases;}
 {auto storage=std::make_unique<ItlIwx>();auto &x=*storage;int c=0xc1;assert(x.bzRouteReply(&x.com,0x80,255,&c,64)==R::Notification&&!x.fBzFault);++cases;}
 for(unsigned which=0;which<9;++which){auto storage=std::make_unique<ItlIwx>();auto &x=*storage;iwx_rx_data d;auto*s=&x.com;s->sc_device_family=IWX_DEVICE_FAMILY_AX210;auto&r=s->txq[3];r.qid=3;r.ring_count=256;r.tail=255;r.queued=1;r.data[255].m=&x;x.fBzQueueOwned=1U<<3;
 iwx_rx_packet p{};p.hdr={IWX_TX_CMD,0,3,255};p.length=sizeof(iwx_tx_resp)+4;auto*t=(iwx_tx_resp*)p.data;t->frame_count=1;t->tx_queue=3;t->status.status=which==1?0x82:1;uint32_t ssn=256;std::memcpy(&t->status+1,&ssn,4);
 if(which==2)p.length=3;if(which==3)t->tx_queue=0;if(which==4)x.fBzQueueOwned=0;if(which==5)t->frame_count=0;if(which==6){ssn=258;std::memcpy(&t->status+1,&ssn,4);}if(which==7)r.queued=0;if(which==8){ssn=255;std::memcpy(&t->status+1,&ssn,4);}
 x.iwx_rx_tx_cmd(s,&p,&d);
 if(which<=1){assert(!x.fBzFault&&r.tail==0&&!r.queued&&x.retired==1&&x.freed==1&&x.cleared==1&&x.fBzStats[17]==1);assert(s->sc_ic.ic_if.stats.outputErrors==which);assert(x.fBzConnection.total==1);}else assert(x.fBzFault==23&&!x.retired&&!x.freed);
 ++cases;}
 {auto storage=std::make_unique<ItlIwx>();auto &x=*storage;iwx_node n;assert(!x.iwx_mld_auth(&x.com,&n));assert(x.sequence==std::vector<unsigned>({1,2,3,4,5,6,7,8,9}));assert(x.com.p.v["ItlwmMldAssocStep"]==7&&x.com.p.v["ItlwmMldAssocErr"]==0);assert(x.com.sc_flags&IWX_FLAG_MLD_TXQ_ACTIVE);++cases;}
 for(unsigned failure:{1U,3U,4U,6U,7U,8U,9U}){auto storage=std::make_unique<ItlIwx>();auto &x=*storage;iwx_node n;x.failAt=failure;assert(x.iwx_mld_auth(&x.com,&n)==EIO);assert(x.com.p.v["ItlwmMldAssocStep"]!=7&&x.com.p.v["ItlwmMldAssocErr"]==EIO);++cases;}
 {auto storage=std::make_unique<ItlIwx>();auto &x=*storage;uint8_t frame[30]={};frame[0]=0x80;x.bzObserveMgmt(frame,24);frame[0]=0x50;x.bzObserveMgmt(frame,24);assert(x.fBzScanBeacons==1&&x.fBzScanProbes==1&&!x.fBzConnection.total);
 frame[0]=0xb0;frame[26]=2;x.bzObserveMgmt(frame,30);frame[0]=0x10;frame[26]=17;x.bzObserveMgmt(frame,30);assert(x.fBzConnection.total==2);frame[1]=0x40;x.bzObserveMgmt(frame,30);x.bzObserveMgmt(frame,23);assert(x.fBzConnection.total==2);++cases;}
 {auto storage=std::make_unique<ItlIwx>();auto &x=*storage;for(unsigned i=0;i<200;++i)x.bzConnectionEvent(2,i,0,0);uint64_t wire[kgp_bz::ConnectionTrace::Words];x.fBzConnection.snapshot(wire);assert(wire[3]==200&&wire[4]==64&&wire[5]==136);x.gate.held=false;x.bzConnectionEvent(1,1,1,1);assert(x.fBzConnection.total==200);++cases;}
 std::cout<<"PASS "<<cases<<" production routing/TX-reclaim/auth/observer cases. Synthetic RX packets per pinned reference contract; saved P4 fixture separately verified. Mock hardware/KPIs,not association proof.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-connect-')as d:
 p=Path(d);(p/'test.cpp').write_text(pre.replace('using UInt32',constants+'\n'+wire+'\nusing UInt32',1)+functions+"void ItlIwx::oldP4Guard(iwx_softc*sc,unsigned qid,unsigned idx){"+oldguard+"}\n"+main)
 cmd=[C,'-isysroot',SDK,'-std=gnu++17','-O1','-g','-fsanitize='+san,'-I'+str(R),str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=90)
print('PASS saved P4 watchdog/properties: fault3/100,46packet publications/zero TX retirements,PNVM0x50001 accepted. Not rejected-packet bytes.')
