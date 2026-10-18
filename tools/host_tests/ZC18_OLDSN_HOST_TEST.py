#!/usr/bin/env python3
"""Extract real old-SN detector and initializer; prove separated-group bug.
No network, throughput, firmware or physical DMA simulation is represented.
"""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2]
BASE='53975f5ed87a6f7fb11d2129fef54660c6645ce7'
def extract(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 q=re.search(r'(?:int|void)\s+ItlIwx::\s*'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
header=(R/'itlwm/hal_iwx/if_iwxvar.h').read_text()
structure=header[header.index('struct iwx_reorder_buffer {'):header.index('\n};',header.index('struct iwx_reorder_buffer {'))+3]
reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()
flag=re.search(r'#define\s+IWX_RX_MPDU_REORDER_BA_OLD_SN\s+[^\n]+',reg)[0]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <atomic>
#include <sys/time.h>
struct CTimeout{};
struct ieee80211com {unsigned delba=0;};
struct ieee80211_node {unsigned refs=3;};
struct iwx_softc{ieee80211com sc_ic;};
constexpr unsigned IEEE80211_REASON_UNSPECIFIED=1;
#define XYLog(...) ((void)0)
void ieee80211_delba_request(ieee80211com*ic,ieee80211_node*ni,int reason,int dir,int tid){assert(ni->refs==3&&reason==1&&dir==0&&tid>=0&&tid<8);++ic->delba;}
'''+structure+'\n'+flag+r'''
struct ItlIwx{
 uint64_t fZc18Stats[6]={};
 void iwx_init_reorder_buffer(iwx_reorder_buffer*,uint16_t,uint16_t);
 int iwx_oldsn_workaround(iwx_softc*,ieee80211_node*,int,iwx_reorder_buffer*,uint32_t,uint32_t);
};
'''
tests=r'''
int main(){
 // Counterexample: 20 isolated OLD_SN groups with clean groups in between.
 // ZC17 tears down RX BA despite not a single adjacent pair of bad groups.
 {ItlIwx d;iwx_softc sc;ieee80211_node ni;iwx_reorder_buffer b{};d.iwx_init_reorder_buffer(&b,4095,64);
  unsigned fired=0;for(unsigned i=0;i<20;++i){fired+=d.iwx_oldsn_workaround(&sc,&ni,0,&b,IWX_RX_MPDU_REORDER_BA_OLD_SN,2*i+1);d.iwx_oldsn_workaround(&sc,&ni,0,&b,0,2*i+2);}
#ifdef BASELINE
  assert(fired==1&&sc.sc_ic.delba==1);std::puts("BASELINE COUNTEREXAMPLE: 20 nonconsecutive bad groups trigger DELBA (reproduced from frozen source)");return 0;
#else
  assert(!fired&&!sc.sc_ic.delba&&d.fZc18Stats[1]==20&&d.fZc18Stats[2]==19&&d.fZc18Stats[5]==1);
#endif
 }
#ifndef BASELINE
 // Both qualified DMA modes share this exact function; there is no family or
 // mapper branch here. Hardware applicability is also checked in source gate.
 for(int family:{201,210,211})for(bool mapped:{false,true}){
  if(family==201&&mapped)continue;
  ItlIwx d;iwx_softc sc;ieee80211_node ni;iwx_reorder_buffer b{};d.iwx_init_reorder_buffer(&b,4095,64);
  for(unsigned g=1;g<=20;++g){
   // First frame clean, later subframe OLD_SN: must count this same group.
   assert(!d.iwx_oldsn_workaround(&sc,&ni,3,&b,0,g));
   assert(d.iwx_oldsn_workaround(&sc,&ni,3,&b,IWX_RX_MPDU_REORDER_BA_OLD_SN,g)==int(g==20));
   if(g<20){assert(!d.iwx_oldsn_workaround(&sc,&ni,3,&b,IWX_RX_MPDU_REORDER_BA_OLD_SN,g));assert(b.consec_oldsn_drops==g);}
   assert(b.head_sn==4095&&b.buf_size==64&&b.num_stored==0&&ni.refs==3);
  }
  assert(sc.sc_ic.delba==1&&d.fZc18Stats[3]==1&&d.fZc18Stats[5]==20&&d.fZc18Stats[1]==20);
 }
 // Thousands of repeated same-group notifications do not inflate the streak.
 {ItlIwx d;iwx_softc s;ieee80211_node n;iwx_reorder_buffer b{};d.iwx_init_reorder_buffer(&b,0,64);
  for(int i=0;i<10000;++i)assert(!d.iwx_oldsn_workaround(&s,&n,0,&b,IWX_RX_MPDU_REORDER_BA_OLD_SN,0));
  assert(b.consec_oldsn_drops==1&&d.fZc18Stats[1]==1&&d.fZc18Stats[4]==9999);
 }
 // GP2 wrap is identity-based, not a host-time subtraction or changed timer.
 {ItlIwx d;iwx_softc s;ieee80211_node n;iwx_reorder_buffer b{};d.iwx_init_reorder_buffer(&b,4095,256);
  for(uint32_t g:{0xfffffffeu,0xffffffffu,0u,1u})assert(!d.iwx_oldsn_workaround(&s,&n,0,&b,IWX_RX_MPDU_REORDER_BA_OLD_SN,g));
  assert(b.consec_oldsn_drops==4&&b.head_sn==4095);d.iwx_oldsn_workaround(&s,&n,0,&b,0,2);d.iwx_oldsn_workaround(&s,&n,0,&b,0,3);assert(!b.consec_oldsn_drops);
 }
 // Independent state oracle: arbitrary group membership; multiple TIDs/BAIDs,
 // real reset initialization, threshold callback and clean group transitions.
 {ItlIwx d;iwx_softc s;ieee80211_node n;iwx_reorder_buffer b[8]{};
  unsigned streak[8]={};uint32_t seed=0x187f;unsigned expected=0;
  for(int t=0;t<8;++t)d.iwx_init_reorder_buffer(&b[t],t*511,64);
  for(unsigned g=1;g<30000;++g){seed=seed*1664525u+1013904223u;unsigned t=(seed>>8)%8;bool bad=(seed%23)!=0;
   if(g%107==0){d.iwx_init_reorder_buffer(&b[t],g%4096,64);streak[t]=0;}
   bool shouldFire=bad&&++streak[t]==20;if(!bad)streak[t]=0;
   if(shouldFire){++expected;streak[t]=0;}
   assert(!d.iwx_oldsn_workaround(&s,&n,t,&b[t],0,g));
   int got=d.iwx_oldsn_workaround(&s,&n,t,&b[t],bad?IWX_RX_MPDU_REORDER_BA_OLD_SN:0,g);
   assert(got==int(shouldFire));
   if(shouldFire)d.iwx_init_reorder_buffer(&b[t],g%4096,64);
  }assert(s.sc_ic.delba==expected&&n.refs==3);
 }
 // TSan: real new counters accessed by notification producer and snapshot
 // consumer under the same modeled main gate, like existing publication.
 {ItlIwx d;iwx_softc s;ieee80211_node n;iwx_reorder_buffer b{};d.iwx_init_reorder_buffer(&b,0,64);std::mutex gate;std::atomic<bool> done{false};
  std::thread notifier([&]{for(unsigned g=1;g<=20000;++g){std::lock_guard<std::mutex>l(gate);d.iwx_oldsn_workaround(&s,&n,0,&b,(g%2)?IWX_RX_MPDU_REORDER_BA_OLD_SN:0,g);if(g%1000==0)d.iwx_init_reorder_buffer(&b,g%4096,64);}done=true;});
  while(!done){std::lock_guard<std::mutex>l(gate);uint64_t snapshot[6];std::memcpy(snapshot,d.fZc18Stats,sizeof(snapshot));assert(snapshot[3]==0&&snapshot[5]<=1);}
  notifier.join();assert(!s.sc_ic.delba&&n.refs==3);
 }
 std::puts("PASS exact extracted ZC18 function: nonconsecutive false DELBA removed; 20 consecutive groups still recover; first-clean/later-old group, repeated subframes, 32-bit wrap, TID isolation, repeated reset, unchanged RX window/ownership and gated snapshot concurrency. No physical performance claim.");
#endif
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc18-oldsn-') as tmp:
 p=Path(tmp)
 for old in [True,False]:
  s=subprocess.check_output(['git','-C',str(R),'show',BASE+':itlwm/hal_iwx/ItlIwx.cpp']).decode() if old else (R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
  (p/'test.cpp').write_text(mock+'\n'.join(extract(s,n) for n in ['iwx_init_reorder_buffer','iwx_oldsn_workaround'])+tests)
  cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-pthread','-fsanitize='+san]+(['-DBASELINE'] if old else [])+[str(p/'test.cpp'),'-o',str(p/'test')]
  print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120)
