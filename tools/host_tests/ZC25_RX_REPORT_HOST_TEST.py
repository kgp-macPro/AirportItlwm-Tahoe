#!/usr/bin/env python3
"""Extract actual ZC25 RX report helpers; model only gate/provider/pool, not DMA or throughput.
Accept sanitizer as argv[1], default address,undefined. ZC25_SOURCE_ROOT optionally
selects an explicit source worktree; normal tracked invocation uses parents[2].
"""
from pathlib import Path
import os,re,subprocess,tempfile,sys
R=Path(os.environ.get('ZC25_SOURCE_ROOT',Path(__file__).resolve().parents[2]))
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def extract(n):
    masked=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
    q=re.search(r'void\s+ItlIwx::\s*'+re.escape(n)+r'\s*\(',masked);assert q,n
    a=masked.index('{',q.end());b=a+1;d=1
    while d:d+=(masked[b]=='{')-(masked[b]=='}');b+=1
    return s[q.start():b]

assert '__atomic_store_n(&fRxZcSnapshots,0ULL,__ATOMIC_RELAXED)' in s
assert '__atomic_store_n(&fRxZcPropertyFailures,0ULL,__ATOMIC_RELAXED)' in s
assert 'fRxZcLastReport=fRxZcSnapshots=fRxZcPropertyFailures=0' not in s
report=extract('rxZcReport')
assert report.index('if (!reason || !pci.pa_tag) return;') < report.index('time2GNow')
assert 'that->rxZcCapturePeriodic();' in s
assert 'rxZcPublishPeriodic();' in s
cap=extract('rxZcCapturePeriodic')
guard=cap[:cap.index('const uint64_t now=')]
assert guard.index('__atomic_load_n(&fRxZcClosing,__ATOMIC_ACQUIRE)') < guard.index('!fRxZcRunning') < guard.index('!fRxZcPool')

