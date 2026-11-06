#!/usr/bin/env python3
"""Compile actual production rate methods and RX task loop with host stubs.
No firmware is loaded; this verifies arithmetic/state contracts, not RF operation.
"""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
reg=(root/'itlwm/hal_iwx/if_iwxreg.h').read_text()
def extract(text, marker):
    a=text.index(marker);b=text.index('{',a);level=1;i=b+1
    while level:
        level += (text[i]=='{')-(text[i]=='}');i+=1
    return text[a:i]
methods='\n'.join(extract(s,m) for m in ['static uint32_t\niwx_ax_he_tlc_rate_kbps','void ItlIwx::axTxRateNote','bool ItlIwx::getHeTxReport'])
defs=dict(re.findall(r'^#define\s+(IWX_RATE_\w+)\s+([^\n]+)',reg,re.M))
used=set(re.findall(r'IWX_RATE_\w+',methods));pending=list(used)
while pending:
    for d in re.findall(r'IWX_RATE_\w+',defs[pending.pop()]):
        if d not in used: used.add(d);pending.append(d)
defines='\n'.join('#define '+k+' '+defs[k] for k in sorted(used))
report=extract((root/'include/HAL/ItlDriverInfo.hpp').read_text(),'struct ItlHeTxReport')+';'
a=s.index('    const int rxGeneration',s.index('iwx_ba_task(void *arg)'))
b=s.index('    for (tid = 0;',s.index('    for (tid = 0;',a)+10)
loop=s[a:b]
old=(root/'tools/host_tests/reference/ax/1411f062ac02.cpp').read_text()
a0=old.index('    for (tid = 0;',old.index('iwx_ba_task(void *arg)')); b0=old.index('    for (tid = 0;',a0+10); oldloop=old[a0:b0]
ba_enum=extract(reg,'enum iwx_ba_resp_flags {')+';'
stat_enum=extract(s,'enum {\n    AXD_SINGLE')+';'
newhook=extract(s,'void ItlIwx::axHeDiagBa')
oldhook=extract((root/'tools/host_tests/reference/ax/da5bccf4b268.cpp').read_text(),'void ItlIwx::axHeDiagBa').replace('ItlIwx::axHeDiagBa','ItlIwx::oldAxHeDiagBa')
code=r'''
#include <cstdint>
#include <cassert>
#include <vector>
#include <functional>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define BIT(x) (1U<<(x))
#define IEEE80211_S_RUN 4
#define IEEE80211_NODE_HE 1
#define IEEE80211_BA_REQUESTED 1
#define IWX_FLAG_SHUTDOWN 1
#define IWX_MAX_TID_COUNT 8
#define ENXIO 6
#define XYLog(...) ((void)0)
void splx(int) {}
struct ieee80211_rx_ba { int ba_state=1; uint16_t ba_winstart=0,ba_winsize=64; int ba_timeout_val=0; };
struct ieee80211_node { int ni_flags=1; ieee80211_rx_ba ni_rx_ba[16]; };
struct ieee80211com { int ic_state=4; ieee80211_node *ic_bss=nullptr; };
struct iwx_softc { int sc_generation=1,ns_nstate=4,sc_flags=0; ieee80211com sc_ic;
struct { uint32_t start_tidmask=0,stop_tidmask=0; } ba_rx; };
int refused=0;
void ieee80211_addba_req_refuse(ieee80211com*,ieee80211_node*,int) { ++refused; }
'''+defines+'\n'+ba_enum+'\n'+stat_enum+'\n'+report+r'''
#define le16toh(x) (x)
#define le32toh(x) (x)
#define IWX_STATION_ID 0
struct iwx_compressed_ba_notif {uint32_t tx_rate=0,flags=0;uint8_t sta_id=0,retry_cnt=0;uint16_t txed=0,done=0,tfd_cnt=0;};
class ItlIwx {
public:
iwx_softc com; bool bz=false; uint32_t fPc1ResetEpoch=1,fAxRateEpoch=1,fAxBaEpoch=1;
uint64_t now=1000000000ULL;
struct AxRateSample { uint64_t seq=0,stamp=0,identity=0;uint32_t raw=0,epoch=0; } fAxRate[2];
bool bzDevice(){return bz;}
bool axHeDiagActive(iwx_softc*){return !bz;}
uint64_t time2GNow(){return now;}
uint64_t fAxHeDiagStats[88]={};
void axHeDiagBa(iwx_softc*,const iwx_compressed_ba_notif*);
void oldAxHeDiagBa(iwx_softc*,const iwx_compressed_ba_notif*);
void axTxRateNote(uint32_t,unsigned);
bool getHeTxReport(ItlHeTxReport&);
std::vector<int> calls;std::function<void(int)> hook; int stopErr=0;
int iwx_sta_rx_agg(iwx_softc*,ieee80211_node*,int tid,int,int,int,int start) {
    calls.push_back(start*100+tid); if(hook)hook(start);return start?0:stopErr;
}
void axBaEvent(int,int,int,int,int){}
void process(){auto *sc=&com;auto *ic=&sc->sc_ic;auto *ni=ic->ic_bss;auto *that=this;int tid,s=0;
'''+loop+r'''
}
void oldProcess(){auto *sc=&com;auto *ic=&sc->sc_ic;auto *ni=ic->ic_bss;auto *that=this;int tid,s=0;
'''+oldloop+r'''
}
};
'''+methods+'\n'+newhook+'\n'+oldhook+r'''
int main(){
  ieee80211_node n;ItlIwx d;d.com.sc_ic.ic_bss=&n;ItlHeTxReport r={};
  assert(d.getHeTxReport(r)&&!r.valid);
  iwx_compressed_ba_notif feedback;feedback.tx_rate=0x11dc1b;feedback.flags=IWX_BA_RESP_TX_BAR;feedback.done=64;feedback.txed=64;
  d.oldAxHeDiagBa(&d.com,&feedback);assert(d.getHeTxReport(r)&&!r.valid); // R6 drops valid BAR feedback
  d.axHeDiagBa(&d.com,&feedback);assert(d.getHeTxReport(r)&&r.valid&&r.source==2&&r.mcs==11);
  for(unsigned flag=0;flag<6;++flag){++d.fAxRateEpoch;feedback.flags=flag;d.axHeDiagBa(&d.com,&feedback);assert(d.getHeTxReport(r)&&r.valid==(flag<=1));}
  ++d.fAxRateEpoch;feedback.flags=1;feedback.sta_id=1;d.axHeDiagBa(&d.com,&feedback);assert(d.getHeTxReport(r)&&!r.valid);
  feedback.sta_id=0;feedback.done=0;d.axHeDiagBa(&d.com,&feedback);assert(d.getHeTxReport(r)&&!r.valid);feedback.done=64;
  d.bz=true;d.axHeDiagBa(&d.com,&feedback);assert(!d.getHeTxReport(r));d.bz=false;

  assert(iwx_ax_he_tlc_rate_kbps(0x0011dc1b)==2401960);
  assert(iwx_ax_he_tlc_rate_kbps(0x0031dc13)==490000);
  unsigned cases=0;
  for(unsigned m=0;m<12;++m)for(unsigned bw=0;bw<4;++bw)for(unsigned ns=0;ns<2;++ns)for(unsigned gi=0;gi<5;++gi){
    uint32_t raw=IWX_RATE_MCS_HE_MSK|m|(bw<<IWX_RATE_MCS_CHAN_WIDTH_POS)|(ns<<IWX_RATE_MCS_NSS_POS)|(gi<<IWX_RATE_MCS_HE_GI_LTF_POS);
    d.axTxRateNote(raw,2);assert(d.getHeTxReport(r)&&r.valid&&r.mcs==m&&r.bw==bw&&r.nss==ns+1&&r.gi==gi);++cases;
  }
  d.axTxRateNote(0x0031dc13,1);d.axTxRateNote(0x0011dc1b,2);d.axTxRateNote(0x0031dc13,1);
  assert(d.getHeTxReport(r)&&r.valid&&r.source==2&&r.mcs==11);
  d.now+=5000000001ULL;assert(d.getHeTxReport(r)&&r.valid&&!r.fresh&&r.mcs==11&&r.age_ms==5000);
  d.axTxRateNote(0x0031dc13,1);assert(d.getHeTxReport(r)&&r.valid&&r.fresh&&r.mcs==3&&r.source==1);
  ++d.fAxRateEpoch;assert(d.getHeTxReport(r)&&!r.valid);
  d.axTxRateNote(0x0011dc1b,2);++d.com.sc_generation;assert(d.getHeTxReport(r)&&!r.valid);
  d.axTxRateNote(0x0011dc1b,2);++d.fPc1ResetEpoch;assert(d.getHeTxReport(r)&&!r.valid);
  d.axTxRateNote(0x0011dc1b|(2U<<IWX_RATE_MCS_HE_TYPE_POS),2);assert(d.getHeTxReport(r)&&!r.valid);
  d.bz=true;assert(!d.getHeTxReport(r));d.bz=false;
  d.com.ba_rx.start_tidmask=d.com.ba_rx.stop_tidmask=1;d.oldProcess();
  assert((d.calls==std::vector<int>{100})&&d.com.ba_rx.stop_tidmask==1);d.calls.clear();
  // Coalesced stop+start must finish both, in order, in one task invocation.
  d.com.ba_rx.start_tidmask=d.com.ba_rx.stop_tidmask=1;d.process();assert((d.calls==std::vector<int>{0,100}));
  assert(!d.com.ba_rx.start_tidmask&&!d.com.ba_rx.stop_tidmask);
  // New work submitted while the synchronous command sleeps must survive.
  d.calls.clear();d.com.ba_rx.start_tidmask=1;d.hook=[&](int){d.com.ba_rx.start_tidmask|=1;};d.process();assert(d.com.ba_rx.start_tidmask==1);
  // Reset while waiting: don't consume work belonging to the next lifetime.
  d.calls.clear();d.hook=[&](int){++d.com.sc_generation;d.com.ba_rx.start_tidmask=3;};d.process();assert(d.calls.size()==1&&d.com.ba_rx.start_tidmask==3);
  // Failed remove must not be followed by a conflicting start.
  d.hook={};d.calls.clear();d.com.ba_rx.start_tidmask=d.com.ba_rx.stop_tidmask=1;d.stopErr=5;d.process();assert(d.calls==std::vector<int>{0}&&refused==1);
  // Not RUN: consume stale request without allocating a firmware session.
  d.calls.clear();d.stopErr=0;d.com.ns_nstate=0;d.com.ba_rx.start_tidmask=1;d.process();assert(d.calls.empty());
  printf("PASS %u HE tuples; coherent last-known/freshness/newest-source/lifetime/unknown contracts; RX stop-start, requeue, reset, failed-remove and RUN cases\n",cases);
}
'''
code='#include <cstdio>\n'+code
with tempfile.TemporaryDirectory(prefix='p10r6-host-') as tmp:
    path=Path(tmp);(path/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++14','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
