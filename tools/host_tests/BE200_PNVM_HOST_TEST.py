#!/usr/bin/env python3
"""Actual P2/current PNVM loader/lookup/inflate/section code with embedded PNVM.
Allocation/DMA/sync/MMIO/wait mocked. Does not prove FW acceptance or runtime DMA.
"""
from pathlib import Path
import re,subprocess,tempfile,sys,struct,zlib
R=Path(__file__).resolve().parents[2];G='/Applications/Xcode.app/Contents/Developer/usr/bin/git'
C='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
SDK='/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk'
def fn(s,name):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:static inline\s+)?(?:const char \*|int|void|bool|OSData \*)\s*'+re.escape(name).replace('::',r'::\s*')+r'\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
source=(R/'itlwm/hal_iwx/ItlIwxBz.inc').read_text();cpp=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()+(R/'itlwm/hal_iwx/if_iwxvar.h').read_text()
old=subprocess.check_output([G,'show','5dc1f094e9f982285a842b50b2df45ed0a9e5d18:itlwm/hal_iwx/ItlIwxBz.inc'],cwd=R,text=True)
readname='ItlIwx::iwx_bz_port_read_pnvm';section=fn(source,'ItlIwx::iwx_bz_port_pnvm_handle_section');load=fn(source,'ItlIwx::iwx_bz_port_load_pnvm')
fwheader=(R/'include/FwData.h').read_text();getfw=fn(fwheader,'getFWDescByName');inflate=fn(fwheader,'uncompressFirmware')
reverse=fn((R/'itl80211/linux/kernel.h').read_text(),'reverse_strchr')
embedded=(R/'include/FwBinary.cpp').read_text();m=re.search(r'const unsigned char iwlwifi_gl_c0_fm_c0_pnvm\[\] = \{(.*?)\};',embedded,re.S);assert m
packed=bytes(int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{2})',m[1]));assert zlib.decompress(packed)==(R/'itlwm/firmware/be200-gl92/iwlwifi-gl-c0-fm-c0.pnvm').read_bytes()
constants=''
for n in sorted(set(re.findall(r'\bIWX_[A-Z0-9_]+',section+load+fn(source,readname)))):
 x=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',reg,re.M)or re.search(r'\b'+n+r'\s*=\s*([^,\n]+)',reg);assert x,n
 constants+='#define '+n+' '+x[1]+'\n'
for n in ['CSR_HW_REV_TYPE','CSR_HW_RFID_TYPE']:
 x=re.search(r'^#define '+n+r'\([^\n]+',reg,re.M);assert x;constants+=x[0]+'\n'
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <fstream>
#include <iostream>
#include <cerrno>
#include <zlib.h>
#include "itlwm/hal_iwx/ItlIwxBzContracts.hpp"
using UInt32=uint32_t;using u32=uint32_t;using u_char=unsigned char;using uint=unsigned;using __le16=uint16_t;using __le32=uint32_t;
constexpr int kIOReturnSuccess=0,kIODirectionOut=2;
#define le16_to_cpup(p) (*(p))
#define le32_to_cpup(p) (*(p))
#define le32toh(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)
#define isset(a,b) ((a)[(b)/8]&(1U<<((b)%8)))
#define roundup(x,y) (((x)+(y)-1)&~((y)-1))
#define XYLog(...) ((void)0)
#define DEVNAME(sc) "mock"
#define __packed __attribute__((packed))
struct iwx_ucode_tlv {uint32_t type,length;};struct iwx_sku_id {uint32_t data[3];};
struct iwx_pnvm_section {uint32_t offset;const uint8_t data[];}__packed;
static int allocationFailure=0,heapAllocations=0;
static void *kernelMalloc(size_t n,int,int){if(++heapAllocations==allocationFailure)return nullptr;return std::malloc(n);}
static voidpf zcalloc(voidpf,unsigned n,unsigned size){return std::calloc(n,size);}static void zcfree(voidpf,voidpf p){std::free(p);}
struct OSData {std::vector<uint8_t>data;static OSData*withBytes(const void*p,unsigned n){auto*r=new OSData;r->data.assign((const uint8_t*)p,(const uint8_t*)p+n);return r;}
 unsigned getLength(){return data.size();}const void*getBytesNoCopy(){return data.data();}void release(){delete this;}};
