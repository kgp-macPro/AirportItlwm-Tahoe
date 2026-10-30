#!/usr/bin/env python3
"""Extract actual status/free/PDU helpers; allocator/transport are host stubs.
A returned nonnull response is CPU-owned. No firmware DMA or TCP effect modeled.
"""
from pathlib import Path
import os,re,subprocess,tempfile,sys
R=Path(os.environ.get('ZC26_SOURCE_ROOT',Path(__file__).resolve().parents[2]))
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def extract(name):
    masked=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s,flags=re.S)
    q=re.search(r'(?:void|int)\s+ItlIwx::\s*'+re.escape(name)+r'\s*\(',masked);assert q,name
    a=masked.index('{',q.end());b=a+1;d=1
    while d:d+=(masked[b]=='{')-(masked[b]=='}');b+=1
    return s[q.start():b]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include <iostream>
#include <cerrno>
#define KASSERT(c,m) assert(c)
#define le32toh(x) (x)
#define htole32(x) (x)
#define htole16(x) (x)
#define XYLog(...) do {} while(0)
#define IWX_FW_CMD_ID_AND_COLOR(i,c) (((i)<<8)|(c))
constexpr int IWX_FLAG_STA_ACTIVE=0x40,IWX_ADD_STA_SUCCESS=1,IWX_ADD_STA_STATUS_MASK=0xff;
constexpr int IWX_DQA_AUX_QUEUE=1,IWX_AUX_STA_ID=2,IWX_STA_AUX_ACTIVITY=3,IWX_MAC_INDEX_AUX=4,IWX_MGMT_TID=15,IWX_ADD_STA=0x18,IWX_LONG_GROUP=1,IWX_FW_CMD_VER_UNKNOWN=255;
struct iwx_node{};
struct iwx_add_sta_cmd{uint32_t sta_id=0,station_type=0,mac_id_n_color=0;uint16_t tid_disable_tx=0;};
constexpr int IWX_CMD_WANT_RESP=1,IWX_CMD_FAILED_MSK=0x40;
struct iwx_rx_packet {uint32_t len_n_flags;struct{uint8_t cmd,group_id,idx,qid;}hdr;uint8_t data[8];};
struct iwx_cmd_response{uint32_t status;};
struct iwx_host_cmd{uint32_t id=0;uint16_t len[2]={};const void*data[2]={};unsigned flags=0,resp_pkt_len=0;iwx_rx_packet*resp_pkt=nullptr;};
struct iwx_softc{unsigned family=0;bool mapped=false;unsigned sc_flags=0;struct{unsigned ring_count=256;}txq[3];};
static std::unordered_set<void*> live;
static unsigned freeCalls=0,checks=0;
static void tracked_free(void*p){++freeCalls;if(p){assert(live.erase(p)==1);std::free(p);}}
#define free tracked_free
static unsigned iwx_rx_packet_payload_len(const iwx_rx_packet*p){return p->len_n_flags;}
struct ItlIwx{int scenario=0,transportCalls=0,enableCalls=0,enableError=0;uint32_t replyStatus=0x1234abcd;uint8_t commandVersion=11;bool retainedTransportOwnership=false;unsigned freeBefore=0;
int iwx_send_cmd(iwx_softc*,iwx_host_cmd*c){++transportCalls;assert(c->flags&IWX_CMD_WANT_RESP);assert(c->resp_pkt_len==sizeof(iwx_rx_packet)+sizeof(iwx_cmd_response));c->resp_pkt=nullptr;
 if(scenario==3){retainedTransportOwnership=true;return ETIMEDOUT;}
 if(scenario==4)return ENOMEM;
 if(scenario==5)return 0;
 if(scenario==6)return -EIO;
 auto*p=static_cast<iwx_rx_packet*>(std::calloc(1,sizeof(iwx_rx_packet)));assert(p);live.insert(p);c->resp_pkt=p;p->len_n_flags=sizeof(iwx_cmd_response);uint32_t st=replyStatus;memcpy(p->data,&st,4);
 if(scenario==1)p->hdr.group_id=IWX_CMD_FAILED_MSK;
 if(scenario==2)p->len_n_flags=3;
 return 0;
}
uint8_t iwx_lookup_cmd_ver(iwx_softc*,int,int){return commandVersion;}
int iwx_enable_txq(iwx_softc*,int sta,int qid,int tid,unsigned count){++enableCalls;assert(sta==IWX_AUX_STA_ID&&qid==IWX_DQA_AUX_QUEUE&&tid==IWX_MGMT_TID&&count==256);return enableError;}
int iwx_add_sta_status_tail(iwx_softc*);
int iwx_add_aux_sta(iwx_softc*);
int iwx_send_cmd_status(iwx_softc*,iwx_host_cmd*,uint32_t*);
int iwx_send_cmd_pdu_status(iwx_softc*,uint32_t,uint16_t,const void*,uint32_t*);
void iwx_free_resp(iwx_softc*,iwx_host_cmd*);
};
static void ck(bool b){++checks;assert(b);}
'''
test=r'''
int main(){for(unsigned family:{0U,22000U,22500U})for(bool mapped:{false,true})for(int mode=0;mode<7;mode++){
 // Family22000 mapped is NOT production-qualified; this transport-only loop
 // tests absence of family side effects, not support for that DMA route.
 ItlIwx d;d.scenario=mode;iwx_softc sc{family,mapped};iwx_host_cmd c;uint32_t status=0xfeedbeef;unsigned before=freeCalls;
 int e=d.iwx_send_cmd_status(&sc,&c,&status);
 ck(d.transportCalls==1);ck(c.flags&IWX_CMD_WANT_RESP);ck(c.resp_pkt==nullptr);ck(live.empty());
 if(mode==0){ck(!e);ck(status==0x1234abcd);ck(freeCalls==before+1);}else{ck(e==(mode==3?ETIMEDOUT:(mode==4?ENOMEM:(mode==6?-EIO:EIO))));ck(status==0xfeedbeef);if(mode==1||mode==2)ck(freeCalls==before+1);if(mode==3||mode==4||mode==6)ck(freeCalls==before);}
 if(mode==3)ck(d.retainedTransportOwnership); // helper did not recycle transport/DMA
 status=0xfeedbeef;uint32_t cmddata=42;before=freeCalls;int calls=d.transportCalls;
 e=d.iwx_send_cmd_pdu_status(&sc,9,4,&cmddata,&status);ck(d.transportCalls==calls+1);ck(live.empty());
 if(mode==0){ck(!e);ck(status==0x1234abcd);ck(freeCalls==before+1);}else{ck(e==(mode==3?ETIMEDOUT:(mode==4?ENOMEM:(mode==6?-EIO:EIO))));ck(status==0xfeedbeef);}
 }
 {ItlIwx d;iwx_softc sc;iwx_host_cmd c;c.flags=IWX_CMD_WANT_RESP;unsigned n=freeCalls;d.iwx_free_resp(&sc,&c);ck(c.resp_pkt==nullptr);ck(freeCalls==n+1);ck(live.empty());}

 for(int mode=0;mode<7;mode++)for(bool priorActive:{false,true})for(bool statusOK:{false,true}){
  ItlIwx d;d.scenario=mode;d.replyStatus=statusOK?IWX_ADD_STA_SUCCESS:0x80;iwx_softc sc;sc.sc_flags=0x2000|(priorActive?IWX_FLAG_STA_ACTIVE:0);auto before=sc.sc_flags;
  int e=d.iwx_add_sta_status_tail(&sc);
  if(mode==0&&statusOK){ck(!e);ck(sc.sc_flags==(before|IWX_FLAG_STA_ACTIVE));}
  else{ck(e==(mode==3?ETIMEDOUT:(mode==4?ENOMEM:(mode==6?-EIO:EIO))));ck(sc.sc_flags==before);}
  ck(live.empty());ck(d.enableCalls==0);
 }
 for(uint8_t version:{uint8_t(11),uint8_t(12),uint8_t(255)})for(int mode=0;mode<7;mode++)for(bool statusOK:{false,true}){
  ItlIwx d;d.scenario=mode;d.commandVersion=version;d.replyStatus=statusOK?IWX_ADD_STA_SUCCESS:0x80;iwx_softc sc;
  int e=d.iwx_add_aux_sta(&sc);
  if(version==12){ck(!e);ck(!d.transportCalls);ck(!d.enableCalls);}
  else if(mode==0&&statusOK){ck(!e);ck(d.transportCalls==1);ck(d.enableCalls==1);}
  else{ck(e==(mode==3?ETIMEDOUT:(mode==4?ENOMEM:(mode==6?-EIO:EIO))));ck(d.transportCalls==1);ck(d.enableCalls==0);}
  ck(live.empty());
 }
 {ItlIwx d;d.replyStatus=IWX_ADD_STA_SUCCESS;d.enableError=EBUSY;iwx_softc sc;ck(d.iwx_add_aux_sta(&sc)==EBUSY);ck(d.enableCalls==1);ck(live.empty());}
 std::cout<<"PASS actual status/free/PDU, full auxiliary-station and actual station-status-tail checks="<<checks<<"; null/failed/malformed/valid CPU-response ownership, transport errors unchanged, no DMA release. Nonnullfailed normal RX reachability separately constrained by RX filtering.\n";
}
'''

sta=extract('iwx_add_sta_cmd')
anchor='    status = IWX_ADD_STA_SUCCESS;'
a=sta.rindex(anchor)
sta_tail='int ItlIwx::iwx_add_sta_status_tail(struct iwx_softc *sc)\n{\n    int err; uint32_t status; struct iwx_add_sta_cmd add_sta_cmd={};\n'+sta[a:]
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc26-status-') as td:
    p=Path(td);(p/'test.cpp').write_text(mock+'\n'+extract('iwx_free_resp')+'\n'+extract('iwx_send_cmd_status')+'\n'+extract('iwx_send_cmd_pdu_status')+'\n'+sta_tail+'\n'+extract('iwx_add_aux_sta')+'\n'+test)
    c=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
    print('SOURCE',R,flush=True);print('COMMAND',c,flush=True);subprocess.run(c,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
