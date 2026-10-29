#!/usr/bin/env python3
"""Execute actual P1/P2 boot context + P2 sync helper and pinned Apple synchronize.
Mocked allocation, walkAll, MMIO, gate and provider. No hardware simulation/proof.
"""
from pathlib import Path
import subprocess,re,tempfile,sys,hashlib
R=Path(__file__).resolve().parents[2]
G='/Applications/Xcode.app/Contents/Developer/usr/bin/git'
C='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
SDK='/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk'
def function(s,qualified):
 mask=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s,flags=re.S)
 pat=r'(?:int|IOReturn)\s+'+qualified.replace('::',r'::\s*')+r'\('
 m=re.search(pat,mask);assert m,qualified
 a=mask.index('{',m.end());b=a+1;d=1
 while d:d+=(mask[b]=='{')-(mask[b]=='}');b+=1
 return s[m.start():b]
apple=R/'tools/host_tests/reference/Apple-IODMACommand-f6217f891ac0.cpp'
a=apple.read_text();applebody=function(a,'IODMACommand::synchronize')
current=(R/'itlwm/hal_iwx/ItlIwxBz.inc').read_text()
old=subprocess.check_output([G,'show','070d4b641dfd967d366af35bfcc953a0c759b931:itlwm/hal_iwx/ItlIwxBz.inc'],cwd=R,text=True)
context='ItlIwx::iwx_bz_port_ctxt_info_gen3_init'
helper=function((R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text(),'ItlIwx::bzSyncBootMemory')
reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()+(R/'itlwm/hal_iwx/if_iwxvar.h').read_text()
source=function(current,context)
defs={};skip={'IWX_WRITE','IWX_WRITE64','IWX_SETBITS','IWX_CSR_HW_REV_STEP','IWX_DQA_CMD_QUEUE','IWX_TFD_QUEUE_CB_SIZE','IWX_RX_QUEUE_CB_SIZE'}
pending=set(re.findall(r'\bIWX_[A-Z0-9_]+',source))-skip
while pending:
 n=pending.pop()
 if n in defs:continue
 m=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',reg,re.M) or re.search(r'\b'+n+r'\s*=\s*([^,\n]+)',reg);assert m,n
 defs[n]=m[1];pending|=set(re.findall(r'\bIWX_[A-Z0-9_]+',m[1]))-skip-set(defs)
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <iostream>
#include <cerrno>
using IOReturn=int;using IOOptionBits=unsigned;using UInt64=uint64_t;
constexpr IOReturn kIOReturnSuccess=0,kIOReturnBadArgument=int(0xe00002c2U),kIOReturnNotReady=int(0xe00002d8U);
constexpr unsigned kIODirectionIn=1,kIODirectionOut=2,kIODirectionOutIn=3,kIODirectionInOut=3,kForceDoubleBuffer=0x01000000;
constexpr unsigned kWalkSyncIn=1,kWalkSyncOut=2,kWalkSyncAlways=4,PAGE_SIZE=4096,IWX_DQA_CMD_QUEUE=0;
#define BIT(x) (1U<<(x))
#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) ((uint64_t)(x))
#define IWX_CSR_HW_REV_STEP(x) (((x)>>2)&3)
#define IWX_TFD_QUEUE_CB_SIZE(x) (7)
#define IWX_RX_QUEUE_CB_SIZE(x) (9)
#define XYLog(...) ((void)0)
#define DEVNAME(sc) "test"
#define IWX_WRITE64(sc,r,v) (this->write(r,v))
#define IWX_WRITE(sc,r,v) (this->write(r,v))
#define IWX_SETBITS(sc,r,v) (this->write(r,v))
struct Memory {std::vector<uint8_t> bytes;explicit Memory(unsigned n):bytes(n,0){};};
struct IODMACommandInternal {bool fDoubleBuffer=false,fCursor=false,fForceDoubleBuffer=false;uint64_t fPreparedOffset=0,fPreparedLength=0;};
struct IODMACommand {
 IODMACommandInternal own,*fInternalState=&own;int fActive=1;Memory*memory=nullptr;void*identity=nullptr;
 std::vector<uint8_t> device;std::vector<unsigned> options;int fail=0;unsigned walk=0;
 IOReturn synchronize(IOOptionBits);void*getIOMemoryDescriptor(){return identity;}
 IOReturn complete(bool,bool){return 0;}IOReturn prepare(uint64_t,uint64_t,bool,bool){return 0;}
 IOReturn walkAll(unsigned op){++walk;if(fail)return fail;
  if(op&kWalkSyncOut)device=memory->bytes;else if(op&kWalkSyncIn)memory->bytes=device;return 0;}
};
'''
# Record options without rewriting the Apple implementation: call through subclass.
post=r'''
struct Command:IODMACommand {IOReturn synchronize(IOOptionBits op){options.push_back(op);return IODMACommand::synchronize(op);}};
struct iwx_dma_info {uint64_t paddr=0;void*vaddr=nullptr;size_t size=0;Memory*buffer=nullptr;Command*cmd=nullptr;};
struct iwx_prph_scratch_ctrl_cfg {struct{uint16_t version,mac_id,size;}version;struct{uint64_t free_rbd_addr;}rbd_cfg;struct{uint32_t control_flags;}control;};
struct iwx_context_info_dram {uint64_t placeholder[64];};
struct iwx_prph_scratch {iwx_prph_scratch_ctrl_cfg ctrl_cfg;iwx_context_info_dram dram;};
struct iwx_context_info_gen3 {uint64_t prph_info_base_addr,prph_scratch_base_addr;uint32_t prph_scratch_size;uint64_t cr_head_idx_arr_base_addr,tr_tail_idx_arr_base_addr,cr_tail_idx_arr_base_addr,mtr_base_addr,mcr_base_addr;uint16_t mtr_size,mcr_size;};
struct iwx_fw_sects{};
struct Provider {std::map<std::string,uint64_t>numbers;std::map<std::string,std::string>strings;
 void setProperty(const char*,const void*,unsigned){assert(false && "wrong property overload");}
 void setProperty(const char*k,const char*v){strings[k]=v;}void setProperty(const char*k,uint64_t v,unsigned){numbers[k]=v;}};
struct iwx_softc {iwx_dma_info ctxt_info_dma,prph_scratch_dma,prph_info_dma,iml_dma;
 struct{iwx_dma_info free_desc_dma,used_desc_dma,stat_dma;unsigned count=512;}rxq;
 struct{iwx_dma_info desc_dma;}txq[1];unsigned sc_hw_rev=0x478,sc_device_family=999,sc_iml_len=16;
 char sc_iml[16]="firmware";Provider provider;Provider*sc_pcitag=&provider;
 struct{bool active=true;uint64_t rom_start_time_ns=0;}sc_boot_diag;
};
static uint64_t iwx_boot_time_ns(){return 10;}
static void iwx_bz_record_pci_irq_mode(iwx_softc*,bool){}
static void *rxZcArmAction=nullptr;
struct Loop {int result=0;int runAction(void*,void*){return result;}};
struct ItlIwx {
 bool fBzAdmitted=true,fDma5Published=false,fBzHardwareOwned=false;unsigned fBzFault=0;
 Loop loop;unsigned allocations=0,failAllocation=0,barriers=0,doorbells=0,freed=0;int sectionError=0;unsigned wireNoOwnership=0;
 iwx_dma_info*bad=nullptr;unsigned badKind=0;std::vector<std::unique_ptr<Memory>>mems;std::vector<std::unique_ptr<Command>>commands;
 std::vector<unsigned>registers;
 int bzSyncBootMemory(iwx_softc*);int iwx_bz_port_ctxt_info_gen3_init(iwx_softc*,const iwx_fw_sects*);
 Loop*getMainWorkLoop(){return &loop;}void bzFail(unsigned v,int){fBzFault=v;}
 void setup(iwx_dma_info*d,unsigned size){mems.emplace_back(new Memory(size));commands.emplace_back(new Command);
  d->buffer=mems.back().get();d->cmd=commands.back().get();d->cmd->memory=d->buffer;d->cmd->identity=d->buffer;
  d->cmd->device.assign(size,0xa5);d->cmd->own.fPreparedLength=size;
  d->size=size;d->vaddr=d->buffer->bytes.data();d->paddr=0x100000+mems.size()*0x10000;
 }
 int iwx_dma_contig_alloc(void*,iwx_dma_info*d,unsigned n,unsigned){if(++allocations==failAllocation)return ENOMEM;setup(d,n);return 0;}
 int iwx_init_fw_sec(iwx_softc*,const iwx_fw_sects*,iwx_context_info_dram*){return sectionError;}
 void iwx_ctxt_info_free_fw_img(iwx_softc*){++freed;}
 void iwx_dma_contig_free(iwx_dma_info*){++freed;assert(!fBzHardwareOwned);}
 void iwx_record_boot_dma(iwx_softc*,const iwx_fw_sects*){}
 // Inject AFTER successful allocation/CPU construction, at the actual sync
 // boundary. A null vaddr before construction violates allocator preconditions.
 void iwx_enable_fwload_interrupt(iwx_softc*){if(bad){auto*d=bad;
  if(badKind==1)d->cmd->fActive=0;if(badKind==2)d->cmd->identity=this;
  if(badKind==3)d->cmd->fail=-703;if(badKind==4)d->cmd=nullptr;
  if(badKind==5)d->buffer=nullptr;if(badKind==6)d->vaddr=nullptr;if(badKind==7)d->size=0;}}

 void iwx_dma_queue_publish_barrier(){++barriers;}
 void iwx_set_ltr(iwx_softc*){}
 bool iwx_nic_lock(iwx_softc*){return true;}void iwx_nic_unlock(iwx_softc*){}
 void iwx_write_umac_prph(iwx_softc*,unsigned,unsigned){}
 void write(unsigned reg,uint64_t){++doorbells;registers.push_back(reg);if(!fBzHardwareOwned||!fDma5Published||!barriers)++wireNoOwnership;}
 void initialize(iwx_softc&s){s.sc_device_family=IWX_DEVICE_FAMILY_BZ;
  setup(&s.ctxt_info_dma,sizeof(iwx_context_info_gen3));setup(&s.rxq.free_desc_dma,8192);
  setup(&s.rxq.used_desc_dma,2048);setup(&s.rxq.stat_dma,2);setup(&s.txq[0].desc_dma,32768);}
};
'''
post=post.replace('struct iwx_softc {','struct iwx_softc {void*sc_dmat=nullptr;')
base_main=r'''
int main(){unsigned cases=0;
 {Memory m(16);Command c;c.memory=&m;c.device.assign(16,0xa5);assert(c.synchronize(3)==kIOReturnBadArgument);assert(c.walk==0);++cases;
  m.bytes[0]=42;assert(c.synchronize(2)==0&&c.device[0]==42);c.device[0]=17;assert(c.synchronize(1)==0&&m.bytes[0]==17);++cases;}
 {ItlIwx x;iwx_softc s;x.initialize(s);iwx_fw_sects f;int e=x.iwx_bz_port_ctxt_info_gen3_init(&s,&f);
#ifdef P1
  assert(e==EIO&&!x.fBzHardwareOwned&&!x.fDma5Published&&x.doorbells==0&&x.barriers==0);
  assert(s.ctxt_info_dma.cmd->options==std::vector<unsigned>{3});
  std::cout<<"REPRODUCED P1: actual context+Apple synchronize -> EIO before ROM, no publication\n";
#else
  assert(!e&&x.fBzHardwareOwned&&x.fDma5Published&&x.doorbells==6&&x.barriers==1&&!x.wireNoOwnership);
  assert(s.provider.numbers["KGP_BE200_BOOT_SYNC_INDEX"]==8);
  for(auto&c:x.commands){assert(c->options==std::vector<unsigned>{2});assert(c->device==c->memory->bytes);}
  assert(x.registers.front()==IWX_CSR_CTXT_INFO_ADDR);assert(x.registers.back()==IWX_CSR_GP_CNTRL);
#endif
 ++cases;}
#ifndef P1
 for(unsigned region=0;region<8;++region)for(unsigned kind=1;kind<=7;++kind){
  ItlIwx x;iwx_softc s;iwx_dma_info*regions[]={&s.ctxt_info_dma,&s.prph_scratch_dma,&s.prph_info_dma,&s.iml_dma,&s.rxq.free_desc_dma,&s.rxq.used_desc_dma,&s.rxq.stat_dma,&s.txq[0].desc_dma};
  x.bad=regions[region];x.badKind=kind;x.initialize(s);iwx_fw_sects f;
  assert(x.iwx_bz_port_ctxt_info_gen3_init(&s,&f)==EIO);
  assert(!x.fBzHardwareOwned&&!x.fDma5Published&&!x.doorbells&&!x.barriers&&x.fBzFault==32);
  assert(s.provider.numbers["KGP_BE200_BOOT_SYNC_INDEX"]==region);
  unsigned expected=uint32_t(kind==1?kIOReturnNotReady:kind==3?-703:kIOReturnBadArgument);
  assert(s.provider.numbers["KGP_BE200_BOOT_SYNC_RESULT"]==expected);++cases;
 }
 for(unsigned alloc=1;alloc<=3;++alloc){ItlIwx x;iwx_softc s;x.initialize(s);x.failAllocation=alloc;iwx_fw_sects f;
  assert(x.iwx_bz_port_ctxt_info_gen3_init(&s,&f)==ENOMEM&&!x.doorbells&&!x.fBzHardwareOwned);++cases;}
 {ItlIwx x;iwx_softc s;x.initialize(s);x.sectionError=EIO;iwx_fw_sects f;assert(x.iwx_bz_port_ctxt_info_gen3_init(&s,&f)==EIO&&!x.doorbells);++cases;}
 {ItlIwx x;iwx_softc s;x.initialize(s);x.loop.result=EIO;iwx_fw_sects f;assert(x.iwx_bz_port_ctxt_info_gen3_init(&s,&f)==ENXIO&&!x.allocations&&!x.doorbells);++cases;}
 {ItlIwx x;iwx_softc s;x.initialize(s);x.fBzAdmitted=false;iwx_fw_sects f;assert(x.iwx_bz_port_ctxt_info_gen3_init(&s,&f)==ENXIO&&!x.allocations&&!x.doorbells);++cases;}
#endif
 std::cout<<"PASS "<<cases<<" actual boot/API scenarios; mocked allocation/walkAll/MMIO; no physical proof\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-boot-api-')as d:
 d=Path(d)
 constants='\n'.join('#define '+k+' '+v for k,v in defs.items())+'\n'
 for label,body in [('P1',function(old,context)),('P2',function(current,context))]:
  f=d/(label+'.cpp');f.write_text(pre+constants+applebody+post+(''if label=='P1'else helper)+body+base_main)
  cmd=[C,'-isysroot',SDK,'-std=c++17','-O1','-g','-fsanitize='+san]+(['-DP1']if label=='P1'else[])+[str(f),'-o',str(d/label)]
  print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(d/label)],check=True,timeout=30)
print('APPLE_REFERENCE_SHA256',hashlib.sha256(apple.read_bytes()).hexdigest())
