#!/usr/bin/env python3
"""Compile BOTH real ieee80211_encap bodies; mock only mbuf/KPI environment.
Reference expectations: Linux v6.9 TLC offload/always-QoS and protected-HT
policy of this net80211 implementation. No fake host BA or physical model.
"""
from pathlib import Path
import re,subprocess,sys,tempfile
R=Path(__file__).resolve().parents[2]
G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
C=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip()
SDK=subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
P='itl80211/openbsd/net80211/ieee80211_output.c'
def fn(s,name):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:mbuf_t|int|u_int16_t)\s+'+name+r'\s*\([^;{}]*\)\s*\{',m);assert x,name;a=m.index('{',x.start());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
src=(R/P).read_text();base=subprocess.check_output([G,'show','028b4626d113efa283ed44f9c4feb92ba6735aab:'+P],cwd=R,text=True)
new=fn(src,'ieee80211_encap');old=fn(base,'ieee80211_encap').replace('ieee80211_encap(','old_encap(',1)
can=fn(src,'ieee80211_can_use_ampdu');assert can==fn(base,'ieee80211_can_use_ampdu')
mac=(R/'itl80211/openbsd/net80211/ieee80211.h').read_text()
accessors='\n'.join(fn(mac,n)for n in ['ieee80211_has_addr4','ieee80211_has_qos','ieee80211_get_qos'])
cpp=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();begin=cpp.index('    tid = IWX_MGMT_TID;',cpp.index('iwx_tx(struct'))
end=cpp.index('    ring = &sc->txq[qid];',begin);route=cpp[begin:end]
wire=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text()+(R/'itlwm/hal_iwx/if_iwxvar.h').read_text()
iwx_constants=''
for name in sorted(set(re.findall(r'\bIWX_[A-Z0-9_]+',route))):
 x=re.search(r'^#define\s+'+name+r'\s+([^\n]+)',wire,re.M);assert x,name;iwx_constants+='#define '+name+' '+x[1]+'\n'

headers='\n'.join(p.read_text()for p in (R/'itl80211/openbsd/net80211').glob('*.h'))
constants=re.search(r'^#define\s+IEEE80211_IS_MULTICAST\([^\n]+',headers,re.M)[0]+'\n';pending=(set(re.findall(r'\bIEEE80211_[A-Z0-9_]+',new+old+can+accessors+route))|{'IEEE80211_C_TX_AMPDU_SETUP_IN_HW','IEEE80211_FC0_SUBTYPE_MASK'})-{'IEEE80211_STA_ONLY','IEEE80211_ADDR_COPY','IEEE80211_SEND_ACTION','IEEE80211_IS_MULTICAST'}
enum={'IEEE80211_M_IBSS':0,'IEEE80211_M_AHDEMO':3,'IEEE80211_M_STA':1,'IEEE80211_M_HOSTAP':6,'IEEE80211_STA_ASSOC':2,'IEEE80211_S_RUN':4,'IEEE80211_BA_INIT':0,'IEEE80211_BA_REQUESTED':1,'IEEE80211_BA_AGREED':2}
done=set()
while pending:
 n=pending.pop()
 if n in done:continue
 done.add(n)
 if n in enum:constants+=f'#define {n} {enum[n]}\n';continue
 x=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',headers,re.M) or re.search(r'\b'+n+r'\s*=\s*([^,\n]+)',headers);assert x,n
 value=re.sub(r'/\*.*','',x[1]).strip();constants+=f'#define {n} {value}\n';pending|=set(re.findall(r'\bIEEE80211_[A-Z0-9_]+',value))-done
pre=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <array>
#include <sys/types.h>
#include <arpa/inet.h>
#define IEEE80211_STA_ONLY 1
#define DLT_IEEE802_11 105
#define DLT_IEEE802_11_RADIO 127
#define LLC_SNAP_LSAP 0xaa
#define LLC_UI 3
#define LLC_SNAPFRAMELEN 8
#define ETHERTYPE_PAE 0x888e
#define ETHERTYPE_IP 0x0800
#define MBUF_DONTWAIT 0
#define DPRINTF(...) ((void)0)
#define XYLog(...) ((void)0)
#define htole16(x) (x)
#define letoh16(x) (x)
#define nitems(x) (sizeof(x)/sizeof((x)[0]))
#define IEEE80211_ADDR_COPY(a,b) memcpy((a),(b),6)
using mbuf_tag_id_t=uintptr_t;
struct Mbuf {std::array<uint8_t,512>b{};unsigned off=128,len=100;bool live=true;};using mbuf_t=Mbuf*;
#define mtod(m,t) ((t)((m)->b.data()+(m)->off))
unsigned mbuf_len(mbuf_t m){assert(m&&m->live);return m->len;}
void mbuf_freem(mbuf_t m){if(m){assert(m->live);m->live=false;}}
void mbuf_adj(mbuf_t m,unsigned n){assert(m->live&&m->len>=n);m->off+=n;m->len-=n;}
bool fail_prepend=false;void mbuf_prepend(mbuf_t*m,unsigned n,int){if(fail_prepend){mbuf_freem(*m);*m=nullptr;return;}assert((*m)->off>=n);(*m)->off-=n;(*m)->len+=n;}
void mbuf_pullup(mbuf_t*m,unsigned n){if((*m)->len<n){mbuf_freem(*m);*m=nullptr;}}
struct ether_header {uint8_t ether_dhost[6],ether_shost[6];uint16_t ether_type;}__attribute__((packed));
struct llc {uint8_t llc_dsap,llc_ssap,llc_control;struct {uint8_t org_code[3];uint16_t ether_type;}llc_snap;}__attribute__((packed));
struct ieee80211_frame {uint8_t i_fc[2],i_dur[2],i_addr1[6],i_addr2[6],i_addr3[6],i_seq[2];}__attribute__((packed));
struct ieee80211_qosframe {uint8_t i_fc[2],i_dur[2],i_addr1[6],i_addr2[6],i_addr3[6],i_seq[2],i_qos[2];}__attribute__((packed));
struct ieee80211_qosframe_addr4 {uint8_t i_fc[2],i_dur[2],i_addr1[6],i_addr2[6],i_addr3[6],i_seq[2],i_addr4[6],i_qos[2];}__attribute__((packed));
struct ieee80211_tx_ba {unsigned ba_state=0;};
struct ieee80211_node {unsigned ni_inact=73,ni_state=2,ni_port_valid=1,ni_rsncipher=0,ni_flags=0,ni_rsnprotos=0;uint16_t ni_txseq=4095,ni_qos_txseqs[16]={};uint8_t ni_bssid[6]={2,2,3,4,5,6};ieee80211_tx_ba ni_tx_ba[16];};
struct _ifnet {char if_xname[8]="en17";};
struct ieee80211com { _ifnet ic_if;unsigned ic_opmode=1,ic_flags=0,ic_caps=0,ic_state=4,ic_tid_noack=0;ieee80211_node*ic_bss=nullptr;struct{unsigned is_tx_nonode=0,is_tx_noauth=0,is_tx_nombuf=0;}ic_stats;};
ieee80211_node*node=nullptr;unsigned releases=0,requests=0,priority=0;bool missing_node=false;
ieee80211_node*ieee80211_find_txnode(ieee80211com*,const uint8_t*){return missing_node?nullptr:node;}
ieee80211_node*ieee80211_ref_node(ieee80211_node*n){return n;}
void ieee80211_release_node(ieee80211com*,ieee80211_node*n){assert(n);++releases;}
int ieee80211_classify(ieee80211com*,mbuf_t){return priority;}
void ieee80211_node_trigger_addba_req(ieee80211_node*,int){++requests;}
const char*ether_sprintf(const uint8_t*){return "mock";}
'''
route_pre=r'''
struct iwx_softc {int first_data_qid=1;struct{int qid=255;}sc_tid_data[8];unsigned txq[32];};
struct RouteEnv {bool isBz=true;unsigned fault=0;bool bzDevice(){return isBz;}void bzFail(unsigned r,int){fault=r;}
 int select(iwx_softc*sc,mbuf_t m,ieee80211_node*ni){
  auto*wh=mtod(m,ieee80211_frame*);unsigned type=wh->i_fc[0]&IEEE80211_FC0_TYPE_MASK,subtype=wh->i_fc[0]&IEEE80211_FC0_SUBTYPE_MASK;uint8_t tid=0;int qid=0;
'''+route+'\nreturn qid;}};\n'
test=r'''
Mbuf packet(unsigned type=ETHERTYPE_IP){Mbuf m;ether_header e{};e.ether_type=htons(type);e.ether_dhost[0]=0x10;e.ether_shost[0]=0x20;memcpy(m.b.data()+m.off,&e,sizeof(e));return m;}
void setup(ieee80211com&ic,ieee80211_node&ni,bool offload){ic.ic_flags=IEEE80211_F_QOS|IEEE80211_F_RSNON;ic.ic_caps=IEEE80211_C_TX_AMPDU|IEEE80211_C_TX_AMPDU_SETUP_IN_HW|(offload?IEEE80211_C_TX_BA_OFFLOAD:0);ic.ic_bss=&ni;ni.ni_flags=IEEE80211_NODE_QOS|IEEE80211_NODE_HT;ni.ni_rsnprotos=IEEE80211_PROTO_RSN;node=&ni;requests=releases=0;}
int main(){unsigned cases=0;
 // Every legacy state combination must remain byte-identical, including sequence
 // wrap, EAPOL, unauthenticated drops and BA INIT/REQUESTED/AGREED behavior.
 for(unsigned mask=0;mask<256;++mask)for(unsigned ba=0;ba<3;++ba)for(unsigned tid=0;tid<4;++tid){
  ieee80211com a{},b{};ieee80211_node x{},y{};setup(a,x,false);x.ni_tx_ba[tid].ba_state=ba;
  if(mask&1)a.ic_flags&=~IEEE80211_F_QOS;if(mask&2)x.ni_flags&=~IEEE80211_NODE_QOS;if(mask&4)x.ni_flags&=~IEEE80211_NODE_HT;
  if(mask&8)x.ni_port_valid=0;if(mask&16)a.ic_flags&=~IEEE80211_F_RSNON;if(mask&32)x.ni_rsnprotos=0;
  if(mask&64)a.ic_state=2;priority=tid;y=x;b=a;b.ic_bss=&y;auto m=packet(mask&128?ETHERTYPE_PAE:ETHERTYPE_IP),n=m;
  node=&x;ieee80211_node *rx=nullptr,*ry=nullptr;auto p=old_encap(&a.ic_if,&m,&rx);unsigned req=requests,rel=releases;
  requests=releases=0;node=&y;auto q=ieee80211_encap(&b.ic_if,&n,&ry);
  assert(bool(p)==bool(q)&&m.live==n.live&&m.off==n.off&&m.len==n.len&&m.b==n.b);assert(!memcmp(&x,&y,sizeof(x)));assert(!memcmp(&a.ic_stats,&b.ic_stats,sizeof(a.ic_stats)));assert(requests==req&&releases==rel);++cases;
 }
 // Actual prior failure: BZ callback cannot create host BA, so P7 sends DATA
 // (24-byte header) -> management queue. Correct path emits QoS/TID without
 // claiming BA agreement or scheduling a host ADDBA timer.
 for(unsigned tid=0;tid<4;++tid)for(unsigned ba=0;ba<3;++ba){ieee80211com ic{};ieee80211_node ni{};setup(ic,ni,true);priority=tid;ni.ni_tx_ba[tid].ba_state=ba;ni.ni_qos_txseqs[tid]=4095;auto m=packet();ieee80211_node*out=nullptr;
  assert(ieee80211_encap(&ic.ic_if,&m,&out)==&m&&out==&ni);auto*w=mtod(&m,ieee80211_qosframe*);assert((w->i_fc[0]&IEEE80211_FC0_SUBTYPE_QOS)&&w->i_qos[0]==tid);assert(ni.ni_qos_txseqs[tid]==0&&ni.ni_txseq==4095&&ni.ni_tx_ba[tid].ba_state==ba&&!requests&&!releases);
 iwx_softc sc;for(unsigned t=0;t<8;++t)sc.sc_tid_data[t].qid=t+3;RouteEnv e;assert(e.select(&sc,&m,&ni)==int(tid+3)&&!e.fault);++cases;}
 // No offload bypass of authorization, EAPOL, negotiated QoS, protected-HT
 // policy, station identity or RUN lifetime. Old path remains the fallback.
 for(unsigned mode=0;mode<8;++mode){ieee80211com ic{};ieee80211_node ni{},other{};setup(ic,ni,true);priority=0;
  unsigned type=mode==0?ETHERTYPE_PAE:ETHERTYPE_IP;
  if(mode==1)ni.ni_port_valid=0;if(mode==2)ic.ic_flags&=~IEEE80211_F_QOS;if(mode==3)ni.ni_flags&=~IEEE80211_NODE_QOS;
  if(mode==4)ni.ni_flags&=~IEEE80211_NODE_HT;if(mode==5)ic.ic_flags&=~IEEE80211_F_RSNON;if(mode==6)ic.ic_state=2;if(mode==7)ic.ic_bss=&other;
  auto m=packet(type);ieee80211_node*out=nullptr;auto p=ieee80211_encap(&ic.ic_if,&m,&out);
  if(mode==1)assert(!p&&!m.live);else {assert(p==&m);auto*w=mtod(&m,ieee80211_frame*);assert(!(w->i_fc[0]&IEEE80211_FC0_SUBTYPE_QOS));}++cases;
 }
 {ieee80211com ic{};ieee80211_node ni{};setup(ic,ni,true);missing_node=true;auto m=packet();ieee80211_node*out=nullptr;assert(!ieee80211_encap(&ic.ic_if,&m,&out)&&!m.live&&!out);missing_node=false;++cases;}
 {ieee80211com ic{};ieee80211_node ni{};setup(ic,ni,true);fail_prepend=true;auto m=packet();ieee80211_node*out=nullptr;assert(!ieee80211_encap(&ic.ic_if,&m,&out)&&!m.live&&!out&&releases==1);fail_prepend=false;++cases;}

 for(unsigned tid=8;tid<16;++tid){ieee80211com ic{};ieee80211_node ni{};setup(ic,ni,true);auto m=packet();ieee80211_node*out=nullptr;assert(ieee80211_encap(&ic.ic_if,&m,&out));mtod(&m,ieee80211_qosframe*)->i_qos[0]=tid;iwx_softc sc;RouteEnv e;assert(e.select(&sc,&m,&ni)==EINVAL&&!m.live&&!e.fault);++cases;}
 {ieee80211com ic{};ieee80211_node ni{};setup(ic,ni,true);auto m=packet(ETHERTYPE_PAE);ieee80211_node*out=nullptr;assert(ieee80211_encap(&ic.ic_if,&m,&out));iwx_softc sc;sc.sc_tid_data[0].qid=3;RouteEnv e;assert(e.select(&sc,&m,&ni)==sc.first_data_qid);++cases;}
 // Non-BA legacy data remains on management; agreed legacy data uses its TID.
 for(unsigned ba=0;ba<3;++ba){ieee80211com ic{};ieee80211_node ni{};setup(ic,ni,false);ni.ni_tx_ba[0].ba_state=ba;priority=0;auto m=packet();ieee80211_node*out=nullptr;assert(ieee80211_encap(&ic.ic_if,&m,&out));iwx_softc sc;sc.sc_tid_data[0].qid=3;RouteEnv e;e.isBz=false;assert(e.select(&sc,&m,&ni)==(ba==IEEE80211_BA_AGREED?3:1));++cases;}
 std::cout<<"PASS "<<cases<<" actual encapsulation legacy-differential/offload/auth/sequence/error cases; no firmware or throughput proof.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='be200-qos-')as td:
 p=Path(td);(p/'test.cpp').write_text(pre+constants+iwx_constants+accessors+can+old+new+route_pre+test)
 cmd=[C,'-isysroot',SDK,'-std=gnu++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