#define OSSafeReleaseNULL(p) do{if(p)(p)->release();(p)=nullptr;}while(0)
struct FwDesc {const char*name;const unsigned char*var;int size;};
static FwDesc fwList[1];static int fwNumber=1;
struct Command {int status=0;unsigned syncs=0;int synchronize(int direction){assert(direction==2);++syncs;return status;}};
struct DMA {void*vaddr=nullptr;uint64_t paddr=0;size_t size=0;Command*cmd=nullptr;};
struct Provider {std::map<std::string,uint64_t>n;std::map<std::string,std::string>s;
 void setProperty(const char*,const void*,unsigned){assert(false);}
 void setProperty(const char*k,uint64_t v,unsigned){n[k]=v;}void setProperty(const char*k,const char*v){s[k]=v;}};
struct iwx_fw_info {void*pnvm_rawdata=nullptr;size_t pnvm_rawsize=0;};
struct iwx_prph_scratch_ctrl_cfg {struct{uint64_t pnvm_base_addr;uint32_t pnvm_size;}pnvm_cfg;};
struct iwx_prph_scratch {iwx_prph_scratch_ctrl_cfg ctrl_cfg;};
struct iwx_softc {unsigned sc_device_family=3;uint32_t sc_hw_rev=0x478,sc_hw_rf_id=0x112200;
 unsigned char sc_enabled_capa[16]={};const char*sc_fwname=kgp_bz::FirmwareName;
 uint32_t sku_id[3]={};Provider provider;Provider*sc_pcitag=&provider;iwx_fw_info sc_fw;
 DMA pnvm_desc_dma,pnvm_dram,pnvm_frag_dma[64],prph_scratch_dma;void*sc_dmat=nullptr;
 int sc_generation=1;int sc_init_complete=0;uint32_t sc_pnvm_status=0;};
static uint64_t iwx_boot_time_ns(){static unsigned n=0;return ++n;}
static void ItlStartupStage(Provider*p,const char*s){p->s["stage"]=s;}
static void ItlStartupFailure(Provider*p,const char*s,int e){p->s["stage"]=s;p->n["error"]=e;}
struct iwl_pnvm_init_complete_ntfy {uint32_t status;};
struct ItlIwx {kgp_bz::PnvmLedger fBzPnvm;unsigned fPc1ResetEpoch=1,fBzFault=0;uint64_t fBzStats[32]={};
 struct Gate {bool inGate(){return true;}void commandWakeup(void*){}}gate;Gate*getMainWorkLoop(){return &gate;}Gate*getMainCommandGate(){return &gate;}
 void bzPublishInitCounters(iwx_softc*){}void bzPnvmComplete(iwx_softc*,const void*,size_t);
 bool hardwareOwned=true,lock=true;int failDma=0,allocs=0,doorbells=0,barriers=0,fault=0,waitResult=0;
 std::vector<std::unique_ptr<uint8_t[]>>buffers;std::vector<std::unique_ptr<Command>>commands;
 int iwx_bz_port_read_pnvm(iwx_softc*);int iwx_bz_port_pnvm_handle_section(iwx_softc*,const uint8_t*,size_t);int iwx_bz_port_load_pnvm(iwx_softc*);void iwx_pnvm_free(iwx_fw_info*);
 int iwx_read_pnvm(iwx_softc*s){return iwx_bz_port_read_pnvm(s);}int iwx_pnvm_handle_section(iwx_softc*s,const uint8_t*p,size_t n){return iwx_bz_port_pnvm_handle_section(s,p,n);}
 int iwx_dma_contig_alloc(void*,DMA*d,size_t n,unsigned){if(++allocs==failDma)return ENOMEM;
  buffers.emplace_back(new uint8_t[n]{});commands.emplace_back(new Command);d->cmd=commands.back().get();d->vaddr=buffers.back().get();d->size=n;d->paddr=0x100000+allocs*0x10000;return 0;}
 void iwx_dma_contig_free(DMA*d){if(hardwareOwned)return;*d={};}
 void bzFail(unsigned why,int){fault=why;fBzFault=why;}
 void iwx_dma_queue_publish_barrier(){++barriers;}
 bool iwx_nic_lock(iwx_softc*){return lock;}void iwx_nic_unlock(iwx_softc*){}
 void iwx_write_umac_prph(iwx_softc*,unsigned,unsigned){++doorbells;assert(barriers);}
 int iwx_wait_notification(iwx_softc*s,int*v,int bits,uint32_t){if(!waitResult){uint32_t status=0x50001;bzPnvmComplete(s,&status,sizeof(status));assert((*v&bits)==bits);}return waitResult;}
 void setup(iwx_softc&s){s.sc_device_family=IWX_DEVICE_FAMILY_BZ;s.sc_enabled_capa[IWX_UCODE_TLV_CAPA_FRAGMENTED_PNVM_IMG/8]|=1U<<(IWX_UCODE_TLV_CAPA_FRAGMENTED_PNVM_IMG%8);
  assert(!iwx_dma_contig_alloc(nullptr,&s.prph_scratch_dma,sizeof(iwx_prph_scratch),0));}
};
'''
main=r'''
static std::vector<uint8_t>original,resource,plain;
static void resetResource(){resource=original;fwList[0]={"iwlwifi-gl-c0-fm-c0.pnvm",resource.data(),int(resource.size())};fwNumber=1;heapAllocations=allocationFailure=0;}
static void setRaw(const std::vector<uint8_t>&b){resource.resize(compressBound(b.size()));uLongf n=resource.size();assert(compress2(resource.data(),&n,b.data(),b.size(),6)==Z_OK);resource.resize(n);fwList[0]={"iwlwifi-gl-c0-fm-c0.pnvm",resource.data(),int(resource.size())};}
struct Entry {uint32_t sku[3];std::vector<std::vector<uint8_t>>chunks;};
static std::vector<Entry>entries(){std::vector<Entry>r;for(size_t at=0;at<plain.size();){unsigned t=kgp_bz::le32(plain.data()+at),n=kgp_bz::le32(plain.data()+at+4);auto*d=plain.data()+at+8;
 if(t==64){Entry x{};std::memcpy(x.sku,d,12);r.push_back(x);}else if(t==19){assert(n>=4&&!r.empty());r.back().chunks.emplace_back(d+4,d+n);}at+=8+((n+3)&~3U);}return r;}