mock=r'''
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <cassert>
#include <iostream>
static std::atomic<uint64_t> clocks{0}, snapshots{0};
static thread_local bool in_gate=false;
struct WorkLoop {bool inGate()const{return in_gate;}};
struct Gate {std::mutex &m;Gate(std::mutex&v):m(v){m.lock();in_gate=true;}~Gate(){in_gate=false;m.unlock();}};
struct Provider { std::mutex mutex;std::map<std::string,std::vector<uint64_t>> props;bool fail=false;unsigned calls=0;
bool setProperty(const char*k,const void*b,size_t n){std::lock_guard<std::mutex>g(mutex);++calls;if(fail)return false;props[k]=std::vector<uint64_t>((const uint64_t*)b,(const uint64_t*)b+n/8);return true;}
std::vector<uint64_t> get(const char*k){std::lock_guard<std::mutex>g(mutex);return props[k];}};
struct Pool{void snapshot(uint64_t*v){++snapshots;for(unsigned i=0;i<16;i++)v[i]=100+i;}bool failed()const{return false;}};
struct Life{uint64_t live=1,created=2,freed=3,callbacks=4,detached=5,closeState=6;};
struct ItlIwx {
WorkLoop wl;Provider provider;struct {Provider*pa_tag;}pci{&provider};Pool pool,*fRxZcPool=&pool;Life life,*fRxZcLife=&life;bool mapped=true;
bool fRxZcPublished=true,fRxZcTerminal=false,fRxZcRunning=true,fRxZcCleanHandoff=false;
uint32_t fRxZcClosing=0,fRxZcFault=0,fRxZcGeneration=7,fRxZcEpoch=8,fPc1ResetEpoch=8,fPc1TerminalEpoch=0,fPc1Fault=0,fPc1Suspended=0;
uint64_t fRxZcSynced=10,fRxZcReplaced=11,fRxZcStarved=12,fRxZcNotif=13;
uint64_t fRxZcLastReport=0,fRxZcSnapshots=0,fRxZcPropertyFailures=0;
bool fRxZcReportedFault=false,fRxZcPressure=false;
uint64_t fRxZcEpisodes=14,fRxZcRecoveries=15,fRxZcBatchQueued=16,fRxZcBatchMax=17;
uint64_t fRxZcFirstPressure[24]={},fRxZcLastRecovery[24]={};
static constexpr unsigned kRxZcCount=2048,IWX_RX_MQ_RING_COUNT=512;
WorkLoop*getMainWorkLoop(){return &wl;}bool dma5Mapped()const{return mapped;}
bool rxZcFailed()const{return fRxZcFault || (fRxZcPool&&fRxZcPool->failed());}
static uint64_t time2GNow(){return ++clocks*1000000000ULL;}
void rxZcReport(unsigned);
    // ZC25 watchdog-owned immutable-after-capture publication buffers.
    // Never written by notification paths; no retained pool/mbuf/DMA pointer.
    uint64_t fRxZcPeriodic[64]={};
    uint64_t fRxZcPeriodicFirstPressure[24]={}, fRxZcPeriodicLastRecovery[24]={};
    bool fRxZcPeriodicReady=false;
    void rxZcCapturePeriodic();
    void rxZcPublishPeriodic();
};
'''
test=r'''static unsigned checks=0;
static void check(bool v){++checks;assert(v);}
static bool eligible(const std::vector<uint64_t>&r,uint64_t gen,uint64_t epoch){return r.size()==64&&r[0]==4&&r[11]==gen&&r[12]==epoch;}
int main(){
std::mutex gate;
{ItlIwx x;auto c=clocks.load(),s=snapshots.load();x.rxZcReport(0);check(clocks==c);check(snapshots==s);check(x.provider.calls==0);check(x.fRxZcSnapshots==0);}
{ItlIwx x;x.rxZcCapturePeriodic();check(!x.fRxZcPeriodicReady);check(x.provider.calls==0);}
for(unsigned mode=0;mode<4;mode++){ItlIwx x;if(mode==0)x.mapped=false;if(mode==1)x.fRxZcPool=nullptr;if(mode==2)x.fRxZcClosing=1;if(mode==3)x.fRxZcRunning=false;auto c=clocks.load();{Gate g(gate);x.rxZcCapturePeriodic();}check(!x.fRxZcPeriodicReady);check(clocks==c);x.rxZcPublishPeriodic();check(x.provider.calls==0);}
{ItlIwx x;{Gate g(gate);x.rxZcReport(1);}auto forced=x.provider.get("KGP_RX_ZC4");{Gate g(gate);x.rxZcCapturePeriodic();check(x.provider.calls==1);x.rxZcPublishPeriodic();check(x.provider.calls==1);}x.rxZcPublishPeriodic();auto r=x.provider.get("KGP_RX_ZC4_PERIODIC");check(r.size()==64);check(r[1]>forced[1]);check(r[2]==0);for(unsigned i=0;i<64;i++)if(i!=1&&i!=2&&i!=17)check(r[i]==forced[i]);auto n=x.provider.calls;x.rxZcPublishPeriodic();check(x.provider.calls==n);}
{ItlIwx x;x.fRxZcFirstPressure[0]=4;x.fRxZcLastRecovery[0]=4;{Gate g(gate);x.rxZcCapturePeriodic();x.fRxZcRunning=false;x.fRxZcClosing=1;x.rxZcReport(3);}auto forced=x.provider.get("KGP_RX_ZC4");x.rxZcPublishPeriodic();check(x.provider.get("KGP_RX_ZC4")==forced);check(forced[2]==3);check(x.provider.get("KGP_RX_ZC4_PERIODIC")[1]<forced[1]);check(x.provider.get("KGP_RX_ZC4_PERIODIC_FIRST_PRESSURE")[0]==4);check(x.provider.get("KGP_RX_ZC4_PERIODIC_LAST_RECOVERY")[0]==4);{Gate g(gate);x.rxZcCapturePeriodic();}check(!x.fRxZcPeriodicReady);}
{ItlIwx x;{Gate g(gate);x.rxZcCapturePeriodic();x.fRxZcGeneration=9;x.fRxZcEpoch=10;x.fRxZcSnapshots=0;x.rxZcReport(1);}x.rxZcPublishPeriodic();check(!eligible(x.provider.get("KGP_RX_ZC4_PERIODIC"),9,10));check(eligible(x.provider.get("KGP_RX_ZC4"),9,10));}
{ItlIwx x;x.fRxZcFault=6;{Gate g(gate);x.rxZcReport(0);check(x.provider.calls==0);x.rxZcCapturePeriodic();}x.rxZcPublishPeriodic();check(x.provider.get("KGP_RX_ZC4_PERIODIC")[3]==6);check(x.fRxZcFault==6);}
{ItlIwx x;x.provider.fail=true;{Gate g(gate);x.rxZcReport(1);x.rxZcCapturePeriodic();}x.rxZcPublishPeriodic();check(x.fRxZcPropertyFailures==2);}
{ItlIwx x;std::thread forced([&]{for(unsigned i=0;i<400;i++){Gate g(gate);x.rxZcReport(1);}});std::thread periodic([&]{for(unsigned i=0;i<400;i++){{Gate g(gate);x.rxZcCapturePeriodic();}x.rxZcPublishPeriodic();}});forced.join();periodic.join();check(x.fRxZcSnapshots==800);check(x.provider.get("KGP_RX_ZC4")[2]==1);check(x.provider.get("KGP_RX_ZC4_PERIODIC")[2]==0);}
std::cout<<"PASS actual production helper checks="<<checks<<" concurrent_records=800\n";
}
'''

san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc25-rx-report-') as t:
    p=Path(t);(p/'test.cpp').write_text(mock+'\n'+extract('rxZcReport')+'\n'+extract('rxZcCapturePeriodic')+'\n'+extract('rxZcPublishPeriodic')+'\n'+test)
    cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
    print('SOURCE',str(R),flush=True);print('COMMAND',cmd,flush=True)
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
