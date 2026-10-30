#!/usr/bin/env python3
"""Extract IWX production reorder functions. Fake time/KPI/mbufs, not a throughput test."""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2];G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
SRC=R/'itlwm/hal_iwx/ItlIwx.cpp'
def extract(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 q=re.search(r'(?:void|int)\s+ItlIwx::\s*'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cerrno>
#include <sys/time.h>
#include <deque>
#include <vector>
#include <mutex>
#include <thread>
#include <iostream>
#include <algorithm>
#define MIN(a,b) std::min(a,b)
#define XYLog(...) ((void)0)
#define RX_REORDER_BUF_TIMEOUT_MQ_USEC (100000ULL)
#define USEC_TO_TIMEVAL(u,t) do{(t)->tv_sec=(u)/1000000;(t)->tv_usec=(u)%1000000;}while(0)
#define SEQ_LT(a,b) ((((a)-(b))&0xfff)>2048)
#define container_of(s,t,m) ((s)->owner)
constexpr unsigned IEEE80211_RXI_SAME_SEQ=1,IEEE80211_RXI_HWDEC_SAME_PN=2,IWX_RX_REORDER_DATA_INVALID_BAID=255;
using u_int=unsigned;
struct Frame{int id;};using mbuf_t=Frame*;struct mbuf_list{std::deque<mbuf_t>q;};
#define MBUF_LIST_INITIALIZER() {}
static uint64_t nowUs=0;static unsigned live=0;static std::vector<int>delivered;
static int splnet(){return 1;}static void splx(int){}
static void getmicrouptime(timeval*t){t->tv_sec=nowUs/1000000;t->tv_usec=nowUs%1000000;}
static bool ml_empty(mbuf_list*l){return l->q.empty();}
static mbuf_t ml_dequeue(mbuf_list*l){if(l->q.empty())return nullptr;auto*m=l->q.front();l->q.pop_front();return m;}
static void ml_purge(mbuf_list*l){while(auto*m=ml_dequeue(l)){delete m;--live;}}
struct CTimeout{bool pending=false;uint64_t due=0;};
static int timeout_pending(CTimeout**t){return *t&&(*t)->pending;}
static int timeout_add_msec(CTimeout**t,int ms){assert(ms>=0&&ms<=100);if(!*t)return 0;(*t)->pending=true;(*t)->due=nowUs+uint64_t(ms)*1000;return 1;}
static int timeout_add_usec(CTimeout**t,int us){return timeout_add_msec(t,us/1000);}
static int timeout_del(CTimeout**t){if(*t)(*t)->pending=false;return 1;}
static int timeout_free(CTimeout**t){delete *t;*t=nullptr;return 1;}
struct ieee80211_rxinfo{unsigned rxi_flags=0;};struct ieee80211_node{};
struct iwx_reorder_buffer{uint16_t head_sn=0,num_stored=0,buf_size=0,last_amsdu=0;uint8_t last_sub_index=0;CTimeout*reorder_timer=nullptr;timeval zc16_deadline{};int removed=0,valid=0;unsigned consec_oldsn_drops=0;uint32_t consec_oldsn_ampdu_gp2=0;int consec_oldsn_prev_drop=0;};
struct iwx_reorder_buf_entry{mbuf_list frames;timeval reorder_time{};uint32_t rx_pkt_status=0;int chanidx=0,is_shortpre=0;uint32_t rate_n_flags=0,device_timestamp=0;ieee80211_rxinfo rxi;};
struct iwx_softc;
struct iwx_rxba_data{iwx_softc*sc=nullptr;iwx_reorder_buffer reorder_buf;iwx_reorder_buf_entry entries[64];timeval last_rx{};CTimeout*session_timer=nullptr;int baid=0;};
static iwx_rxba_data*iwx_rxba_data_from_reorder_buf(iwx_reorder_buffer*b){return reinterpret_cast<iwx_rxba_data*>(reinterpret_cast<char*>(b)-offsetof(iwx_rxba_data,reorder_buf));}
struct _ifnet{};struct ieee80211com{ieee80211_node node;ieee80211_node*ic_bss=&node;_ifnet ic_if;struct{unsigned is_ht_rx_ba_window_gap_timeout=0;}ic_stats;};
struct ItlIwx;struct iwx_softc{ItlIwx*owner;ieee80211com sc_ic;};
static void if_input(_ifnet*,mbuf_list*l){while(auto*m=ml_dequeue(l)){delivered.push_back(m->id);delete m;--live;}}
struct ItlIwx{iwx_softc com;ItlIwx(){com.owner=this;}
 void iwx_init_reorder_buffer(iwx_reorder_buffer*,uint16_t,uint16_t);
 void iwx_clear_reorder_buffer(iwx_softc*,iwx_rxba_data*);
 static void iwx_reorder_timer_expired(void*);
 int iwx_is_sn_less(uint16_t,uint16_t,uint16_t);
 void iwx_release_frames(iwx_softc*,ieee80211_node*,iwx_rxba_data*,iwx_reorder_buffer*,uint16_t,mbuf_list*);
 void zc16ArmReorder(iwx_rxba_data*);
 void iwx_rx_frame(iwx_softc*,mbuf_t m,int,uint32_t,int,int,uint32_t,ieee80211_rxinfo*,mbuf_list*l){l->q.push_back(m);}
};
struct Fixture{ItlIwx hal;iwx_rxba_data ba;Fixture(unsigned head=0){nowUs=0;delivered.clear();assert(live==0);ba.sc=&hal.com;ba.reorder_buf.reorder_timer=new CTimeout;hal.iwx_init_reorder_buffer(&ba.reorder_buf,head,64);}
 ~Fixture(){hal.iwx_clear_reorder_buffer(&hal.com,&ba);assert(!ba.reorder_buf.reorder_timer&&live==0);}
 void store(unsigned sn,int id){auto&e=ba.entries[sn%64];e.frames.q.push_back(new Frame{id});++live;++ba.reorder_buf.num_stored;getmicrouptime(&e.reorder_time);}
 void release(unsigned nssn){mbuf_list ml;hal.iwx_release_frames(&hal.com,hal.com.sc_ic.ic_bss,&ba,&ba.reorder_buf,nssn,&ml);if_input(&hal.com.sc_ic.ic_if,&ml);}
 void fire(uint64_t t){nowUs=t;auto*q=ba.reorder_buf.reorder_timer;if(q&&q->pending&&q->due<=t){q->pending=false;hal.iwx_reorder_timer_expired(&ba.reorder_buf);}}
};
'''
test=r'''
int main(){
 {Fixture f;f.store(2,2);f.release(0);for(int t=10;t<=90;t+=10){nowUs=t*1000;f.release(0);}
#ifdef BASELINE
  assert(f.ba.reorder_buf.reorder_timer->due==190000);f.fire(100000);assert(delivered.empty());std::cout<<"BASELINE COUNTEREXAMPLE: same frame timer postponed from 100 to 190 ms by non-advancing NSSN\n";
#else
  assert(f.ba.reorder_buf.reorder_timer->due==100000);f.fire(99999);assert(delivered.empty());f.fire(100000);assert((delivered==std::vector<int>{2})&&f.ba.reorder_buf.num_stored==0);
#endif
 }
#ifndef BASELINE
 // Normal contiguous release, removal, no early expiry, 12-bit wrap.
 for(unsigned head:{0U,4094U}){Fixture f(head);f.store((head+1)&0xfff,1);f.store((head+2)&0xfff,2);f.release((head+3)&0xfff);assert((delivered==std::vector<int>{1,2}));assert(!timeout_pending(&f.ba.reorder_buf.reorder_timer));}
 // Old pending timer is kept, but a later timestamp (A-MSDU last subframe)
 // is respected by the existing expiry predicate, never forced out early.
 {Fixture f;f.store(2,20);f.release(0);nowUs=90000;f.store(2,21);f.release(0);f.fire(100000);assert(delivered.empty());assert(f.ba.reorder_buf.reorder_timer->due==190000);f.fire(190000);assert((delivered==std::vector<int>{20,21}));}
 // An earlier sequence may arrive later. Preserve sequence release ordering.
 {Fixture f;f.store(2,2);f.release(0);nowUs=90000;f.store(1,1);f.release(0);f.fire(100000);assert(delivered.empty());f.fire(190000);assert((delivered==std::vector<int>{1,2}));}
 // Sub-ms remainder rounds UP; overdue uses 1 ms, repeated events cannot push it.
 {Fixture f;f.store(2,2);nowUs=99501;f.release(0);assert(f.ba.reorder_buf.reorder_timer->due==100501);f.fire(100501);assert(delivered.size()==1);}
 {Fixture f;f.store(2,2);nowUs=200000;f.release(0);assert(f.ba.reorder_buf.reorder_timer->due==201000);nowUs=200100;f.release(0);assert(f.ba.reorder_buf.reorder_timer->due==201000);f.fire(201000);assert(delivered.size()==1);}
 // Removed/empty/clear and reinitialization leave no timer or old packet alive.
 {Fixture f;f.store(2,2);f.release(0);f.ba.reorder_buf.removed=1;f.hal.zc16ArmReorder(&f.ba);assert(!timeout_pending(&f.ba.reorder_buf.reorder_timer));}
 for(int cycle=0;cycle<1000;++cycle){Fixture f(cycle%4096);f.store((cycle+2)%4096,2);f.release(cycle%4096);f.hal.iwx_clear_reorder_buffer(&f.hal.com,&f.ba);assert(live==0);f.ba.reorder_buf.reorder_timer=new CTimeout;f.hal.iwx_init_reorder_buffer(&f.ba.reorder_buf,0,64);f.store(1,1);f.release(2);assert(delivered.back()==1);}
 // Source timer and notification actions interleave under a mock of the SAME
 // gate, not under invented lock-free access. TSan checks new deadline state.
 {Fixture f;std::mutex gate;f.store(2,2);f.release(0);std::thread t([&]{for(int i=0;i<2000;++i){std::lock_guard<std::mutex>l(gate);f.release(f.ba.reorder_buf.head_sn);}});for(int i=0;i<200;++i){std::lock_guard<std::mutex>l(gate);f.fire(i*1000);}t.join();assert(delivered.size()==1);}
 std::cout<<"CURRENT SOURCE PASS: absolute age, non-postponement, AMSDU, wrap, deadline rounding, teardown/rearm, gated concurrency; mock timers/mbufs, NOT runtime TCP or DMA proof\n";
#endif
}
'''
names=['iwx_init_reorder_buffer','iwx_clear_reorder_buffer','iwx_reorder_timer_expired','iwx_is_sn_less','iwx_release_frames']
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc16-reorder-') as t:
 p=Path(t)
 for baseline in [True,False]:
  src=subprocess.check_output([G,'-C',str(R),'show','f2a02dcd7cd0f44c8ec2804428103beccca4d2b0:itlwm/hal_iwx/ItlIwx.cpp']).decode() if baseline else SRC.read_text()
  body=mock+'\n'.join(extract(src,n) for n in names+([] if baseline else ['zc16ArmReorder']))+test
  f=p/('old.cpp' if baseline else 'current.cpp');f.write_text(body)
  cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-fsanitize='+san]+(['-DBASELINE'] if baseline else [])+[str(f),'-o',str(p/'run')]
  print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'run')],check=True,timeout=60)
