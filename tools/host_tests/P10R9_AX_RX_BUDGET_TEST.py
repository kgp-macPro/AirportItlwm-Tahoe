#!/usr/bin/env python3
"""Compile the actual mapped RX service; check ownership/publication/reentry.
Offline assertions do not demonstrate hardware throughput or the stall cause.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
a=s.index('void ItlIwx::\niwx_notif_intr(');b=s.index('{',a);j=b+1;depth=1
while depth:depth+=(s[j]=='{')-(s[j]=='}');j+=1
method=s[a:j]
code=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <functional>
#define IWX_RX_MQ_RING_COUNT 512
#define IWX_DEVICE_FAMILY_AX210 22000
#define IWX_RFH_Q0_FRBDCB_WIDX_TRG 1
#define MBUF_LIST_INITIALIZER() {}
#define DPRINTFN(a,b) do{}while(0)
#define le16toh(v) (v)
struct iwx_rx_data {unsigned slot;};
struct iwx_rb_status {uint16_t closed_rb_num;};
struct iwx_softc {int sc_generation=1,sc_device_family=IWX_DEVICE_FAMILY_AX210;struct {unsigned cur=0;uint16_t *stat=nullptr;iwx_rx_data data[512];} rxq;};
struct mbuf_list {std::vector<unsigned> packets;};
unsigned ml_len(mbuf_list *m){return m->packets.size();}
void iwx_dma_queue_publish_barrier(){}
struct Workloop {bool inGate(){return true;}};
class ItlIwx {public:
 bool bz=false,leased=true,he=true;unsigned bzcalls=0,nativecalls=0;
 unsigned fRxZcClosing=0,fPc1Fault=0,fRxZcNotif=0,fPc1ResetEpoch=1;
 unsigned fRx2GHw=0;bool fRxZcRunning=true,failed=false;
 unsigned fRxZcGeneration=1,fRxZcEpoch=1,fRxZcBatchQueued=0,fRxZcBatchMax=0;
 unsigned parsed=0,repostFail=99999,packetsPerCompletion=2,nextPacket=0;
 std::vector<unsigned> credits,batches,delivered,reposted,paths;std::function<void(iwx_softc*)> hook;
 bool bzDevice(){return bz;}bool dma5Leased(){return leased;}bool rxZcFailed(){return failed;}
 bool axHeDiagActive(iwx_softc*){return !bz&&he;}
 void bzNotif(iwx_softc*){++bzcalls;}void dexterOriginal_iwx_notif_intr(iwx_softc*){++nativecalls;}
 void alive2FEvent(unsigned){}void rxZcReport(unsigned){}void dma5Report(){}
 Workloop *getMainWorkLoop(){static Workloop w;return &w;}
 void iwx_rx_pkt(iwx_softc*,iwx_rx_data*,mbuf_list *m){++parsed;for(unsigned i=0;i<packetsPerCompletion;++i)m->packets.push_back(nextPacket++);}
 bool rxZcRepost(unsigned cur){if(parsed==repostFail)return false;reposted.push_back(cur);return true;}
 void write(iwx_softc*,unsigned credit){assert(!reposted.empty());credits.push_back(credit);}
 int axInputHandoff(iwx_softc *sc,mbuf_list *m,unsigned path){paths.push_back(path);batches.push_back(ml_len(m));delivered.insert(delivered.end(),m->packets.begin(),m->packets.end());m->packets.clear();if(hook){auto h=hook;h(sc);}return 0;}
 void iwx_notif_intr(iwx_softc*);
};
#define IWX_WRITE(sc,reg,value) write(sc,value)
'''+method+r'''
void setup(iwx_softc &sc,uint16_t &hw,unsigned start,unsigned stop){hw=stop;sc.rxq.stat=&hw;sc.rxq.cur=start;for(unsigned i=0;i<512;++i)sc.rxq.data[i].slot=i;}
void fifo(const ItlIwx &d){for(unsigned i=0;i<d.delivered.size();++i)assert(d.delivered[i]==i);}
int main(){
 {iwx_softc sc;uint16_t hw;setup(sc,hw,0,100);ItlIwx d;d.iwx_notif_intr(&sc);assert(d.parsed==100&&sc.rxq.cur==100);assert((d.batches==std::vector<unsigned>{64,64,64,8}));assert((d.credits==std::vector<unsigned>{24,56,88,96}));assert(d.fRxZcBatchMax==64);fifo(d);}
 {iwx_softc sc;uint16_t hw;setup(sc,hw,496,48);ItlIwx d;d.iwx_notif_intr(&sc);assert(d.parsed==64&&sc.rxq.cur==48);assert((d.credits==std::vector<unsigned>{8,40,40}));fifo(d);}
 // A large reorder release is never split, discarded, or reordered.
 {iwx_softc sc;uint16_t hw;setup(sc,hw,0,1);ItlIwx d;d.packetsPerCompletion=768;d.iwx_notif_intr(&sc);assert(d.delivered.size()==768&&d.batches[0]==768);fifo(d);}
 // Callback reset, stop, mode loss or reentrant cursor change ends old service.
 for(unsigned k=0;k<5;++k){iwx_softc sc;uint16_t hw;setup(sc,hw,0,100);ItlIwx d;d.hook=[&](iwx_softc *s){if(k==0)++s->sc_generation;if(k==1)++d.fPc1ResetEpoch;if(k==2)d.fRxZcRunning=false;if(k==3)d.he=false;if(k==4)s->rxq.cur=40;};d.iwx_notif_intr(&sc);assert(d.parsed==32&&d.credits.size()==1&&d.paths.size()==1);fifo(d);}
 {iwx_softc sc;uint16_t hw;setup(sc,hw,0,100);ItlIwx d;d.repostFail=3;d.iwx_notif_intr(&sc);assert(sc.rxq.cur==2&&d.parsed==3&&d.credits.empty()&&d.paths[0]==2);}
 // Device/mode separation: BZ early dispatch; AX VHT and native unchanged.
 {iwx_softc sc;uint16_t hw;setup(sc,hw,0,100);ItlIwx d;d.bz=true;d.iwx_notif_intr(&sc);assert(d.bzcalls==1&&!d.parsed&&d.batches.empty());}
 {iwx_softc sc;uint16_t hw;setup(sc,hw,0,100);ItlIwx d;d.he=false;d.iwx_notif_intr(&sc);assert(d.batches.size()==1&&d.batches[0]==200&&d.credits.size()==1);fifo(d);}
 {iwx_softc sc;uint16_t hw;setup(sc,hw,0,100);ItlIwx d;d.leased=false;d.iwx_notif_intr(&sc);assert(d.nativecalls==1&&!d.parsed);}
 puts("PASS actual AX RX loop: FIFO, completion/packet triggers, RFH credit ordering/wrap, unsplit reorder release, reset/stop/reentry, failed repost, BZ/VHT/native bypass; ASan/UBSan");}
'''
with tempfile.TemporaryDirectory(prefix='p10r9-host-') as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
