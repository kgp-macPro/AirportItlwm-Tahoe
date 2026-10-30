#!/usr/bin/env python3
"""Actual IWX setup fragment, host duration declaration and inactivity callback.
No firmware execution; tests do not establish BA-task/teardown hardware lifetime.
"""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2];BASE='53975f5ed87a6f7fb11d2129fef54660c6645ce7'
def extract(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 q=re.search(r'void\s+ItlIwx::\s*'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>
#include <sys/time.h>
#define XYLog(...) ((void)0)
#define container_of(p,t,m) reinterpret_cast<t*>(p)
constexpr unsigned IEEE80211_S_RUN=5,IWX_FLAG_SHUTDOWN=1,IWX_RX_REORDER_DATA_INVALID_BAID=127,IEEE80211_REASON_TIMEOUT=39;
#define RX_REORDER_BUF_TIMEOUT_MQ_USEC (100000ULL)
#define USEC_TO_TIMEVAL(u,t) do{(t)->tv_sec=(u)/1000000;(t)->tv_usec=(u)%1000000;}while(0)
thread_local uint64_t nowUs=0;
void getmicrouptime(timeval*t){USEC_TO_TIMEVAL(nowUs,t);}
int splnet(){return 1;}void splx(int){}
struct CTimeout{int ms=-1;unsigned arms=0;};
int timeout_add_msec(CTimeout**t,int n){assert(t&&*t&&n>=0);(*t)->ms=n;++(*t)->arms;return 1;}
int timeout_add_usec(CTimeout**t,int n){return timeout_add_msec(t,n/1000);}
struct ieee80211_rx_ba{int ba_timeout_val=0;};
struct ieee80211_node{ieee80211_rx_ba ni_rx_ba[8];unsigned refs=3;};
struct ieee80211com{unsigned ic_state=5;ieee80211_node*ic_bss=nullptr;struct{unsigned is_ht_rx_ba_timeout=0;}ic_stats;unsigned delba=0;};
struct iwx_softc{ieee80211com sc_ic;unsigned sc_flags=0;};
struct iwx_reorder_buffer{unsigned head_sn=0,buf_size=0;};
struct iwx_rxba_data{uint8_t sta_id=0,tid=0,baid=127; TIMEOUT_TYPE timeout=0;timeval last_rx{};CTimeout*session_timer=nullptr;iwx_softc*sc=nullptr;iwx_reorder_buffer reorder_buf;};
constexpr unsigned IWX_STATION_ID=0;
void ieee80211_delba_request(ieee80211com*ic,ieee80211_node*ni,int reason,int dir,int tid){assert(ni->refs==3&&reason==39&&dir==0&&tid<8);++ic->delba;}
struct ItlIwx{
 iwx_softc com;uint64_t fZc18Stats[12]={};
 static void iwx_rx_ba_session_expired(void*);
 void iwx_init_reorder_buffer(iwx_reorder_buffer*b,unsigned s,unsigned w){b->head_sn=s;b->buf_size=w;}
 void install(iwx_rxba_data*rxba,ieee80211_node*ni,int timeout_val,uint8_t tid=0,uint8_t baid=1,uint16_t ssn=4095,uint16_t winsize=64){ SETUP }
};
static_assert(offsetof(ItlIwx,com)==0,"mock container relation");
struct Fixture{ItlIwx d;ieee80211_node ni;CTimeout timer;iwx_rxba_data b;Fixture(){d.com.sc_ic.ic_bss=&ni;b.sc=&d.com;b.session_timer=&timer;}
 void setup(unsigned tu){nowUs=0;d.install(&b,&ni,tu*1024);}
 void fire(uint64_t us){nowUs=us;ItlIwx::iwx_rx_ba_session_expired(&b);}
};
'''
tests=r'''
int main(){
#ifdef BASELINE
 {Fixture f;f.setup(64);assert(f.b.timeout==0&&f.timer.ms==65);f.fire(65536);assert(!f.d.com.sc_ic.delba&&f.timer.ms==0);}
 {Fixture f;f.setup(1000);assert(f.b.timeout==40960);f.b.last_rx={0,800000};f.fire(1024000);assert(f.d.com.sc_ic.delba==1);}
 {Fixture f;f.setup(10);f.fire(10240);assert(!f.d.com.sc_ic.delba);}
 std::puts("BASELINE COUNTEREXAMPLES: 64 TU truncates to zero and rearms 0ms; 1000 TU session DELBA after 224ms idle; 10 TU true expiry ignored by unrelated 100ms comparison");
#else
 for(int family:{201,210,211})for(bool mapped:{false,true}){if(family==201&&mapped)continue;
  for(unsigned tu:{1u,10u,64u,100u,1000u,65535u}){
   Fixture f;f.setup(tu);uint64_t duration=uint64_t(tu)*1024;
   assert(f.b.timeout==duration&&f.timer.ms==int((duration+999)/1000));
   assert(f.ni.ni_rx_ba[0].ba_timeout_val==0&&f.b.reorder_buf.head_sn==4095&&f.b.reorder_buf.buf_size==64);
   f.fire(duration-1);assert(!f.d.com.sc_ic.delba&&f.timer.ms==1);f.fire(duration);assert(f.d.com.sc_ic.delba==1);
   assert(f.d.fZc18Stats[6]==1&&f.d.fZc18Stats[8]==2&&f.d.fZc18Stats[9]==1&&f.d.fZc18Stats[10]==1&&f.d.fZc18Stats[11]==duration);
  }
 }
 // Every legal ADDBA TU value survives setup, no wrap/truncation.
 {Fixture f;for(unsigned tu=0;tu<65536;++tu){unsigned arms=f.timer.arms;f.setup(tu);assert(f.b.timeout==tu*1024u);if(tu)assert(f.timer.ms==int((uint64_t(tu)*1024+999)/1000));else assert(f.timer.arms==arms);} }
 // RX refresh moves inactivity deadline, unlike a fixed timestamp since setup.
 {Fixture f;f.setup(1000);f.b.last_rx={0,800000};f.fire(1024000);assert(!f.d.com.sc_ic.delba&&f.timer.ms==800);f.fire(1823999);assert(!f.d.com.sc_ic.delba&&f.timer.ms==1);f.fire(1824000);assert(f.d.com.sc_ic.delba==1);}
 // Zero disabled, shutdown, not RUN and invalid BAID never arm or teardown.
 for(int condition=0;condition<4;++condition){Fixture f;f.setup(condition?10:0);if(condition==1)f.d.com.sc_flags=IWX_FLAG_SHUTDOWN;if(condition==2)f.d.com.sc_ic.ic_state=0;if(condition==3)f.b.baid=127;unsigned arms=f.timer.arms;f.fire(999999);assert(f.timer.arms==arms&&!f.d.com.sc_ic.delba);}
 // Setup after invalidation does not reuse last_rx/timeout from old session.
 {Fixture f;for(unsigned i=0;i<1000;++i){f.setup(64);f.b.baid=127;f.fire(1000000);assert(!f.d.com.sc_ic.delba);nowUs=2000000;f.d.install(&f.b,&f.ni,1000*1024,0,1);f.fire(2100000);assert(!f.d.com.sc_ic.delba&&f.timer.ms==924);}assert(f.ni.refs==3);}
 // Real threads: RX timestamp update and timer callback use the same modeled
 // main workloop gate. No newly shared per-session ownership or packet field.
 {Fixture f;f.setup(1000);f.b.last_rx={0,800000};std::mutex gate;std::thread rx([&]{for(unsigned i=0;i<10000;++i){std::lock_guard<std::mutex>l(gate);f.b.last_rx={0,800000};}});
  for(unsigned i=0;i<10000;++i){std::lock_guard<std::mutex>l(gate);f.fire(1024000);assert(!f.d.com.sc_ic.delba);}rx.join();}
 // BA task setup counters use actual relaxed atomics, independent of snapshot.
 {Fixture f;std::atomic<bool>done{false};std::thread task([&]{for(unsigned i=0;i<10000;++i)f.setup(i%2?64:0);done=true;});
  while(!done){auto n=__atomic_load_n(&f.d.fZc18Stats[6],__ATOMIC_RELAXED);auto z=__atomic_load_n(&f.d.fZc18Stats[7],__ATOMIC_RELAXED);auto t=__atomic_load_n(&f.d.fZc18Stats[11],__ATOMIC_RELAXED);assert(n<=5000&&z<=5000&&(t==0||t==65536));}task.join();}
 std::puts("PASS exact BA inactivity callback/setup/declaration: all 65536 TU values; zero/short/long/max, no early/zero rearm, RX refresh, state/invalidation/restart, main-gated callback concurrency and independent atomic setup observation. No firmware, radio or physical TCP proof.");
#endif
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc18-session-') as tmp:
 p=Path(tmp)
 for old in [True,False]:
  def read(n):return subprocess.check_output(['git','-C',str(R),'show',BASE+':'+n]).decode() if old else (R/n).read_text()
  s=read('itlwm/hal_iwx/ItlIwx.cpp');h=read('itlwm/hal_iwx/if_iwxvar.h')
  decl=h[h.index('struct iwx_rxba_data {'):];typ=re.search(r'(uint\d+_t)\s+timeout;',decl)[1]
  start=s.index('        rxba->sta_id = IWX_STATION_ID;');end=s.index('\n    } else {',start);setup=s[start:end]
  (p/'test.cpp').write_text(mock.replace('TIMEOUT_TYPE',typ).replace('SETUP',setup)+extract(s,'iwx_rx_ba_session_expired')+tests)
  cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fsanitize='+san]+(['-DBASELINE'] if old else [])+[str(p/'test.cpp'),'-o',str(p/'test')]
  print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120)
