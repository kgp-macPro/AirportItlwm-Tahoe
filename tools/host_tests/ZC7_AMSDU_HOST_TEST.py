#!/usr/bin/env python3
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[2]
src=(root/'itl80211/openbsd/net80211/ieee80211_input.c').read_text()
a=src.index('void\nieee80211_amsdu_decap(');b=src.index('\n/*',a+6);body=src[a:b]
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <vector>
#include <arpa/inet.h>
using u_int8_t=uint8_t;
constexpr int ETHER_HDR_LEN=14,LLC_SNAPFRAMELEN=8,MBUF_DONTWAIT=0,LLC_SNAP_LSAP=0xaa,LLC_UI=3;
int live=0,splitCalls=0,pullCalls=0,delivered=0;
bool reject=false,failSplit=false,failPull=false;
struct M{std::vector<uint8_t>b;size_t first;M(std::vector<uint8_t>x,size_t n):b(x),first(n){++live;}~M(){--live;}};
using mbuf_t=M*;
struct ether_header{uint8_t d[6],s[6];uint16_t ether_type;} __attribute__((packed));
struct llc{uint8_t llc_dsap,llc_ssap,llc_control;struct{uint8_t org_code[3];uint16_t ether_type;}__attribute__((packed)) llc_snap;}__attribute__((packed));
struct ieee80211_frame{uint8_t i_addr1[6];};
struct ieee80211com{struct{int is_rx_decap=0;}ic_stats;};struct ieee80211_node{};
struct mbuf_list{std::deque<M*>q;};
#define MBUF_LIST_INITIALIZER() {}
#define mtod(m,T) reinterpret_cast<T>((m)->b.data())
#define IEEE80211_IS_MULTICAST(a) ((a)[0]&1)
#define DPRINTF(x) ((void)0)
size_t mbuf_len(M*m){return m->first;}
size_t mbuf_pkthdr_len(M*m){return m->b.size();}
void mbuf_freem(M*m){delete m;}
void mbuf_adj(M*m,size_t n){n=std::min(n,m->b.size());m->b.erase(m->b.begin(),m->b.begin()+n);m->first=m->first>n?m->first-n:0;}
int mbuf_pullup(M**m,size_t n){++pullCalls;if(failPull){delete *m;return 1;}(*m)->first=n;return 0;}
int mbuf_split(M*m,size_t n,int,M**out){++splitCalls;if(failSplit)return 1;
 assert(n<=m->b.size());std::vector<uint8_t>v(m->b.begin()+n,m->b.end());*out=new M(v,v.size());m->b.resize(n);m->first=std::min(m->first,n);return 0;}
void ml_purge(mbuf_list*l){for(auto m:l->q)delete m;l->q.clear();}
void ml_enqueue(mbuf_list*l,M*m){l->q.push_back(m);}
M*ml_dequeue(mbuf_list*l){if(l->q.empty())return nullptr;auto m=l->q.front();l->q.pop_front();return m;}
int ieee80211_amsdu_decap_validate(ieee80211com*,M*m,ieee80211_node*){assert(m->first>=14);return reject;}
void ieee80211_enqueue_data(ieee80211com*,M*m,ieee80211_node*,int,mbuf_list*l){++delivered;ml_enqueue(l,m);}
M*packet(bool contiguous){std::vector<uint8_t>b(6,0);
 for(int i=0;i<3;++i){size_t p=b.size();b.resize(p+32,0);auto*e=reinterpret_cast<ether_header*>(b.data()+p);e->ether_type=htons(18);}
 return new M(b,contiguous?b.size():6);}
'''
test=r'''
void run(bool contiguous,int failure){
 assert(!live);splitCalls=pullCalls=delivered=0;reject=failure==1;failSplit=failure==2;failPull=failure==3;
 ieee80211com ic;ieee80211_node ni;mbuf_list out;auto m=packet(contiguous);
 ieee80211_amsdu_decap(&ic,m,&ni,6,&out);
 if(failure==0){assert(delivered==3);assert(splitCalls==3);assert(pullCalls==(contiguous?0:1));}
 if(failure==1){assert(!delivered&&!splitCalls&&ic.ic_stats.is_rx_decap==1);}
 if(failure==2){assert(!delivered&&splitCalls==1&&ic.ic_stats.is_rx_decap==1);}
 if(failure==3){assert(!delivered&&pullCalls==1&&!splitCalls);}
 ml_purge(&out);assert(live==0);
}
int main(){run(true,0);run(false,0);run(true,1);run(false,1);run(true,2);run(false,3);
 std::cout<<"PASS extracted A-MSDU: contiguous fast path, fragmented header pullup, invalid subframe rejected before split, split failure preserves then frees original, pullup failure consumes chain, all allocations balanced\n";}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc7-amsdu-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+body+test)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120);print('PASS',san)
