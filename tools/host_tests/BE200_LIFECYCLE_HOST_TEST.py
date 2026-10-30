#!/usr/bin/env python3
"""Execute actual post-stop cleanup and gate-wait helpers; mock hardware/KPIs."""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2]
def extract(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:void|int)\s+ItlIwx::\s*'+n+r'\(',m);assert x;a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cerrno>
#include "itlwm/hal_iwx/ItlIwxBzContracts.hpp"
constexpr unsigned IWX_FLAG_MLD_TXQ_ACTIVE=0x800;
constexpr unsigned IWX_CMD_QUEUE_SIZE_GEN3=128,IWX_TXDATA_FLAG_CMD_PENDING=2,IWX_MAX_DRAM_ENTRY=64;
constexpr int IWX_GATE_SLICE_MS=10,IWX_GATE_STALL_REPORT_MS=100,THREAD_UNINT=0,THREAD_AWAKENED=0,THREAD_TIMED_OUT=1;
constexpr int kMillisecondScale=1000000;using AbsoluteTime=uint64_t;
static uint64_t now=0,advance=0;static uint64_t iwx_boot_time_ns(){return now;}
static void clock_interval_to_deadline(unsigned x,unsigned scale,uint64_t*p){*p=now+x*scale;}
struct Provider {template<class...A>void setProperty(A...){}};
struct Dma {void*cmd=nullptr;};struct Ring {Dma desc_dma,cmd_dma,bc_tbl;struct{unsigned flags=2;}data[128];};
struct iwx_softc {unsigned sc_flags=0,sc_mld_txq_pending=0,sc_mld_txq_failed=0;uint32_t sc_mld_key_flags[8]={};Ring sc_tvqm_ring,txq[1];Dma pnvm_frag_dma[64],pnvm_desc_dma;Provider p;Provider*sc_pcitag=&p;unsigned sc_gate_stalls=0,sc_gate_stall_max_ms=0;};
struct Gate {bool held=true;int result=THREAD_AWAKENED;unsigned sleeps=0;bool inGate(){return held;}int commandSleep(void*,uint64_t,int){++sleeps;now+=advance;return result;}};
struct ItlIwx {bool fBzScanStarted=false;bool fBzHardwareOwned=true,fPc1LastMaster=false,fPc1Suspended=false;unsigned fPc1TerminalEpoch=0,fPc1ResetEpoch=1,fBzFault=9,fBzError=22,fBzQueueOwned=7;uint64_t fBzStats[32]={};kgp_bz::PnvmLedger fBzPnvm;kgp_bz::CommandLedger fBzCommands;Gate gate;unsigned freed=0,fw=0,paging=0,resets=0;
 Gate*getMainWorkLoop(){return &gate;}Gate*getMainCommandGate(){return &gate;}
 void iwx_ctxt_info_free_fw_img(iwx_softc*){++fw;}void iwx_ctxt_info_free_paging(iwx_softc*){++paging;}
 void iwx_reset_tx_ring(iwx_softc*,Ring*){++resets;}
 void iwx_free_tx_ring(iwx_softc*,Ring*){++freed;}
 void iwx_dma_contig_free(Dma*){assert(!fBzHardwareOwned);++freed;}
 void bzStoppedCleanup(iwx_softc*);int iwx_gate_sleep(iwx_softc*,void*,uint32_t*,int);
};
'''
test=r'''
int main(){
 for(unsigned stage=0;stage<4;++stage){ItlIwx t;iwx_softc sc;sc.sc_flags=IWX_FLAG_MLD_TXQ_ACTIVE;sc.sc_mld_key_flags[0]=17;t.fBzHardwareOwned=stage<1;t.fPc1LastMaster=stage>=2;t.fPc1Suspended=stage>=3;t.fPc1TerminalEpoch=stage>=4?1:0;t.bzStoppedCleanup(&sc);assert(!t.freed&&!t.fw&&!t.paging&&sc.sc_mld_key_flags[0]==17&&(sc.sc_flags&IWX_FLAG_MLD_TXQ_ACTIVE));}
 {ItlIwx t;iwx_softc sc;t.fBzHardwareOwned=false;t.fPc1LastMaster=t.fPc1Suspended=true;t.fPc1TerminalEpoch=1;t.gate.held=false;t.bzStoppedCleanup(&sc);assert(!t.freed);}
 {ItlIwx t;iwx_softc sc;t.fBzHardwareOwned=false;t.fPc1LastMaster=t.fPc1Suspended=true;t.fPc1TerminalEpoch=1;sc.sc_tvqm_ring.desc_dma.cmd=&t;sc.sc_flags=IWX_FLAG_MLD_TXQ_ACTIVE;sc.sc_mld_key_flags[0]=17;sc.sc_mld_txq_pending=sc.sc_mld_txq_failed=255;t.fBzScanStarted=true;
  t.fBzCommands.begin(0,3,1,1,false);assert(t.fBzPnvm.arm(1,1)&&t.fBzPnvm.publish(1,1));assert(!t.fBzPnvm.reset(false));t.bzStoppedCleanup(&sc);assert(t.fw==1&&t.paging==1&&t.resets==1&&t.freed==66);
  assert(!sc.sc_mld_key_flags[0]&&!sc.sc_mld_txq_pending&&!sc.sc_mld_txq_failed&&!(sc.sc_flags&IWX_FLAG_MLD_TXQ_ACTIVE)&&!t.fBzScanStarted);
  assert(!t.fBzFault&&!t.fBzQueueOwned&&t.fBzStats[15]==9&&t.fBzCommands.available(0)&&!t.fBzPnvm.issued);}
 {ItlIwx t;iwx_softc sc;uint32_t budget=4;advance=0;for(unsigned i=0;i<3;i++)assert(!t.iwx_gate_sleep(&sc,nullptr,&budget,1));assert(t.iwx_gate_sleep(&sc,nullptr,&budget,1)==EWOULDBLOCK&&budget==0);}
 {ItlIwx t;iwx_softc sc;uint32_t budget=1000;advance=1100000000ULL;assert(t.iwx_gate_sleep(&sc,nullptr,&budget,1)==EWOULDBLOCK);assert(sc.sc_gate_stalls==1&&sc.sc_gate_stall_max_ms==1100);}
 std::cout<<"PASS actual BZ cleanup: partial-init/no-stop/no-epoch/off-gate retention, positive terminal cleanup, pending command reset; actual gate wait bounds include early wakes and delayed reacquisition. Mock KPIs; not DMA or runtime proof.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-life-') as d:
 p=Path(d);(p/'t.cpp').write_text(pre+extract((R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text(),'bzStoppedCleanup')+extract((R/'itlwm/hal_iwx/ItlIwxBz.inc').read_text(),'iwx_gate_sleep')+test)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+san,'-I'+str(R),str(p/'t.cpp'),'-o',str(p/'t')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'t')],check=True,timeout=60)
