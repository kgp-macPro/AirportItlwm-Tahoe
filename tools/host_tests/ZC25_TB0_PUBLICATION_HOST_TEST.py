#!/usr/bin/env python3
"""Extract and execute the production ZC25 split-TB0 publication/retirement code.

Uses ZC25_SOURCE_ROOT when running outside the worktree. Models device byte
reads/writes and positive completion; cache/PAT/DMA hardware cost and throughput
are explicitly NOT modeled. No source or production worktree writes.
"""
from pathlib import Path
import hashlib
import os, re, subprocess, sys, tempfile
R=Path(os.environ.get('ZC25_SOURCE_ROOT',Path(__file__).resolve().parents[2]))
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()
def mask(x):
 return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),x,flags=re.S)
def fn(name):
 m=mask(s);q=re.search(r'(?:static\s+)?(?:void|bool|int)\s+(?:ItlIwx::\s*)?'+name+r'\s*\(',m);assert q,name
 a=m.index('{',q.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
def typ(name):
 q=re.search(r'^struct '+name+r' \{.*?^\} __packed;',reg,re.M|re.S);assert q,name;return q[0]
a=s.index('    // Validate/copy before transferring')
b=s.index('    iwx_tx_update_byte_tbl(sc, ring, idx, totlen, num_tbs);',a)+len('    iwx_tx_update_byte_tbl(sc, ring, idx, totlen, num_tbs);')
transfer=s[a:b]
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <cerrno>
#include <limits>
#define __packed __attribute__((packed))
#define IWX_TFH_NUM_TBS 25
#define IWX_FIRST_TB_SIZE 20
#define IWX_FIRST_TB_SIZE_ALIGN 64
#define IWX_DEF_CMD_PAYLOAD_SIZE 320
#define IWX_TFD_QUEUE_BC_SIZE 320
#define IWX_TFD_QUEUE_BC_SIZE_GEN3 1024
#define IWX_DEVICE_FAMILY_AX210 2
#define IWX_DQA_CMD_QUEUE 0
#define IWX_INVALID_QUEUE -1
#define nitems(x) (sizeof(x)/sizeof((x)[0]))
#define htole16(x) (x)
#define le16toh(x) (x)
#define le64toh(x) (x)
#ifndef howmany
#define howmany(x,y) (((x)+((y)-1))/(y))
#endif
struct iwx_cmd_header{uint8_t cmd,group_id,qid,idx;};
struct iwx_cmd_header_wide{uint8_t x[8];};
'''
mid=r'''
struct Mbuf{bool freed=false;};struct Node{int references=1;};
static void mbuf_freem(Mbuf*m){assert(m&&!m->freed);m->freed=true;}
struct Dma{void*vaddr=nullptr;uint64_t paddr=0;size_t size=0;};
struct Data{uint64_t cmd_paddr=0;Mbuf*m=nullptr;Node*in=nullptr;unsigned type=0,token=0;bool quarantined=false;};
struct iwx_tx_ring{Dma desc_dma,cmd_dma,bc_tbl,first_tb_dma;iwx_tfh_tfd*desc=nullptr;iwx_device_cmd*cmd=nullptr;Data data[1024];unsigned ring_count=0;int qid=1;};
struct iwx_softc{int sc_device_family=2;};
struct ItlIwx{bool mapped=true;unsigned fTx2JAdmission=0,fTx2IBlocked=0;static constexpr unsigned kTx2JClosed=0x80000000;
 bool dma5Mapped(){return mapped;}
 void tx2IRetire(Data*d,bool normal){assert(!normal);d->quarantined=d->token!=0;d->token=0;}
 void iwx_tx_update_byte_tbl(iwx_softc*,iwx_tx_ring*,int,uint16_t,uint16_t);
 int transfer(iwx_softc*,iwx_tx_ring*,int,const iwx_tfh_tfd*,const iwx_device_cmd*,Mbuf*,Node*,unsigned,int,unsigned);
};
struct Store{
 uint8_t*p=nullptr;size_t n=0;
 explicit Store(size_t length):n(length){void*x=nullptr;assert(!posix_memalign(&x,64,n+64));p=(uint8_t*)x;memset(p,0xa5,n+64);}
 ~Store(){free(p);}
 Store(const Store&)=delete;Store&operator=(const Store&)=delete;
 std::vector<uint8_t> snapshot()const{return std::vector<uint8_t>(p,p+n+64);}
 bool equals(const std::vector<uint8_t>&old)const{return old.size()==n+64&&!memcmp(p,old.data(),n+64);}
 void assertExcept(const std::vector<uint8_t>&old,size_t offset,size_t length)const{
  assert(old.size()==n+64&&offset+length<=n);
  assert(!memcmp(p,old.data(),offset));
  assert(!memcmp(p+offset+length,old.data()+offset+length,n+64-offset-length));
 }
};
struct Ring{
 iwx_tx_ring r;Store d,c,bc,tb0;
 explicit Ring(unsigned count):d(count*256),c(count*324),bc(2048),tb0(count*64){
  r.ring_count=count;r.desc=(iwx_tfh_tfd*)d.p;r.cmd=(iwx_device_cmd*)c.p;
  r.desc_dma={d.p,0x123400000ULL,d.n};r.cmd_dma={c.p,0x234500000ULL,c.n};r.bc_tbl={bc.p,0x345600000ULL,bc.n};
  r.first_tb_dma={tb0.p,0x456700000ULL,tb0.n};
  for(unsigned i=0;i<count&&i<nitems(r.data);++i)r.data[i].cmd_paddr=r.cmd_dma.paddr+i*324;
 }
};
static void prepare(Ring&v,unsigned slot,unsigned nt,unsigned commandBytes,iwx_tfh_tfd&d,iwx_device_cmd&c,unsigned tag=1){
 memset(&d,0,sizeof(d));memset(&c,0,sizeof(c));d.num_tbs=nt;d.tbs[0].tb_len=20;d.tbs[0].addr=v.r.first_tb_dma.paddr+uint64_t(slot)*64;
 d.tbs[1].tb_len=commandBytes-20;d.tbs[1].addr=v.r.data[slot].cmd_paddr+20;
 for(unsigned i=2;i<nt;++i){d.tbs[i].tb_len=1200;d.tbs[i].addr=0x567800000ULL+i*4096;}
 auto*p=(uint8_t*)&c;for(unsigned i=0;i<commandBytes;++i)p[i]=(i+tag)&255;
}
static void assertWire(Ring&v,unsigned slot,unsigned fetched,unsigned cb,const iwx_tfh_tfd&d,const iwx_device_cmd&c){
 assert(!memcmp(&v.r.desc[slot],&d,fetched));
 assert(d.tbs[0].addr==v.r.first_tb_dma.paddr+uint64_t(slot)*64);
 assert(d.tbs[1].addr==v.r.cmd_dma.paddr+uint64_t(slot)*324+20);
 assert(d.tbs[0].addr!=d.tbs[1].addr-20); // physically separate DMA objects, same concatenated bytes
 assert(!memcmp(v.tb0.p+slot*64,&c,20));
 assert(!memcmp(v.c.p+slot*324+20,(const uint8_t*)&c+20,cb-20));
 // A simulated hardware reader concatenates TB0 and TB1, exactly preserving
 // the original command image despite their non-contiguous bus addresses.
 std::vector<uint8_t> wire(cb);memcpy(wire.data(),v.tb0.p+slot*64,20);memcpy(wire.data()+20,v.c.p+slot*324+20,cb-20);
 assert(!memcmp(wire.data(),&c,cb));
 assert((d.tbs[0].addr&63)==0);
 assert((d.tbs[0].addr>>12)==((d.tbs[0].addr+19)>>12));
 assert((d.tbs[0].addr>>32)==((d.tbs[0].addr+19)>>32));
 assert(d.tbs[0].addr!=(uintptr_t)&c&&d.tbs[0].addr!=(uintptr_t)&d);
 assert(d.tbs[1].addr!=(uintptr_t)&c&&d.tbs[1].addr!=(uintptr_t)&d);
}
'''
transfer_decl='''int ItlIwx::transfer(iwx_softc*sc,iwx_tx_ring*ring,int idx,const iwx_tfh_tfd*desc,const iwx_device_cmd*cmd,Mbuf*m,Node*in,unsigned type,int totlen,unsigned num_tbs){auto*data=&ring->data[idx];\n'''+transfer+'\nreturn 0;}\n'
test=r'''
int main(){unsigned cases=0;
 static_assert(sizeof(iwx_device_cmd)==324,"same production command stride");
 // Full TFD/command shape coverage, with every fetched reserved byte compared.
 for(unsigned count:{16U,32U,128U,256U,1024U}){
  Ring v(count);
  for(unsigned slot:{0U,count/2,count-1})for(unsigned nt=2;nt<=25;++nt)for(unsigned cb:{24U,36U,44U,52U,60U,68U,320U,324U}){
   iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,slot,nt,cb,d,c);
   auto bd=v.d.snapshot(),bc=v.c.snapshot(),bt=v.tb0.snapshot(),bb=v.bc.snapshot();
   assert(iwx_tx_publish_control(&v.r,slot,&d,&c));unsigned fetched=(2+nt*10+63)&~63U;
   assertWire(v,slot,fetched,cb,d,c);
   v.d.assertExcept(bd,slot*256,fetched);v.c.assertExcept(bc,slot*324+20,cb-20);v.tb0.assertExcept(bt,slot*64,20);assert(v.bc.equals(bb));
   ItlIwx driver;iwx_softc sc;
   for(int family:{1,2}){sc.sc_device_family=family;unsigned bcslot=family==2?slot:(slot<256?slot:255);driver.iwx_tx_update_byte_tbl(&sc,&v.r,bcslot,1200,nt);
    auto word=((uint16_t*)v.bc.p)[bcslot];unsigned chunks=family==2?word>>14:word>>12;assert((chunks+1)*64==fetched);}
   // Device writeback changes ONLY first-TB backing, never staging, cached
   // command bytes, read-only descriptor/BC storage or another slot's TB0.
   auto cachedC=v.c.snapshot(),cachedD=v.d.snapshot(),cachedB=v.bc.snapshot(),oldT=v.tb0.snapshot();
   memset(v.tb0.p+slot*64,0x7b,20);
   assert(v.c.equals(cachedC)&&v.d.equals(cachedD)&&v.bc.equals(cachedB));v.tb0.assertExcept(oldT,slot*64,20);assert(((uint8_t*)&c)[0]==1);
   sc.sc_device_family=2;iwx_clear_tx_desc(&sc,&v.r,slot);assert(!v.r.desc[slot].num_tbs);
   assert(!memcmp((char*)&v.r.desc[slot]+2,(char*)&d+2,fetched-2));
   prepare(v,slot,3,60,d,c,2);assert(iwx_tx_publish_control(&v.r,slot,&d,&c));assertWire(v,slot,64,60,d,c);
   ++cases;
  }
 }
 // Every physical slot, including cacheline/page edge positions of the old
 // 324-byte stride. The split TB0 is always isolated to its own 64-byte unit.
 for(unsigned count:{16U,32U,128U,256U,1024U}){Ring v(count);for(unsigned i=0;i<count;++i){iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,i,3,60,d,c,i);assert(iwx_tx_publish_control(&v.r,i,&d,&c));assertWire(v,i,64,60,d,c);++cases;}}
 // Adversarial DMA base near a4GiB edge, while all TB0s remain64B-aligned.
 {Ring v(256);v.r.first_tb_dma.paddr=0xffffc000ULL;for(unsigned i=0;i<256;++i){iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,i,3,60,d,c,i);assert(iwx_tx_publish_control(&v.r,i,&d,&c));assertWire(v,i,64,60,d,c);++cases;}}
 // Reject malformed descriptions BEFORE writing any of the4 DMA views.
 for(unsigned fault=0;fault<29;++fault){Ring v(16);iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,3,60,d,c);
  unsigned slot=0;auto*dp=&d;auto*cp=&c;
  switch(fault){case 0:d.num_tbs=1;break;case 1:d.num_tbs=26;break;case 2:d.tbs[0].tb_len=19;break;case 3:d.tbs[1].tb_len=305;break;
   case 4:d.tbs[1].tb_len=1;break;case 5:slot=16;break;case 6:v.r.qid=0;break;case 7:v.r.desc_dma.size=63;break;
   case 8:v.r.cmd_dma.size=59;break;case 9:++v.r.data[0].cmd_paddr;break;case 10:++d.tbs[0].addr;break;case 11:++d.tbs[1].addr;break;
   case 12:dp=&v.r.desc[0];break;case 13:cp=&v.r.cmd[0];break;case 14:v.r.qid=-1;break;case 15:v.r.cmd=nullptr;break;
   case 16:v.r.first_tb_dma.vaddr=nullptr;break;case 17:v.r.first_tb_dma.size=19;break;
   case 18:++v.r.first_tb_dma.paddr;++d.tbs[0].addr;break;
   case 19:slot=15;prepare(v,slot,3,60,d,c);v.r.first_tb_dma.size=slot*64+19;break;
   case 20:v.r.desc=nullptr;break;case 21:v.r.cmd_dma.vaddr=v.d.p;break;case 22:v.r.desc_dma.vaddr=v.c.p;break;
   // Real64-aligned command backing can place an individual324-strideTB1
   // exactlyat/crossing4GiB. This is distinct from a broken backingalignment.
   case 23:case 24:{slot=fault==23?1:2;v.r.cmd_dma.paddr=fault==23?0xfffffe80ULL:0xfffffd40ULL;
    for(unsigned j=0;j<v.r.ring_count;++j)v.r.data[j].cmd_paddr=v.r.cmd_dma.paddr+uint64_t(j)*324;
    prepare(v,slot,3,60,d,c);break;}
   case 25:slot=1;v.r.first_tb_dma.paddr=UINT64_MAX-63;prepare(v,slot,3,60,d,c);break;
   case 26:slot=1;v.r.cmd_dma.paddr=UINT64_MAX-319;
    for(unsigned j=0;j<v.r.ring_count;++j)v.r.data[j].cmd_paddr=v.r.cmd_dma.paddr+uint64_t(j)*324;
    prepare(v,slot,3,60,d,c);break;
   case 27:v.r.first_tb_dma.size=63;break; //64-byte slot extent, not only20written bytes
   case 28:v.r.first_tb_dma.vaddr=static_cast<uint8_t*>(v.r.first_tb_dma.vaddr)+1;break;

  }
  auto bd=v.d.snapshot(),bc=v.c.snapshot(),bt=v.tb0.snapshot(),bb=v.bc.snapshot();
  assert(!iwx_tx_publish_control(&v.r,slot,dp,cp));assert(v.d.equals(bd)&&v.c.equals(bc)&&v.tb0.equals(bt)&&v.bc.equals(bb));++cases;
 }
 // Actual caller's post-map publication and ownership transfer, both modes.
 for(bool mapped:{false,true})for(bool valid:{false,true}){Ring v(16);iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,3,60,d,c);
  ItlIwx driver;driver.mapped=mapped;Mbuf m;Node node;iwx_softc sc;v.r.data[0].token=mapped?1:0;if(!valid)d.num_tbs=26;
  int result=driver.transfer(&sc,&v.r,0,&d,&c,&m,&node,8,1200,3);
  if(valid){assert(!result&&!m.freed&&v.r.data[0].m==&m&&v.r.data[0].in==&node&&!driver.fTx2IBlocked);}
  else{assert(result==EINVAL&&m.freed&&!v.r.data[0].m&&!v.r.data[0].in&&driver.fTx2IBlocked);assert(v.r.data[0].quarantined==mapped);--node.references;assert(!node.references);}
  ++cases;
 }
 // Queue0 descriptor retirement retains its historical first-TB semantics;
 // the split publication helper is NEVER accepted for firmware command queue.
 {Ring v(16);iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,4,60,d,c);v.r.qid=0;memcpy(v.r.desc,&d,256);iwx_softc sc;iwx_clear_tx_desc(&sc,&v.r,0);
  assert(!v.r.desc[0].num_tbs&&!memcmp(&v.r.desc[0].tbs[0],&d.tbs[0],10));for(int i=1;i<4;++i){iwx_tfh_tb zero{};assert(!memcmp(&v.r.desc[0].tbs[i],&zero,10));}++cases;}
 // Concurrent model of positive completion, including firmware-coordinate
 //65536 wrap vs physical ring256 wrap. StartnearfullFWwraptoexerciseit. No slot reusable before completion.
 {Ring v(256);std::atomic<unsigned>state[256]{};constexpr unsigned loops=20000;
  std::thread consumer([&]{iwx_softc sc;for(unsigned n=0;n<loops;++n){unsigned fw=(65520+n)&65535,slot=fw&255;while(state[slot].load(std::memory_order_acquire)!=1)std::this_thread::yield();
   assert(v.r.desc[slot].num_tbs==3);assert(v.tb0.p[slot*64]==(n&255));assert(v.c.p[slot*324+20]==((n+20)&255));
   memset(v.tb0.p+slot*64,0x7b,20);iwx_clear_tx_desc(&sc,&v.r,slot);state[slot].store(0,std::memory_order_release);}});
  for(unsigned n=0;n<loops;++n){unsigned fw=(65520+n)&65535,slot=fw&255;while(state[slot].load(std::memory_order_acquire)!=0)std::this_thread::yield();
   iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,slot,3,60,d,c,n);assert(iwx_tx_publish_control(&v.r,slot,&d,&c));std::atomic_thread_fence(std::memory_order_seq_cst);state[slot].store(1,std::memory_order_release);}
  consumer.join();++cases;
 }
 std::cout<<"PASS "<<cases<<" actual split-publication cases plus20000 concurrent positive-completion wraps; exact virtual command image,4buffer guards,64B TB0 isolation,device writeback,TFD/BC fetch agreement,all physical slots,both modes,malformed rejection and actual caller ownership branch. Cache/PAT/hardware latency/throughput NOT modeled.\n";
}
'''
print('SOURCE_CPP_SHA256='+hashlib.sha256(s.encode()).hexdigest(),flush=True)
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc25-tb0-publication-') as td:
 p=Path(td);types='\n'.join(typ(n) for n in ['iwx_tfh_tb','iwx_tfh_tfd','iwx_device_cmd','iwx_agn_scd_bc_tbl','iwx_gen3_bc_tbl'])
 (p/'test.cpp').write_text(pre+types+mid+fn('iwx_tx_publish_control')+fn('iwx_clear_tx_desc')+fn('iwx_tx_update_byte_tbl')+transfer_decl+test)
 compiler=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip()
 sdk=subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
 command=[compiler,'-isysroot',sdk,'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('SOURCE_ROOT='+str(R),flush=True);print(command,flush=True)
 subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True,timeout=120)
