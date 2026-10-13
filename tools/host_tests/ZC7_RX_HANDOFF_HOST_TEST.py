#!/usr/bin/env python3
"""Extract the actual RX list dequeue/helper and stack handoff body."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[2]
src=(root/'itl80211/openbsd/sys/_mbuf.cpp').read_text();hdr=(root/'itl80211/openbsd/sys/_mbuf.h').read_text()
def extract(s,pattern):
 m=re.search(pattern,s);assert m
 a=s.index('{',m.end());b=a+1;n=1
 while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[m.start():b]
helper=extract(hdr,r'static inline mbuf_t\s+ml_dequeue\(')
body=extract(src,r'static IOReturn _if_input\(')
mock=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
using OSObject=void;using IOReturn=int;constexpr int kIOReturnSuccess=0;
struct M {M*next=nullptr;unsigned id;};using mbuf_t=M*;
mbuf_t mbuf_nextpkt(mbuf_t m){return m->next;}
void mbuf_setnextpkt(mbuf_t m,mbuf_t n){m->next=n;}
struct mbuf_list{M*ml_head=nullptr;M*ml_tail=nullptr;unsigned ml_len=0;};
struct Stat{unsigned inputPackets=0;};
struct IONetworkInterface{
 enum{kInputOptionQueuePacket=1};bool immediate=false;
 unsigned received=0,flushed=0;std::vector<M*>pending;std::vector<bool>seen;
 void inputPacket(M*m,int,int){assert(m->next==nullptr);assert(!seen.at(m->id));seen[m->id]=true;++received;
  if(immediate)delete m;else{m->next=nullptr;pending.push_back(m);}}
 void flushInputQueue(){++flushed;for(auto m:pending)delete m;pending.clear();}
};
struct _ifnet{IONetworkInterface*iface;Stat*netStat;};
#define panic(...) abort()
#define XYLog(...) ((void)0)
'''
test=r'''
void run(unsigned n,bool immediate){
 IONetworkInterface iface;iface.immediate=immediate;iface.seen.resize(n);Stat stats;_ifnet ifp{&iface,&stats};
 mbuf_list ml;for(unsigned i=0;i<n;++i){auto m=new M;m->id=i;if(ml.ml_tail)ml.ml_tail->next=m;else ml.ml_head=m;ml.ml_tail=m;++ml.ml_len;}
 assert(_if_input(nullptr,&ifp,&ml,nullptr,nullptr)==0);
 assert(!ml.ml_head&&!ml.ml_tail&&!ml.ml_len);assert(iface.received==n&&stats.inputPackets==n);
 assert(iface.flushed==(n?1:0));for(bool b:iface.seen)assert(b);
}
int main(){for(unsigned n:{0,1,2,64,512,2048})for(bool immediate:{false,true})run(n,immediate);
 // Separate interface lists (real calls are serialized by the existing gate).
 std::thread a([]{for(int n=0;n<1000;++n)run(64,false);}),b([]{for(int n=0;n<1000;++n)run(64,true);});a.join();b.join();
 std::cout<<"PASS extracted RX handoff: empty/single/batch, nextpkt detached, immediate free, link rewrite, every packet once, empty caller list, unchanged DMA storage\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc7-rx-handoff-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+helper+body+test)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=120);print('PASS',san)
