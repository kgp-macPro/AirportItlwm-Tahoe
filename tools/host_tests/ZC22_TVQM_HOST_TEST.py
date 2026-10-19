#!/usr/bin/env python3
"""Execute three actual TVQM/BA functions with injected boundary outcomes.
Mock IOKit, firmware and the existing terminal cleanup contract; not DMA proof.
"""
from pathlib import Path
import re,subprocess,tempfile,sys,os
R=Path(__file__).resolve().parents[2]
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def fn(name):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 q=re.search(r'(?:int|void)\s+ItlIwx::\s*'+name+r'\s*\(',m);assert q,name
 a=m.index('{',q.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
mock=r'''
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
using bus_addr_t=uint64_t;
#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)
#define le16toh(x) (x)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define XYLog(...) ((void)0)
#define DEVNAME(x) "mock"
#define container_of(p,type,member) ((type*)((char*)(p)-offsetof(type,member)))
#define IWX_DEVICE_FAMILY_AX210 2
#define IWX_DEFAULT_QUEUE_SIZE 256
#define IWX_MAX_TID_COUNT 8
#define IWX_DQA_CMD_QUEUE 0
#define IWX_DQA_AUX_QUEUE 1
#define IWX_QID_MGMT 2
#define IWX_INVALID_QUEUE -1
#define IWX_FLAG_SHUTDOWN 1
#define IWX_TX_QUEUE_CFG_ENABLE_QUEUE 1
#define IWX_STATION_ID 0
#define IWX_SCD_QUEUE_CFG 0x1d
#define IWX_CMD_WANT_RESP 2
#define IWX_FIRST_TB_SIZE_ALIGN 64
#define IWX_TFH_NUM_TBS 25
#define MCLBYTES 2048
#define BUS_DMA_NOWAIT 0
#define IWX_CMD_FAILED_MSK 0x40
#define IWX_TFD_QUEUE_CB_SIZE(x) 5
#define IWX_AGG_SSN_TO_TXQ_IDX(x,n) ((x)&((n)-1))
#define IEEE80211_STATUS_UNSPECIFIED 1
static int splnet(){return 0;}static void splx(int){}
static thread_local int allocAt=0,failAt=0,live=0,responses=0;
static void* backing(size_t n){if(++allocAt==failAt)return nullptr;void*p=calloc(1,n);assert(p);++live;return p;}
static void release(void*p){if(p){free(p);--live;}}
struct iwx_dma_info{void*cmd=nullptr;void*vaddr=nullptr;uint64_t paddr=0;};
struct iwx_tfh_tfd{char x[256];};struct iwx_device_cmd{char x[324];};
struct iwx_gen3_bc_tbl{char x[2048];};struct iwx_agn_scd_bc_tbl{char x[640];};
struct iwx_tx_data{uint64_t cmd_paddr;unsigned tx2I_token;void*map;void*m;};
struct iwx_tx_ring{iwx_dma_info desc_dma,cmd_dma,bc_tbl;iwx_tfh_tfd*desc;iwx_device_cmd*cmd;int ring_count,cur,tail,qid,queued;iwx_tx_data data[1024];};
struct iwx_tx_queue_cfg_cmd{uint16_t flags;uint8_t sta_id,tid;uint32_t cb_size;uint64_t byte_cnt_addr,tfdq_addr;};
struct iwx_tx_queue_cfg_rsp{uint16_t queue_number,flags,write_pointer,reserved;};
struct iwx_rx_packet{struct{unsigned group_id;}hdr;iwx_tx_queue_cfg_rsp data[1];};
struct iwx_host_cmd{int id,flags;size_t resp_pkt_len;const void*data[1];size_t len[1];iwx_rx_packet*resp_pkt=nullptr;};
struct ieee80211_tx_ba{int ba_winstart=0,ba_winend=0,ba_winsize=64,ba_timeout_val=100;};
struct ieee80211_rx_ba{int ba_winstart=0,ba_winsize=64,ba_timeout_val=100;};
struct ieee80211_node{ieee80211_tx_ba ni_tx_ba[8];ieee80211_rx_ba ni_rx_ba[8];};
struct ieee80211com{ieee80211_node*ic_bss=nullptr;int accepted[8]={},refused[8]={};};
static void ieee80211_addba_resp_accept(ieee80211com*c,ieee80211_node*,int t){++c->accepted[t];}
static void ieee80211_addba_resp_refuse(ieee80211com*c,ieee80211_node*,int t,int){++c->refused[t];}
struct iwx_softc{int sc_device_family=2,sc_dmat=0,sc_generation=1,sc_flags=0,first_data_qid=1,qfullmsk=0;
 ieee80211com sc_ic;struct{unsigned start_tidmask=0,stop_tidmask=0;}ba_rx,ba_tx;
 iwx_tx_ring sc_tvqm_ring{};iwx_tx_ring txq[512]{};struct{int qid=-1;}sc_tid_data[9];};
static int bus_dmamap_create(int,int,int,int,int,int,void**p){*p=backing(8);return *p?0:ENOMEM;}
struct ItlIwx{
 iwx_softc com;ieee80211_node node;
 unsigned fTx2JAdmission=0,fTx2IBlocked=0,fCmd2KEpoch=1;static constexpr unsigned kTx2JClosed=0x10000;
 int result=0,qid=2,wr=0,replyKind=0,sendCount=0,freeCount=0,resetCount=0,nicCalls=0,unlocks=0;
 bool mapped=true;unsigned retired=0;bool occupied=false,advanceGeneration=false,advanceEpoch=false,shutdown=false,failNicFirst=false,autoQid=false,terminal=false;
 ItlIwx(){com.sc_ic.ic_bss=&node;allocAt=failAt=0;assert(live==0&&responses==0);}
 int iwx_tvqm_enable_txq(iwx_softc*,int,int,uint32_t);int iwx_tvqm_alloc_txq(iwx_softc*,int,int);static void iwx_ba_task(void*);
 bool dma5Mapped(){return mapped;}
 void tx2IRetire(iwx_tx_data*,bool normal){assert(normal);++retired;}
 void iwx_txd_done(iwx_softc*,iwx_tx_data*d){d->m=nullptr;}
 void iwx_ampdu_txq_advance(iwx_softc*,iwx_tx_ring*,int,bool=true);
 void dexterOriginal_iwx_ampdu_txq_advance(iwx_softc*,iwx_tx_ring*,int);
 void iwx_sta_rx_agg(iwx_softc*,ieee80211_node*,int,int,int,int,int){}
 bool iwx_nic_lock(iwx_softc*){return !(failNicFirst&&++nicCalls==1);}
 void iwx_nic_unlock(iwx_softc*){++unlocks;}
 unsigned getTxQueueSize(){return com.sc_device_family>=2?65536:256;}
 void iwx_tx_ring_init(iwx_softc*,iwx_tx_ring*r,int n){r->ring_count=n;r->qid=-1;r->cur=r->tail=0;}
 int iwx_tx_control_alloc(iwx_dma_info*d,size_t n,unsigned){d->vaddr=backing(n);d->cmd=d->vaddr;d->paddr=0x4000;return d->cmd?0:ENOMEM;}
 static void iwx_dma_publish_barrier(){__atomic_thread_fence(__ATOMIC_SEQ_CST);}
 int iwx_send_cmd(iwx_softc*s,iwx_host_cmd*h){++sendCount;h->resp_pkt=nullptr;
  if(advanceGeneration)++s->sc_generation;if(advanceEpoch)++fCmd2KEpoch;if(shutdown)s->sc_flags|=1;
  if(result)return result;if(replyKind==1)return 0;
  h->resp_pkt=(iwx_rx_packet*)calloc(1,sizeof(iwx_rx_packet));assert(h->resp_pkt);++responses;
  h->resp_pkt->hdr.group_id=replyKind==2?IWX_CMD_FAILED_MSK:0;
  h->resp_pkt->data[0]={(uint16_t)(autoQid?qid++:qid),0,(uint16_t)wr,0};return 0;}
 size_t iwx_rx_packet_payload_len(iwx_rx_packet*){return replyKind==3?1:sizeof(iwx_tx_queue_cfg_rsp);}
 bool tx5RingReleasable(iwx_tx_ring*){return !occupied;}
 void iwx_reset_tx_ring(iwx_softc*,iwx_tx_ring*r){++resetCount;r->cur=r->tail=r->queued=0;}
 void iwx_free_tx_ring(iwx_softc*,iwx_tx_ring*r){++freeCount;
  for(auto*d:{&r->desc_dma,&r->cmd_dma,&r->bc_tbl}){release(d->cmd);*d={};}
  for(auto&d:r->data){release(d.map);d.map=nullptr;}r->ring_count=0;}
 void iwx_free_resp(iwx_softc*,iwx_host_cmd*h){if(h->resp_pkt){free(h->resp_pkt);--responses;h->resp_pkt=nullptr;}}
 bool terminalCleanup(){if(!terminal)return false;iwx_free_tx_ring(&com,&com.sc_tvqm_ring);return true;}
 ~ItlIwx(){terminal=true;terminalCleanup();for(auto&r:com.txq)iwx_free_tx_ring(&com,&r);assert(live==0&&responses==0);}
};
'''
mock+='static void iwx_clear_tx_desc(iwx_softc*,iwx_tx_ring*r,int idx){assert(unsigned(idx)<unsigned(r->ring_count));}\n'
test=r'''
int main(){unsigned scenarios=0;
 for(int family:{1,2}){
  // AX201-family conventional and AX210/211 coordinate domains; mapping itself is mocked.
  for(int error:{EBUSY,ENOMEM,EIO,ENXIO,ETIMEDOUT,ENOSPC,-EIO}){
   auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;d->result=error;
   int r=d->iwx_tvqm_alloc_txq(&d->com,0,0);assert(r==(error>0?-error:error));
   assert(d->sendCount==1&&d->com.sc_tid_data[0].qid==-1&&d->fTx2IBlocked);
   int n=live;assert(n>0&&!d->terminalCleanup()&&live==n);assert(d->freeCount==0&&responses==0);
   assert(d->iwx_tvqm_alloc_txq(&d->com,1,0)==-EBUSY&&d->sendCount==1&&live==n);
   d->terminal=true;assert(d->terminalCleanup()&&live==0);++scenarios;
  }
  for(int fault:{1,2,3,4,259}){
   auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;failAt=fault;
   assert(d->iwx_tvqm_enable_txq(&d->com,0,0,256)<0);assert(!live&&!responses&&!d->sendCount&&!d->fTx2IBlocked);++scenarios;
  }
  {auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;failAt=1;
   assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==2&&d->com.txq[2].ring_count==128&&d->sendCount==1);++scenarios;}
  for(int reply:{1,2,3}){auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;d->replyKind=reply;
   assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==-EIO&&d->sendCount==1&&!responses&&live>0&&d->freeCount==0);++scenarios;}
  for(int q:{0,1,32,511,512,65535}){auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;d->qid=q;
   assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)<0&&d->com.sc_tid_data[0].qid==-1&&!responses);++scenarios;}
  for(int wr:{0,17,255}){auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;d->wr=wr;
   assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==2&&d->com.txq[2].cur==wr&&d->com.txq[2].tail==wr&&!responses);
   assert(!d->com.sc_tvqm_ring.desc_dma.cmd&&!d->fTx2IBlocked);++scenarios;}
  for(int change=0;change<3;++change){auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=family;
   d->advanceGeneration=change==0;d->advanceEpoch=change==1;d->shutdown=change==2;
   assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==-ENXIO&&d->com.sc_tid_data[0].qid==-1&&!responses);++scenarios;}
 }
 {auto d=std::make_unique<ItlIwx>();d->wr=65535;assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==2&&d->com.txq[2].tail==255);++scenarios;}
 {auto d=std::make_unique<ItlIwx>();d->com.sc_device_family=1;d->wr=256;assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)<0);++scenarios;}
 for(int q:{1,2,3}){auto d=std::make_unique<ItlIwx>();d->com.first_data_qid=2;d->qid=q;
  int r=d->iwx_tvqm_alloc_txq(&d->com,0,0);assert(q==3?r==3:r<0);++scenarios;}
 {auto d=std::make_unique<ItlIwx>();d->com.sc_tid_data[1].qid=2;assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==-EBUSY&&d->com.sc_tid_data[1].qid==2&&d->freeCount==0);++scenarios;}
 {auto d=std::make_unique<ItlIwx>();d->occupied=true;assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==-EBUSY&&d->freeCount==0);++scenarios;}
 {auto d=std::make_unique<ItlIwx>();assert(d->iwx_tvqm_alloc_txq(&d->com,-1,0)==-EINVAL);assert(d->iwx_tvqm_alloc_txq(&d->com,8,0)==-EINVAL);assert(!live&&!d->sendCount);++scenarios;}
 {auto d=std::make_unique<ItlIwx>();d->failNicFirst=true;d->com.ba_tx.start_tidmask=3;
  ItlIwx::iwx_ba_task(&d->com);assert(d->com.sc_ic.refused[0]==1&&d->com.sc_ic.accepted[1]==1&&!d->com.sc_ic.refused[1]&&d->unlocks==1&&!d->com.ba_tx.start_tidmask);++scenarios;}
 {auto d=std::make_unique<ItlIwx>();d->result=EBUSY;d->com.ba_tx.start_tidmask=1;
  ItlIwx::iwx_ba_task(&d->com);assert(d->com.sc_ic.refused[0]==1&&!d->com.sc_ic.accepted[0]&&d->com.sc_tid_data[0].qid==-1);++scenarios;}
 // TSan exercises production atomic failure-publication while a diagnostic
 // reader observes only those atomic flags. No concurrent ring reclamation is
 // simulated as safe: real DMA/teardown still depends on existing stop proofs.
 {auto d=std::make_unique<ItlIwx>();std::atomic<bool> done{false};
  std::thread reader([&]{while(!done.load()) {auto b=__atomic_load_n(&d->fTx2IBlocked,__ATOMIC_ACQUIRE);
   if(b)assert(__atomic_load_n(&d->fTx2JAdmission,__ATOMIC_ACQUIRE)&ItlIwx::kTx2JClosed);}});
  d->result=ETIMEDOUT;assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==-ETIMEDOUT);done=true;reader.join();++scenarios;}
 for(bool mapped:{false,true}){auto d=std::make_unique<ItlIwx>();d->mapped=mapped;d->wr=65535;
  assert(d->iwx_tvqm_alloc_txq(&d->com,0,0)==2);auto*r=&d->com.txq[2];assert(r->cur==65535&&r->tail==255);
  d->iwx_ampdu_txq_advance(&d->com,r,255,true);assert(r->tail==255);
  r->data[255].m=(void*)1;r->queued=1;d->iwx_ampdu_txq_advance(&d->com,r,0,true);assert(r->tail==0&&r->queued==0);
  r->data[0].m=(void*)1;r->queued=1;d->iwx_ampdu_txq_advance(&d->com,r,1,true);assert(r->tail==1&&r->queued==0);++scenarios;}
 assert(!live&&!responses);std::cout<<"PASS "<<scenarios<<" actual TVQM/BA scenarios: negative errno, bounded no-retry retention, response release, local allocation cleanup, valid/reserved/live QIDs, firmware coordinate domains, stale epochs, BA refusal/next-TID recovery, atomic fault publication. Hardware DMA/throughput not proved.\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc21-tvqm-') as td:
 p=Path(td);(p/'test.cpp').write_text(mock+fn('iwx_tvqm_enable_txq')+fn('iwx_tvqm_alloc_txq')+fn('iwx_ba_task')+fn('iwx_ampdu_txq_advance')+fn('dexterOriginal_iwx_ampdu_txq_advance')+test)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print(cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
