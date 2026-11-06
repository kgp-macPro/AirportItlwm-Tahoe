#!/usr/bin/env python3
"""Actual P3/P4 notification and wait functions. Gate/MMIO/wakeup are mocks.
Opaque status semantics follow pinned Linux callback; no hardware proof.
"""
from pathlib import Path
import re,subprocess,tempfile,sys,hashlib,json
R=Path(__file__).resolve().parents[2];G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
C=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip()
SDK=subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
def fn(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:static\s+)?(?:void|int|bool)\s+'+re.escape(n).replace('::',r'::\s*')+r'\(',m);assert x,n
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
transport=(R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text();src=(R/'itlwm/hal_iwx/ItlIwxBz.inc').read_text()
oldsrc=subprocess.check_output([G,'show','1d35f8ffe2e014f5b11dad364b78f34ac7e07cc8:itlwm/hal_iwx/ItlIwxBz.inc'],cwd=R,text=True)
oldcpp=subprocess.check_output([G,'show','1d35f8ffe2e014f5b11dad364b78f34ac7e07cc8:itlwm/hal_iwx/ItlIwx.cpp'],cwd=R,text=True)
a=oldcpp.index('                    if(iwx_rx_packet_payload_len(pkt)!=sizeof(iwl_pnvm_init_complete_ntfy))');b=oldcpp.index('\n                }',a)
oldbody=oldcpp[a:b].replace('break;','return;')
hardware=json.loads((R/'tools/host_tests/fixtures/BE200_P4_INTERFACE.json').read_text());assert hardware['properties']['ItlwmPnvmNotifySeen']==1
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <cstring>
#include <iostream>
#include "itlwm/hal_iwx/ItlIwxBzContracts.hpp"
constexpr unsigned IWX_PNVM_COMPLETE=4,IWX_GATE_WAIT_NOTIF=1,IWX_GATE_WAIT_ALIVE=2,IWX_CMD_QUEUE_SIZE_GEN3=128;
struct iwl_pnvm_init_complete_ntfy {uint32_t status;};
struct iwx_softc {int sc_generation=1,sc_init_complete=0;uint32_t sc_pnvm_status=0;
 struct {int ic_state=0;}sc_ic;struct {int uc_intr=0;}sc_uc;struct{bool active=false;int error=0;}sc_boot_diag;
 struct{int*desc=nullptr;}txq[1];};
struct Gate {bool held=true;unsigned wakes=0;bool inGate(){return held;}void commandWakeup(void*){++wakes;}};
struct ItlIwx {iwx_softc com;Gate gate;bool bz=true;unsigned fBzFault=0,fBzError=0,fPc1ResetEpoch=1,fTx2IBlocked=0;
 kgp_bz::PnvmLedger fBzPnvm;kgp_bz::RxLedger fBzRx;uint64_t fBzStats[32]={};
 unsigned action=0,sleeps=0,polls=0;bool duringPoll=false;uint32_t raw=0x50001;
 Gate*getMainWorkLoop(){return &gate;}Gate*getMainCommandGate(){return &gate;}bool bzDevice(){return bz;}
 void bzConnectionEvent(unsigned,uint64_t,uint64_t,uint64_t){}
 void bzFail(uint32_t,int);int bzWaitFailure(iwx_softc*,int,uint32_t);void bzPnvmComplete(iwx_softc*,const void*,size_t);
 int iwx_wait_notification(iwx_softc*,int*,int,uint32_t);int iwx_wait_alive_poll(iwx_softc*,int*,int,uint32_t);
 void event(iwx_softc*s,int*v,int bits){
  if(action==1)bzPnvmComplete(s,&raw,4);
  if(action==2)bzFail(25,EIO);
  if(action==3){++s->sc_generation;*v|=bits;}
  if(action==4){++fPc1ResetEpoch;*v|=bits;}
  if(action==5){*v|=bits;bzFail(25,EIO);}
  if(action==6)*v|=bits;
 }
 int iwx_gate_sleep(iwx_softc*s,void*v,uint32_t*,int){++sleeps;if(!duringPoll)event(s,(int*)v,IWX_PNVM_COMPLETE);return EWOULDBLOCK;}
 int iwx_bz_poll_msix_causes(iwx_softc*s){++polls;if(duringPoll)event(s,&s->sc_init_complete,IWX_PNVM_COMPLETE);return 0;}
 void issue(){assert(fBzPnvm.arm(com.sc_generation,fPc1ResetEpoch));assert(fBzPnvm.publish(com.sc_generation,fPc1ResetEpoch));}
 int wait(bool poll=false){return poll?iwx_wait_alive_poll(&com,&com.sc_init_complete,IWX_PNVM_COMPLETE,2000):iwx_wait_notification(&com,&com.sc_init_complete,IWX_PNVM_COMPLETE,2000);}
};
'''
main=r'''
int main(){unsigned cases=0;
#ifdef OLD_P3
 {ItlIwx t;t.issue();t.action=1;assert(t.wait()==EWOULDBLOCK);assert(t.fBzFault==25&&t.fBzError==EIO&&!t.com.sc_init_complete);++cases;}
 {ItlIwx t;t.issue();t.action=1;t.raw=0;assert(!t.wait()&&t.com.sc_init_complete==IWX_PNVM_COMPLETE);++cases;}
 {ItlIwx t;t.action=4;assert(!t.wait());++cases;}
 std::cout<<"REPRODUCED actual P3 notification: raw0x50001 -> first EIO25 masked by EWOULDBLOCK; reset epoch ignored (3 cases)\n";
#else
 for(uint32_t raw:{0U,0x50001U,1U,0xffffffffU}){ItlIwx t;t.issue();t.raw=raw;t.action=1;assert(!t.wait());assert(t.fBzPnvm.received&&t.fBzPnvm.status==raw&&t.com.sc_pnvm_status==raw&&t.fBzStats[12]==1&&!t.fBzFault);assert(t.fBzPnvm.issued&&!t.fBzPnvm.reset(false));++cases;}
 for(unsigned n:{0U,1U,3U,5U,0xffffffffU}){ItlIwx t;t.issue();t.bzPnvmComplete(&t.com,&t.raw,n);assert(t.fBzFault==25&&t.fBzError==EPROTO&&!t.com.sc_init_complete&&!t.fBzPnvm.received);++cases;}
 {ItlIwx t;t.issue();t.bzPnvmComplete(&t.com,nullptr,4);assert(t.fBzError==EPROTO);++cases;}
 for(unsigned which=0;which<4;++which){ItlIwx t;if(which==1)assert(t.fBzPnvm.arm(1,1));if(which>=2)t.issue();if(which==2)++t.com.sc_generation;if(which==3)++t.fPc1ResetEpoch;t.bzPnvmComplete(&t.com,&t.raw,4);assert(t.fBzError==EPROTO&&!t.fBzPnvm.received);++cases;}
 {ItlIwx t;t.issue();t.bzPnvmComplete(&t.com,&t.raw,4);uint32_t other=9;t.bzPnvmComplete(&t.com,&other,4);assert(t.fBzError==EPROTO&&t.com.sc_pnvm_status==0x50001&&t.fBzStats[12]==1);++cases;}
 {ItlIwx t;t.issue();t.gate.held=false;t.bzPnvmComplete(&t.com,&t.raw,4);assert(!t.fBzPnvm.received&&!t.gate.wakes);++cases;}
 for(bool poll:{false,true}){
  {ItlIwx t;t.issue();t.action=1;t.duringPoll=poll;t.com.sc_boot_diag.active=poll;assert(!t.wait(poll)&&t.fBzPnvm.received);++cases;}
  for(unsigned action:{2U,3U,4U,5U}){ItlIwx t;t.issue();t.action=action;t.duringPoll=poll;t.com.sc_boot_diag.active=poll;assert(t.wait(poll)==((action==3||action==4)?ENXIO:EIO));++cases;}
  {ItlIwx t;t.issue();t.bzFail(25,EPROTO);assert(t.wait(poll)==EPROTO&&!t.sleeps&&!t.polls);++cases;}
  {ItlIwx t;t.issue();t.com.sc_init_complete=IWX_PNVM_COMPLETE;t.bzFail(25,EIO);assert(t.wait(poll)==EIO);++cases;}
  {ItlIwx t;t.issue();assert(t.wait(poll)==EWOULDBLOCK&&!t.fBzPnvm.received&&t.fBzPnvm.issued);++cases;}
 }
 {ItlIwx t;t.issue();t.bzFail(25,EPROTO);t.bzFail(27,EIO);assert(t.wait()==EPROTO&&t.fBzFault==25);++cases;}
 {ItlIwx t;t.bz=false;t.fBzFault=25;t.fBzError=EIO;t.action=4;assert(!t.wait());++cases;}
 {kgp_bz::PnvmLedger l;assert(l.arm(1,1)&&l.publish(1,1)&&l.complete(1,1,0x50001));assert(!l.arm(1,1)&&!l.publish(1,1)&&!l.reset(false));assert(l.reset(true)&&l.arm(2,2)&&l.publish(2,2));assert(!l.complete(1,1,0)&&l.complete(2,2,0));++cases;}
 std::cout<<"PASS "<<cases<<" actual P4 notification/wait + shared ledger cases; gate/IRQ/wake mocked; raw status opaque, no DMA free or runtime success claim\n";
#endif
}
'''
pre=pre.replace('uint32_t raw=0x50001;', 'uint32_t raw='+str(hardware['properties']['ItlwmPnvmNotifyStatus'])+';')
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-pnvm-completion-')as d:
 d=Path(d)
 # Execute the pinned Linux callback itself to establish the reference contract.
 linux=(R/'tools/host_tests/reference/be200/sources/linux-a38297e3fb01/fw/pnvm.c').read_text()
 callback=fn(linux,'iwl_pnvm_complete_fn')
 lin='#include <assert.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\nstruct iwl_notif_wait_data{int unused;};struct iwl_trans{int unused;};struct iwl_rx_packet{uint32_t data[1];};struct iwl_pnvm_init_complete_ntfy{uint32_t status;};\n#define IWL_DEBUG_FW(...) ((void)0)\n#define le32_to_cpu(x) (x)\n'+callback+'\nint main(void){struct iwl_trans t;uint32_t values[]={0,0x50001,1,0xffffffff};for(unsigned i=0;i<4;++i){struct iwl_rx_packet p={{values[i]}};assert(iwl_pnvm_complete_fn(NULL,&p,&t));}}\n'
 (d/'reference.c').write_text(lin)
 subprocess.run([C,'-x','c','-isysroot',SDK,'-std=gnu11','-fsanitize='+san,str(d/'reference.c'),'-o',str(d/'reference')],check=True);subprocess.run([str(d/'reference')],check=True)
 print('PASS pinned Linux v6.9 actual PNVM callback: four opaque status values',flush=True)
 for tag in ['P3','P4']:
  methods=fn(transport,'ItlIwx::bzFail')
  if tag=='P3':methods+='void ItlIwx::bzPnvmComplete(iwx_softc*sc,const void*p,size_t length){struct Packet{const void*data;}packet{p};auto*pkt=&packet;auto iwx_rx_packet_payload_len=[length](Packet*){return length;};'+oldbody+'}\n'
  else:methods+=fn(transport,'ItlIwx::bzWaitFailure')+fn(transport,'ItlIwx::bzPnvmComplete')
  methods+=fn(oldsrc if tag=='P3'else src,'ItlIwx::iwx_wait_notification')+fn(oldsrc if tag=='P3'else src,'ItlIwx::iwx_wait_alive_poll')
  (d/(tag+'.cpp')).write_text(pre+methods+main)
  cmd=[C,'-isysroot',SDK,'-std=gnu++17','-O1','-g','-fsanitize='+san,'-I'+str(R)]+(['-DOLD_P3']if tag=='P3'else[])+[str(d/(tag+'.cpp')),'-o',str(d/tag)]
  print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(d/tag)],check=True,timeout=60)
