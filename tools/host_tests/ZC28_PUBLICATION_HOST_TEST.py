#!/usr/bin/env python3
"""Actual packet publication, completion, byte-count and failure-transfer code.
Mocks cache attributes/hardware; byte-equivalence and ownership models are not RF proof.
"""
from pathlib import Path
import hashlib
import re,subprocess,tempfile,sys,os
R=Path(os.environ.get("ZC28_SOURCE_ROOT",Path(__file__).resolve().parents[2]));s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()
def mask(x):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),x,flags=re.S)
def fn(name):
 m=mask(s);q=re.search(r'(?:static\s+)?(?:void|bool|int)\s+(?:ItlIwx::\s*)?'+name+r'\s*\(',m);assert q,name
 a=m.index('{',q.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
def typ(name):
 q=re.search(r'^struct '+name+r' \{.*?^\} __packed;',reg,re.M|re.S);assert q,name;return q[0]
a=s.index('    // Validate/copy before transferring');b=s.index('    iwx_tx_update_byte_tbl(sc, ring, idx, totlen, num_tbs);',a)+len('    iwx_tx_update_byte_tbl(sc, ring, idx, totlen, num_tbs);')
transfer=s[a:b]
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <cerrno>
#define __packed __attribute__((packed))
#define IWX_TFH_NUM_TBS 25
#define IWX_FIRST_TB_SIZE 20
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
struct iwx_tx_ring{Dma desc_dma,cmd_dma,bc_tbl;iwx_tfh_tfd*desc=nullptr;iwx_device_cmd*cmd=nullptr;Data data[1024];unsigned ring_count=0;int qid=1;};
struct iwx_softc{int sc_device_family=2;};
struct ItlIwx{uint64_t fZc27Stats[128]={};bool mapped=true;unsigned fTx2JAdmission=0,fTx2IBlocked=0;static constexpr unsigned kTx2JClosed=0x80000000;
 bool dma5Mapped(){return mapped;}
 void tx2IRetire(Data*d,bool normal){assert(!normal);d->quarantined=d->token!=0;d->token=0;}
 void iwx_tx_update_byte_tbl(iwx_softc*,iwx_tx_ring*,int,uint16_t,uint16_t);
 int transfer(iwx_softc*,iwx_tx_ring*,int,const iwx_tfh_tfd*,const iwx_device_cmd*,Mbuf*,Node*,unsigned,int,unsigned);
};
struct Ring{
 iwx_tx_ring r;std::vector<uint8_t> d,c,bc;
 explicit Ring(unsigned count):d(count*256+32,0xa5),c(count*324+32,0xa5),bc(2048,0xa5){
  r.ring_count=count;r.desc=(iwx_tfh_tfd*)d.data();r.cmd=(iwx_device_cmd*)c.data();
  r.desc_dma={d.data(),0x123400000ULL,count*256U};r.cmd_dma={c.data(),0x234500000ULL,count*324U};r.bc_tbl={bc.data(),0,bc.size()};
  for(unsigned i=0;i<count;++i)r.data[i].cmd_paddr=r.cmd_dma.paddr+i*324;
 }
};
static void prepare(Ring&v,unsigned slot,unsigned nt,unsigned commandBytes,iwx_tfh_tfd&d,iwx_device_cmd&c,unsigned tag=1){
 memset(&d,0,sizeof(d));memset(&c,0,sizeof(c));d.num_tbs=nt;d.tbs[0].tb_len=20;d.tbs[0].addr=v.r.data[slot].cmd_paddr;
 d.tbs[1].tb_len=commandBytes-20;d.tbs[1].addr=d.tbs[0].addr+20;
 for(unsigned i=2;i<nt;++i){d.tbs[i].tb_len=1200;d.tbs[i].addr=0x345600000ULL+i*4096;}
 auto*p=(uint8_t*)&c;for(unsigned i=0;i<commandBytes;++i)p[i]=(i+tag)&255;
}
'''
transfer_decl='''int ItlIwx::transfer(iwx_softc*sc,iwx_tx_ring*ring,int idx,const iwx_tfh_tfd*desc,const iwx_device_cmd*cmd,Mbuf*m,Node*in,unsigned type,int totlen,unsigned num_tbs){auto*data=&ring->data[idx];\n'''+transfer+'\nreturn 0;}\n'
test=r'''
int main(){unsigned cases=0;uint64_t typicalCopies=0;
 for(unsigned count:{16U,32U,128U,256U,1024U}){
  Ring v(count);for(unsigned slot:{0U,count/2,count-1})for(unsigned nt=2;nt<=25;++nt)for(unsigned cb:{24U,36U,44U,52U,60U,68U,320U,324U}){
   iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,slot,nt,cb,d,c);auto beforeD=v.d,beforeC=v.c;
   assert(iwx_tx_publish_control(&v.r,slot,&d,&c));unsigned fetched=(2+nt*10+63)&~63;
   assert(!memcmp(&v.r.desc[slot],&d,fetched));assert(!memcmp(&v.r.cmd[slot],&c,cb));
   // Every byte outside this transaction remains unchanged, including guard bytes.
   assert(!memcmp(v.d.data(),beforeD.data(),slot*256));
   assert(!memcmp(v.d.data()+slot*256+fetched,beforeD.data()+slot*256+fetched,v.d.size()-slot*256-fetched));
   assert(!memcmp(v.c.data(),beforeC.data(),slot*324));
   assert(!memcmp(v.c.data()+slot*324+cb,beforeC.data()+slot*324+cb,v.c.size()-slot*324-cb));
   ItlIwx driver;iwx_softc sc;
   for(int family:{1,2}){sc.sc_device_family=family;driver.iwx_tx_update_byte_tbl(&sc,&v.r,slot<256?slot:255,1200,nt);
    auto word=((uint16_t*)v.bc.data())[slot<256?slot:255];unsigned chunks=family==2?word>>14:word>>12;
    assert((chunks+1)*64==fetched);}
   // Model device-written command scratch: it has no alias with CPU staging.
   memset(&v.r.cmd[slot],0x7b,20);assert(((uint8_t*)&c)[0]==1);
   auto beforeRetire=v.d;sc.sc_device_family=2;iwx_clear_tx_desc(&sc,&v.r,slot);
   assert(!v.r.desc[slot].num_tbs);assert(!memcmp((char*)&v.r.desc[slot]+2,(char*)&d+2,fetched-2));
   // A later shorter descriptor overwrites the complete NEW fetch prefix.
   prepare(v,slot,3,60,d,c,2);assert(iwx_tx_publish_control(&v.r,slot,&d,&c));assert(!memcmp(&v.r.desc[slot],&d,64));
   if(nt==3&&cb==60)typicalCopies=64+60+2;
   ++cases;
  }
 }
 // Invalid internal views are rejected before ANY write into device backing.
 for(unsigned fault=0;fault<22;++fault){Ring v(16);iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,3,60,d,c);
  unsigned slot=0;auto*dp=&d;auto*cp=&c;
  switch(fault){case 0:d.num_tbs=1;break;case 1:d.num_tbs=26;break;case 2:d.tbs[0].tb_len=19;break;case 3:d.tbs[1].tb_len=305;break;
   case 4:d.tbs[1].tb_len=1;break;case 5:slot=16;break;case 6:v.r.qid=0;break;case 7:v.r.desc_dma.size=63;break;
   case 8:v.r.cmd_dma.size=59;break;case 9:++v.r.data[0].cmd_paddr;break;case 10:++d.tbs[0].addr;break;case 11:++d.tbs[1].addr;break;
   case 12:dp=&v.r.desc[0];break;case 13:cp=&v.r.cmd[0];break;case 14:v.r.qid=-1;break;case 15:v.r.cmd=nullptr;break;
   case 16:slot=1;v.r.cmd_dma.paddr=UINT64_MAX-319;break;
   case 17:slot=0;v.r.cmd_dma.paddr=UINT64_MAX-63;break;
   case 18:slot=11;v.r.cmd_dma.paddr=0xfffff200ULL;break; //TB0 endsat4GiB
   case 19:slot=12;v.r.cmd_dma.paddr=0xfffff0c0ULL;break; //TB0 crosses4GiB
   case 20:slot=1;v.r.cmd_dma.paddr=0xfffffe80ULL;break; //TB1 endsat4GiB
   case 21:slot=2;v.r.cmd_dma.paddr=0xfffffd40ULL;break; //TB1 crosses4GiB
  }
  if(fault>=16){for(unsigned i=0;i<v.r.ring_count;++i)v.r.data[i].cmd_paddr=v.r.cmd_dma.paddr+uint64_t(i)*324;prepare(v,slot,3,fault==17?68:60,d,c);}

  auto bd=v.d,bc=v.c;assert(!iwx_tx_publish_control(&v.r,slot,dp,cp));assert(v.d==bd&&v.c==bc);++cases;
 }

 // Every supported physical slot, including1024-entry default packet rings.
 for(unsigned count:{16U,32U,128U,256U,1024U}){Ring v(count);for(unsigned slot=0;slot<count;++slot){iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,slot,3,60,d,c,slot);assert(iwx_tx_publish_control(&v.r,slot,&d,&c));assert(!memcmp(&v.r.cmd[slot],&c,60)&&!memcmp(&v.r.desc[slot],&d,64));++cases;}}
 // Ending fourbytesBEFORE4GiB, and beginningexactlyat4GiB, are both legal.
 for(uint64_t base:{0xffffffc0ULL,0x100000000ULL}){Ring v(16);v.r.cmd_dma.paddr=base;for(unsigned i=0;i<16;++i)v.r.data[i].cmd_paddr=base+i*324;iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,3,60,d,c);assert(iwx_tx_publish_control(&v.r,0,&d,&c));++cases;}
 // Actual caller's post-map publication/ownership-transfer block, both modes.
 for(bool mapped:{false,true})for(bool valid:{false,true}){Ring v(16);iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,3,60,d,c);
  ItlIwx driver;driver.mapped=mapped;Mbuf m;Node node;iwx_softc sc;v.r.data[0].token=mapped?1:0;if(!valid)d.num_tbs=26;
  int result=driver.transfer(&sc,&v.r,0,&d,&c,&m,&node,8,1200,3);
  if(valid){assert(!result&&!m.freed&&v.r.data[0].m==&m&&v.r.data[0].in==&node&&!driver.fTx2IBlocked);}
  else{assert(result==EINVAL&&m.freed&&!v.r.data[0].m&&!v.r.data[0].in&&driver.fTx2IBlocked);assert(v.r.data[0].quarantined==mapped);--node.references;assert(!node.references);}
  ++cases;
 }
 // Firmware command descriptor handling stays unchanged, preserving TB0.
 {Ring v(16);iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,0,4,60,d,c);v.r.qid=0;memcpy(v.r.desc,&d,256);iwx_softc sc;iwx_clear_tx_desc(&sc,&v.r,0);
  assert(!v.r.desc[0].num_tbs&&!memcmp(&v.r.desc[0].tbs[0],&d.tbs[0],10));for(int i=1;i<4;++i){iwx_tfh_tb zero{};assert(!memcmp(&v.r.desc[0].tbs[i],&zero,10));}++cases;}
 // Ring wrap and positive-completion handoff. State 0/1 models admission and
 // hardware completion, not elapsed time. Producer cannot reuse an occupied slot.
 {Ring v(256);std::atomic<unsigned> state[256]{};constexpr unsigned loops=20000;
  std::thread consumer([&]{iwx_softc sc;for(unsigned n=0;n<loops;++n){unsigned firmware=(65520+n)&65535,i=firmware&255;while(state[i].load(std::memory_order_acquire)!=1)std::this_thread::yield();
   assert(v.r.desc[i].num_tbs==3);assert(((uint8_t*)&v.r.cmd[i])[0]==(n&255));iwx_clear_tx_desc(&sc,&v.r,i);state[i].store(0,std::memory_order_release);}});
  for(unsigned n=0;n<loops;++n){unsigned firmware=(65520+n)&65535,i=firmware&255;while(state[i].load(std::memory_order_acquire)!=0)std::this_thread::yield();
   iwx_tfh_tfd d;iwx_device_cmd c;prepare(v,i,3,60,d,c,n);assert(iwx_tx_publish_control(&v.r,i,&d,&c));std::atomic_thread_fence(std::memory_order_seq_cst);state[i].store(1,std::memory_order_release);}
  consumer.join();++cases;
 }
 assert(typicalCopies==126);
 std::cout<<"PASS "<<cases<<" publication cases plus 20000 concurrent ring wraps: exact device fetch bytes/guards, byte-count fetch agreement, DMA command writeback versus private CPU staging, packet retirement, malformed/UINT64wrap/controlTB4GiB rejection, all1024physicalslots,65536firmware-coordinate wrap and actual pre-ownership failure branch; typical logical control publication126 bytes including BC, not measured hardware transactions. Hardware/PAT/throughput not modeled.\n";
}
'''
print('SOURCE_CPP_SHA256='+hashlib.sha256(s.encode()).hexdigest(),flush=True)
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc26-publication-') as td:
 p=Path(td);types='\n'.join(typ(n) for n in ['iwx_tfh_tb','iwx_tfh_tfd','iwx_device_cmd','iwx_agn_scd_bc_tbl','iwx_gen3_bc_tbl'])
 (p/'test.cpp').write_text(pre+types+mid+fn('iwx_tx_publish_control')+fn('iwx_clear_tx_desc')+fn('iwx_tx_update_byte_tbl')+transfer_decl+test)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print(cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120)
