#!/usr/bin/env python3
"""Extract real output/start/low-water bodies and compare them with frozen BOOTFIX1.
Mocks prove observable control/ownership under specified inputs, not IOKit timing or DMA.
"""
from pathlib import Path
import re,subprocess,tempfile,sys
W=Path(__file__).resolve().parents[2]
BASE='bc8b39bb6ceede8522f5e1c8126d078dd10aac64'
def before(path):return subprocess.check_output(['git','-C',str(W),'show',BASE+':'+path]).decode()
def extract(s,cls,name):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(re.escape(cls)+r'::\s*'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[a:b]
A='AirportItlwm/AirportItlwm.cpp';C='itlwm/hal_iwx/ItlIwx.cpp'
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include "Perf1.hpp"
using IOReturn=int;using UInt32=uint32_t;using IOOptionBits=uint32_t;
constexpr int kIOReturnSuccess=0,kIOReturnNoResources=1,kIOReturnNoFrames=2,kIOReturnError=3,kIOReturnOutputSuccess=0,kIOReturnOutputDropped=4;
constexpr int IEEE80211_S_RUN=4,MBUF_PKTHDR=1,MBUF_TYPE_FREE=2,IFF_RUNNING=1,IFF_UP=2,EDCA_AC_BE=0,IWX_FLAG_TXFLUSH=8,IEEE80211_F_TX_MGMT_ONLY=16;
#define XYLog(...) ((void)0)
#define AIRPORT 1
#define __PRIVATE_SPI__ 1
#define NBPFILTER 0
'''
outmock=r'''
std::vector<std::string> trace;
struct Packet {int id=0;size_t bytes=1200;bool freed=false;};using mbuf_t=Packet*;
int mbuf_type(mbuf_t p){assert(p);return p->freed?MBUF_TYPE_FREE:1;}int mbuf_flags(mbuf_t p){assert(p&&!p->freed);return MBUF_PKTHDR;}size_t mbuf_pkthdr_len(mbuf_t p){assert(p&&!p->freed);return p->bytes;}
struct Queue{std::deque<mbuf_t> q;unsigned capacity=4;bool lockEnqueue(mbuf_t p){assert(p&&!p->freed);if(q.size()>=capacity){trace.push_back("refuse"+std::to_string(p->id));return false;}q.push_back(p);trace.push_back("accept"+std::to_string(p->id));return true;}};
struct _ifnet{struct{Queue*queue=nullptr;bool active=false;}if_snd;struct Stat{unsigned outputErrors=0;}stats,*netStat=&stats;void(*if_start)(_ifnet*)=nullptr;int drain=0,after=0,calls=0;};
int ifq_is_oactive(decltype(_ifnet::if_snd)*q){return q->active;}
struct IC{struct{_ifnet ac_if;}ic_ac;int ic_state=IEEE80211_S_RUN;};
struct HAL{IC ic;KgpPerf1 perf;IC*get80211Controller(){return &ic;}KgpPerf1*getPerf1(){return &perf;}};
void start(_ifnet*p){trace.push_back("start");++p->calls;if(p->drain&&p->if_snd.queue&&!p->if_snd.queue->q.empty()){auto*m=p->if_snd.queue->q.front();p->if_snd.queue->q.pop_front();assert(!m->freed);m->freed=true;trace.push_back("consume"+std::to_string(m->id));}if(p->after&&p->calls>=p->after)p->if_snd.active=true;}
struct IONetworkInterface{std::deque<mbuf_t> packets;int terminal=kIOReturnNoFrames;IOReturn dequeueOutputPackets(int n,mbuf_t*m){assert(n==1);trace.push_back("dequeue");if(packets.empty())return terminal;*m=packets.front();packets.pop_front();return 0;}};
struct AirportItlwm{HAL*fHalService;explicit AirportItlwm(HAL*h):fHalService(h){}void freePacket(mbuf_t m){assert(m&&!m->freed);m->freed=true;trace.push_back("free"+std::to_string(m->id));}IOReturn outputStart(IONetworkInterface*,IOOptionBits);UInt32 outputPacket(mbuf_t,void*);};
'''
outtest=r'''
std::vector<std::string> run(unsigned cfg){trace.clear();HAL h;Queue q;auto&ifp=h.ic.ic_ac.ac_if;ifp.if_snd.queue=&q;ifp.if_start=start;q.capacity=cfg%5;ifp.drain=(cfg/5)%2;ifp.after=(cfg/10)%4;ifp.if_snd.active=(cfg/40)%2;h.ic.ic_state=(cfg/80)%2?0:IEEE80211_S_RUN;
 IONetworkInterface n;n.terminal=(cfg/160)%2?kIOReturnError:kIOReturnNoFrames;std::vector<Packet>p(20);for(unsigned i=0;i<p.size();++i){p[i].id=i;n.packets.push_back(&p[i]);}
 AirportItlwm d(&h);int r=d.outputStart(&n,0);trace.push_back("return"+std::to_string(r));trace.push_back("remaining"+std::to_string(n.packets.size()));trace.push_back("software"+std::to_string(q.q.size()));
 for(auto&m:p)trace.push_back(std::to_string(m.id)+":"+std::to_string(m.freed));
 #ifdef AFTER
 assert(h.perf.read(KgpPerf1::MacDequeued)==h.perf.read(KgpPerf1::OutputPacketCalls));
 assert(h.perf.read(KgpPerf1::OutputPacketCalls)==h.perf.read(KgpPerf1::OutputSuccess)+h.perf.read(KgpPerf1::OutputDropped));
 assert(h.perf.read(KgpPerf1::OutputDropped)==h.perf.read(KgpPerf1::SoftwareRefused)+h.perf.read(KgpPerf1::OutputNotRun));
 #endif
 return trace;}
'''
halmock=r'''
std::vector<std::string> trace;
struct Packet{int id=0;size_t bytes=1200;bool freed=false;int fail=0;uint8_t fc=8;};using mbuf_t=Packet*;
struct ieee80211_node{int id=0;};struct ether_header{char bytes[14];};
size_t mbuf_len(mbuf_t m){assert(m&&!m->freed);return m->fail==1?1:m->bytes;}size_t mbuf_pkthdr_len(mbuf_t m){assert(m&&!m->freed);return m->bytes;}void*mbuf_data(mbuf_t m){assert(m&&!m->freed);return &m->fc;}
int mbuf_pullup(mbuf_t*m,size_t){assert(*m&&!(*m)->freed);(*m)->freed=true;trace.push_back("pullupdrop");return 1;}
void*mbuf_pkthdr_rcvif(mbuf_t m){return reinterpret_cast<void*>(uintptr_t(m->id+1));}
struct Queue{std::deque<mbuf_t>q;bool active=false;};
struct Interface{void signalOutputThread(){trace.push_back("signal");}};
struct _ifnet{void*if_softc=nullptr;Queue if_snd;int if_flags=IFF_RUNNING|IFF_UP,if_timer=0;struct Stat{unsigned outputErrors=0,outputPackets=0;}stats,*netStat=&stats;Interface interface,*iface=&interface;void(*if_start)(_ifnet*)=nullptr;};
unsigned ifq_is_oactive(Queue*q){return q->active;}void ifq_set_oactive(Queue*q){q->active=true;trace.push_back("active");}void ifq_clr_oactive(Queue*q){q->active=false;trace.push_back("clear");}mbuf_t ifq_dequeue(Queue*q){trace.push_back("dequeue");if(q->q.empty())return nullptr;auto m=q->q.front();q->q.pop_front();return m;}mbuf_t mq_dequeue(Queue*q){if(q->q.empty())return nullptr;auto m=q->q.front();q->q.pop_front();return m;}
struct ieee80211com{_ifnet ic_if;Queue ic_mgtq;int ic_xflags=0,ic_state=IEEE80211_S_RUN;};
struct iwx_softc{void*owner;ieee80211com sc_ic;unsigned qfullmsk=0,sc_flags=0;int sc_tx_timer=0;};
struct iwx_tx_ring{int queued=0,low_mark=3,qid=0;};struct OSObject{};
#define container_of(p,T,member) static_cast<T*>((p)->owner)
mbuf_t ieee80211_encap(_ifnet*,mbuf_t m,ieee80211_node**n){assert(m&&!m->freed);if(m->fail==2){m->freed=true;trace.push_back("encapdrop");return nullptr;}*n=reinterpret_cast<ieee80211_node*>(uintptr_t(m->id+1));return m;}
void ieee80211_release_node(ieee80211com*,ieee80211_node*){trace.push_back("node-release");}
struct ItlIwx{iwx_softc com;KgpPerf1 fPerf1;bool mapped=false;struct Start2LContext{bool entered=false;uint32_t reason=0;};bool dma5Mapped(){return mapped;}void path2L(unsigned,unsigned=0,unsigned=0,uint8_t=0){}
 static IOReturn _iwx_start_task(OSObject*,void*,void*,void*,void*);static IOReturn dexterOriginal__iwx_start_task(OSObject*,void*,void*,void*,void*);void iwx_clear_oactive(iwx_softc*,iwx_tx_ring*);
 int iwx_tx(iwx_softc*,mbuf_t m,ieee80211_node*,int){assert(m&&!m->freed);trace.push_back("submit"+std::to_string(m->id));m->freed=true;return m->fail==3?1:0;}};
void restart(_ifnet*){trace.push_back("restart");}
'''
haltest=r'''
std::vector<std::string> run(unsigned cfg){trace.clear();ItlIwx d;d.com.owner=&d;auto&ic=d.com.sc_ic;auto&f=ic.ic_if;f.if_softc=&d.com;d.mapped=cfg%2;f.if_snd.active=(cfg/2)%2;f.if_flags=(cfg/4)%2?0:IFF_RUNNING|IFF_UP;d.com.qfullmsk=(cfg/8)%2;d.com.sc_flags=(cfg/16)%2?IWX_FLAG_TXFLUSH:0;ic.ic_xflags=(cfg/32)%2?IEEE80211_F_TX_MGMT_ONLY:0;
 std::vector<Packet>p(20);for(unsigned i=0;i<p.size();++i){p[i].id=i;p[i].fail=i%4;f.if_snd.q.push_back(&p[i]);}ItlIwx::Start2LContext ctx;auto r=ItlIwx::_iwx_start_task(nullptr,&f,&ctx,nullptr,nullptr);
 trace.push_back("return"+std::to_string(r));trace.push_back("packets"+std::to_string(f.stats.outputPackets));trace.push_back("errors"+std::to_string(f.stats.outputErrors));for(auto&m:p)trace.push_back(std::to_string(m.id)+":"+std::to_string(m.freed));
 #ifdef AFTER
 assert(d.fPerf1.read(KgpPerf1::DataDequeued)==d.fPerf1.read(KgpPerf1::PullupDrop)+d.fPerf1.read(KgpPerf1::EncapDrop)+d.fPerf1.read(KgpPerf1::TxAttempts));
 assert(d.fPerf1.read(KgpPerf1::TxAttempts)==d.fPerf1.read(KgpPerf1::TxErrors)+d.fPerf1.read(KgpPerf1::OutputPacketsIncrement));
 #endif
 return trace;}
std::vector<std::string> low(unsigned cfg){trace.clear();ItlIwx d;d.com.owner=&d;auto&f=d.com.sc_ic.ic_if;f.if_start=restart;f.if_snd.active=cfg%2;d.com.qfullmsk=(cfg/2)%4;iwx_tx_ring r;r.queued=(cfg/8)%5;d.iwx_clear_oactive(&d.com,&r);trace.push_back("qfull"+std::to_string(d.com.qfullmsk));trace.push_back("active"+std::to_string(f.if_snd.active));return trace;}
'''
code=pre
for after in (False,True):
 ns='After' if after else 'Before';a=(W/A).read_text() if after else before(A);c=(W/C).read_text() if after else before(C)
 code+=('\n#define AFTER\n' if after else '\n#undef AFTER\n')+'namespace Out'+ns+'{\n'+outmock
 for name,decl in [('outputStart','IOReturn AirportItlwm::outputStart(IONetworkInterface*interface,IOOptionBits options)'),('outputPacket','UInt32 AirportItlwm::outputPacket(mbuf_t m,void*param)')]:code+=decl+extract(a,'AirportItlwm',name)+'\n'
 code+=outtest+'}\nnamespace Hal'+ns+'{\n'+halmock
 for name in ['_iwx_start_task','dexterOriginal__iwx_start_task']:
  code+='IOReturn ItlIwx::'+name+'(OSObject*target,void*arg0,void*arg1,void*arg2,void*arg3)'+extract(c,'ItlIwx',name)+'\n'
 code+='void ItlIwx::iwx_clear_oactive(iwx_softc*sc,iwx_tx_ring*ring)'+extract(c,'ItlIwx','iwx_clear_oactive')+'\n'+haltest+'}\n'
code+=r'''
int main(){
 for(unsigned i=0;i<320;++i)assert(OutBefore::run(i)==OutAfter::run(i));
 for(unsigned i=0;i<64;++i)assert(HalBefore::run(i)==HalAfter::run(i));
 for(unsigned i=0;i<40;++i)assert(HalBefore::low(i)==HalAfter::low(i));
 KgpPerf1 p;std::atomic<bool>go{true};std::thread observer([&]{uint64_t s[KgpPerf1::Count+KgpPerf1::Queues*KgpPerf1::QueueWords];while(go.load()){p.snapshot(s);}});
 std::vector<std::thread> producers;
 for(unsigned t=0;t<8;++t)producers.emplace_back([&,t]{for(unsigned i=0;i<100000;++i){p.add(KgpPerf1::MacDequeued);p.queue(t,0);p.occupancy(t,256,i%226);}});
 for(auto&t:producers)t.join();go=false;observer.join();assert(p.read(KgpPerf1::MacDequeued)==800000);
 uint64_t s[KgpPerf1::Count+KgpPerf1::Queues*KgpPerf1::QueueWords];p.snapshot(s);
 for(unsigned t=0;t<8;++t){assert(s[KgpPerf1::Count+t*6]==100000);assert(s[KgpPerf1::Count+t*6+3]==225);}
 p.queue(999,0);p.occupancy(999,256,5); // defensive observer bounds; no ownership mutation
 std::cout<<"PASS 320 extracted output ownership/return traces; 64 extracted start-task traces across both modes; 40 low-water/restart traces; 800000 concurrent counter events with racing snapshots. No timing/DMA/physical proof.\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc6-perf1-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-I'+str(W/'include/HAL'),'-std=c++17','-O1','-g','-pthread','-fno-omit-frame-pointer','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('HOST COMMAND:',' '.join(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120)
