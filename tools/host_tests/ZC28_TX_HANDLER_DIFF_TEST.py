#!/usr/bin/env python3
"""Differential actual TX handlers vs frozen ZC26; diagnostic helpers mocked.

This validates packet/retire/error/restart behavior and removal of formatting
from the selected handlers. It does not prove firmware aggregate-reply reachability
or the production diagnostic helper's own serialization/schema correctness.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
root=Path(os.environ.get('ZC28_ROOT',Path(__file__).resolve().parents[2]))
parser.add_argument('sanitizer',nargs='?',default='address,undefined')
parser.add_argument('--source',type=Path,default=root/'itlwm/hal_iwx/ItlIwx.cpp')
args=parser.parse_args()
new=args.source.read_text()
old=subprocess.check_output(['git','-C',str(args.source.parents[2]),'show',
    '48e9a31e68c328b5f19e5bdc81901e900d350738:itlwm/hal_iwx/ItlIwx.cpp'],text=True)
def function(src,name):
    masked=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),src,flags=re.S)
    m=re.search(r'void\s+ItlIwx::\s*'+name+r'\s*\(',masked)
    assert m,name
    a=masked.index('{',m.end());b=a+1;depth=1
    while depth:
        depth+=(masked[b]=='{')-(masked[b]=='}');b+=1
    return src[m.start():b]
names=('iwx_rx_tx_cmd_single','iwx_rx_tx_cmd','dexterOriginal_iwx_rx_tx_cmd')
oldbody='\n'.join(function(old,name) for name in names).replace('ItlIwx::','OldModel::')
newbody='\n'.join(function(new,name) for name in names).replace('ItlIwx::','NewModel::')
assert 'zc28Aggregate(pkt)' in newbody, 'new production aggregate helper not yet present'
assert 'zc28Note(' in newbody, 'new production failure status helper not yet present'
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
constexpr unsigned IWX_TX_STATUS_MSK=0xff,IWX_TX_STATUS_SUCCESS=1;
constexpr unsigned IEEE80211_FC0_TYPE_MGT=0,IWX_DEVICE_FAMILY_AX210=210;
constexpr unsigned IWX_RBUF_SIZE=4096,BUS_DMASYNC_POSTREAD=1;
constexpr unsigned IWX_AGG_TX_STATE_STATUS_MSK=0xfff,IWX_AGG_TX_STATE_TRY_CNT_MSK=0xf000;
constexpr unsigned IWX_AGG_TX_STATE_TRY_CNT_POS=12;
inline uint16_t le16toh(uint16_t n){return n;}
inline uint16_t le16_to_cpu(uint16_t n){return n;}
inline uint32_t le32toh(uint32_t n){return n;}
thread_local uint64_t *printCalls=nullptr;
thread_local bool inGate=false;
#define XYLog(...) (++*printCalls)
#define DPRINTFN(...) do {} while(0)
#define KASSERT(c,...) assert(c)
#define IC2IFP(ic) (&(ic)->ic_if)
#define IWX_AGG_SSN_TO_TXQ_IDX(i,n) ((i)&((n)-1))
const char*iwx_get_agg_tx_status(unsigned){return "synthetic";}
void bus_dmamap_sync(int,int,int,int,int){}
struct Stats {uint64_t outputErrors=0;};
struct _ifnet {Stats *netStat;};
struct ieee80211com {_ifnet ic_if;};
struct iwx_tx_data {unsigned type=8;};
struct iwx_tx_ring {unsigned ring_count=256;iwx_tx_data data[256];};
struct iwx_rx_data {int map=0;};
struct iwx_agg_tx_status {uint16_t status,sequence;};
struct iwx_tx_resp {uint8_t frame_count=1,tx_queue=2,ra_tid=0,reserved=0;
    iwx_agg_tx_status status{},tail[256]{};};
struct iwx_rx_packet {alignas(8) char data[sizeof(iwx_tx_resp)]{};};
struct iwx_softc {ieee80211com sc_ic;iwx_tx_ring txq[32];int sc_device_family=210;
    int sc_tx_timer=15,sc_dmat=0;unsigned sc_mgmt_last_antenna_idx=0;};
class CommonModel {public:
    bool mapped=true,observer=true;uint32_t fPc1Suspended=0,fPc1DroppedCompletion=0;
    uint64_t advances=0,clears=0,antennas=0,prints=0,aggregateCalls=0;
    uint64_t aggregateFrames=0,statusCalls[16]{},firstRaw=0,lastRaw=0;
    int advancedIndex=-1;
    bool dma5Mapped(){return mapped;}
    void iwx_toggle_tx_ant(iwx_softc*,unsigned*p){++antennas;++*p;}
    void iwx_ampdu_txq_advance(iwx_softc*,iwx_tx_ring*,int idx,bool normal=true){
        assert(normal);++advances;advancedIndex=idx;}
    void iwx_clear_oactive(iwx_softc*,iwx_tx_ring*){++clears;}
    void zc28Note(unsigned event,uint64_t a=0,uint64_t b=0,uint64_t c=0){
        assert(inGate);assert(event<16);if(!observer)return;++statusCalls[event];
        if(!firstRaw)firstRaw=a;lastRaw=a;(void)b;(void)c;
    }
    void zc28Aggregate(const iwx_rx_packet*p){
        assert(inGate);if(!observer)return;
        const auto*r=reinterpret_cast<const iwx_tx_resp*>(p->data);
        ++aggregateCalls;aggregateFrames+=r->frame_count;
    }
};
class OldModel:public CommonModel {public:
    void iwx_rx_tx_cmd_single(iwx_softc*,iwx_rx_packet*,iwx_tx_data*);
    void iwx_rx_tx_cmd(iwx_softc*,iwx_rx_packet*,iwx_rx_data*);
    void dexterOriginal_iwx_rx_tx_cmd(iwx_softc*,iwx_rx_packet*,iwx_rx_data*);
};
class NewModel:public CommonModel {public:
    void iwx_rx_tx_cmd_single(iwx_softc*,iwx_rx_packet*,iwx_tx_data*);
    void iwx_rx_tx_cmd(iwx_softc*,iwx_rx_packet*,iwx_rx_data*);
    void dexterOriginal_iwx_rx_tx_cmd(iwx_softc*,iwx_rx_packet*,iwx_rx_data*);
};
'''
tests=r'''
struct Outcome {
    uint64_t errors,advances,clears,antennas,suspendedDrops;int advancedIndex,timer;
    bool operator==(const Outcome&b)const {
        return errors==b.errors&&advances==b.advances&&clears==b.clears&&
            antennas==b.antennas&&suspendedDrops==b.suspendedDrops&&
            advancedIndex==b.advancedIndex&&timer==b.timer;}
};
template<class Model> Outcome invoke(Model&x,iwx_softc&sc,unsigned frames,unsigned status,bool management){
    iwx_rx_packet packet;iwx_rx_data data;
    auto*r=reinterpret_cast<iwx_tx_resp*>(packet.data);r->frame_count=frames;r->tx_queue=2;
    auto*statuses=&r->status;
    for(unsigned i=0;i<frames;++i)statuses[i]={uint16_t(status),uint16_t(i)};
    uint32_t ssn=7;std::memcpy(statuses+frames,&ssn,sizeof(ssn));
    sc.txq[2].data[7].type=management?0:8;printCalls=&x.prints;
    assert(inGate);x.iwx_rx_tx_cmd(&sc,&packet,&data);printCalls=nullptr;
    return {sc.sc_ic.ic_if.netStat->outputErrors,x.advances,x.clears,x.antennas,
            x.fPc1DroppedCompletion,x.advancedIndex,sc.sc_tx_timer};
}
void compare(bool mapped,bool observer,unsigned frames,unsigned status,bool management,bool suspended=false){
    OldModel old;NewModel fresh;old.mapped=fresh.mapped=mapped;fresh.observer=observer;
    old.fPc1Suspended=fresh.fPc1Suspended=suspended;
    iwx_softc a,b;Stats sa,sb;a.sc_ic.ic_if.netStat=&sa;b.sc_ic.ic_if.netStat=&sb;
    inGate=true;auto oa=invoke(old,a,frames,status,management);
    auto ob=invoke(fresh,b,frames,status,management);inGate=false;
    assert(oa==ob);assert(fresh.prints==0);
    if(suspended&&mapped){assert(fresh.fPc1DroppedCompletion==1&&fresh.aggregateCalls==0);return;}
    if(frames==1){
        assert(old.prints==(status!=IWX_TX_STATUS_SUCCESS));
        assert(fresh.statusCalls[3]==(observer&&status!=IWX_TX_STATUS_SUCCESS));
        assert(fresh.aggregateCalls==0);
    }else{
        assert(old.prints==frames);
        assert(fresh.aggregateCalls==observer);
        assert(fresh.aggregateFrames==(observer?frames:0));
    }
}
template<class Model> Outcome concurrentRun(bool observer){
    Model m;m.mapped=false;m.observer=observer;iwx_softc sc;Stats stats;sc.sc_ic.ic_if.netStat=&stats;
    std::mutex gate;
    auto f=[&](unsigned seed){for(unsigned n=0;n<2000;++n){std::lock_guard<std::mutex>l(gate);
        inGate=true;invoke(m,sc,(n+seed)%3?1:16,(n+seed)%4?1:2,(n+seed)%5==0);inGate=false;}};
    std::thread a(f,0),b(f,1);a.join();b.join();assert(!printCalls);
    return {stats.outputErrors,m.advances,m.clears,m.antennas,m.fPc1DroppedCompletion,m.advancedIndex,sc.sc_tx_timer};
}
int main(){
    unsigned cases=0;
    for(bool mapped:{false,true})for(bool observer:{false,true})for(unsigned frames:{1,2,16,64,128,255})
      for(unsigned status:{1,2,0x80,0xff})for(bool mgmt:{false,true}){
        compare(mapped,observer,frames,status,mgmt);++cases;}
    for(bool observer:{false,true}){compare(true,observer,1,2,true,true);
        compare(true,observer,64,1,false,true);cases+=2;}
    assert(concurrentRun<OldModel>(false)==concurrentRun<NewModel>(false));
    assert(concurrentRun<OldModel>(false)==concurrentRun<NewModel>(true));
    // Valid v6-shaped successes are an explicit negative control. No old log
    // calls means a hypothetical aggregate log flood was NOT demonstrated.
    for(unsigned n=0;n<1000;++n)compare(false,n%2,1,1,false);
    std::cout<<"PASS actual3handler differential "<<cases<<" cases+v6successcontrol+gateserialized concurrency; unchangederror/antenna/retire/wake/lifecycle effects, newselectedhandlersprintf0 withobserverstubsON/OFF. AggregateFWreachabilityNOTproved; mockhelperinternalsNOTproductionhelpervalidation.\n";
}
'''
with tempfile.TemporaryDirectory(prefix='zc27-handlerdiff-') as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(mock+oldbody+newbody+tests)
    cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++',
        '-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk',
        '-std=c++17','-O1','-g','-pthread','-fsanitize='+args.sanitizer,str(p/'test.cpp'),'-o',str(p/'test')]
    print('COMMAND',repr(cmd),flush=True)
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=180)