int main(int argc,char**argv){assert(argc==2);std::ifstream f(argv[1],std::ios::binary);original={std::istreambuf_iterator<char>(f),{}};assert(!original.empty());plain.resize(1024*1024);uLongf length=plain.size();assert(uncompress(plain.data(),&length,original.data(),original.size())==Z_OK);plain.resize(length);auto e=entries();assert(e.size()==16);unsigned cases=0;
#ifdef OLD_P2
 {resetResource();ItlIwx x;iwx_softc s;x.setup(s);std::memcpy(s.sku_id,e[0].sku,12);assert(x.iwx_bz_port_load_pnvm(&s)==EINVAL);assert(s.provider.s["ItlwmPnvmName"]=="iwlwifi-gl-c0-fm-c0-.pnvm");assert(s.provider.n["ItlwmPnvmFailStep"]==2&&!x.doorbells);++cases;}
 std::cout<<"REPRODUCED P2 actual compat strrchr + actual loader -> extra hyphen,missing PNVM,before doorbell\n";
#else
 assert(std::strcmp(kgp_bz::pnvmForFirmware(kgp_bz::FirmwareName),"iwlwifi-gl-c0-fm-c0.pnvm")==0);assert(!kgp_bz::pnvmForFirmware(nullptr));
 for(auto*n:{"", "iwlwifi-gl-c0-fm-c0-93.ucode","iwlwifi-gl-c0-fm-c0-92.ucodeX","iwlwifi-bz-b0-fm-b0-92.ucode","-"})assert(!kgp_bz::pnvmForFirmware(n));cases+=7;
 for(auto&entry:e){resetResource();ItlIwx x;iwx_softc s;x.setup(s);std::memcpy(s.sku_id,entry.sku,12);assert(!x.iwx_bz_port_load_pnvm(&s));
 assert(s.provider.s["ItlwmPnvmName"]=="iwlwifi-gl-c0-fm-c0.pnvm"&&s.provider.n["ItlwmPnvmDescFound"]==1);
 assert(s.provider.n["ItlwmPnvmChunkCount"]==entry.chunks.size());auto*desc=(uint64_t*)s.pnvm_desc_dma.vaddr;size_t sum=0;
 for(unsigned i=0;i<entry.chunks.size();++i){assert(desc[i]==s.pnvm_frag_dma[i].paddr);assert(s.pnvm_frag_dma[i].size==entry.chunks[i].size());assert(!std::memcmp(s.pnvm_frag_dma[i].vaddr,entry.chunks[i].data(),entry.chunks[i].size()));assert(s.pnvm_frag_dma[i].cmd->syncs==1);sum+=entry.chunks[i].size();}
 auto&cfg=((iwx_prph_scratch*)s.prph_scratch_dma.vaddr)->ctrl_cfg.pnvm_cfg;assert(cfg.pnvm_base_addr==s.pnvm_desc_dma.paddr&&cfg.pnvm_size==sum);
 assert(s.pnvm_desc_dma.cmd->syncs==1&&s.prph_scratch_dma.cmd->syncs==1&&x.doorbells==1&&x.barriers==1&&!x.fault&&!s.sc_fw.pnvm_rawdata);++cases;
 }
 for(unsigned which=0;which<8;++which){resetResource();ItlIwx x;iwx_softc s;x.setup(s);std::memcpy(s.sku_id,e[0].sku,12);int expected=EINVAL;
  if(which==0)s.sc_fwname="unknown-92.ucode";
  if(which==1)fwNumber=0;
  if(which==2){resource.assign(4,0);fwList[0].var=resource.data();fwList[0].size=4;}
  if(which==3){auto r=plain;r.pop_back();setRaw(r);}
  if(which==4){std::vector<uint8_t>r={64,0,0,0,4,0,0,0,1,0,0,0};setRaw(r);}
  if(which==5){s.sku_id[0]=0xdeadbeef;expected=ENOENT;}
  if(which==6){s.sc_hw_rf_id=0x113200;expected=ENOENT;}
  if(which==7){allocationFailure=1;expected=ENOMEM;}
  int got=x.iwx_bz_port_load_pnvm(&s);assert(got==expected&&!x.doorbells&&!s.sc_fw.pnvm_rawdata);++cases;
 }
 for(unsigned which=0;which<4;++which){resetResource();ItlIwx x;iwx_softc s;x.setup(s);std::memcpy(s.sku_id,e[0].sku,12);x.failDma=2+which;
  if(which==3){x.failDma=0;x.lock=false;}
  assert(x.iwx_bz_port_load_pnvm(&s)!=0&&!x.doorbells);x.iwx_pnvm_free(&s.sc_fw);++cases;}
 {resetResource();ItlIwx x;iwx_softc s;x.setup(s);std::memcpy(s.sku_id,e[0].sku,12);x.waitResult=EWOULDBLOCK;assert(x.iwx_bz_port_load_pnvm(&s)==EWOULDBLOCK&&x.doorbells==1);++cases;}
 {resetResource();ItlIwx x;iwx_softc s;x.setup(s);std::memcpy(s.sku_id,e[0].sku,12);assert(!x.iwx_bz_port_read_pnvm(&s));auto*retained=s.pnvm_desc_dma.vaddr;assert(s.sc_fw.pnvm_rawdata);int result=x.iwx_bz_port_read_pnvm(&s);assert(result==EBUSY&&s.pnvm_desc_dma.vaddr==retained&&!x.doorbells);x.iwx_pnvm_free(&s.sc_fw);++cases;}
#endif
 std::cout<<"PASS "<<cases<<" actual PNVM scenarios; mock DMA/notification; not firmware acceptance proof\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-pnvm-')as d:
 d=Path(d);(d/'embedded-pnvm.zlib').write_bytes(packed)
 for tag,reader in [('P2',fn(old,readname)),('P4',fn(source,readname))]:
  body=pre.replace('using UInt32',constants+'\nusing UInt32',1)+'\nstatic char *kernel_strchr(const char*s,int c){return const_cast<char*>(std::strchr(s,c));}\n#define strchr(a,b) kernel_strchr(a,b)\n'+reverse+'\n#undef strchr\n'+'\n#define strrchr(x,y) reverse_strchr(x,y)\n'+getfw+inflate+'\n#define malloc(a,b,c) kernelMalloc(a,b,c)\n'+fn(cpp,'ItlIwx::iwx_pnvm_free')+section+reader+fn((R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text(),'ItlIwx::bzPnvmComplete')+load+'\n#undef malloc\n'+main
  (d/(tag+'.cpp')).write_text(body)
  cmd=[C,'-isysroot',SDK,'-std=gnu++17','-O1','-g','-fsanitize='+san,'-I'+str(R)]+(['-DOLD_P2']if tag=='P2'else[])+[str(d/(tag+'.cpp')),'-lz','-o',str(d/tag)]
  print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(d/tag),str(d/'embedded-pnvm.zlib')],check=True,timeout=60)
