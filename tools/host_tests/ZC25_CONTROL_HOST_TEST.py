#!/usr/bin/env python3
"""Extract real allocation/free/byte-table code. Mock only IOKit/hardware. No throughput test."""
from pathlib import Path
import hashlib
import re,subprocess,tempfile,sys,os
r=Path(os.environ.get("ZC25_SOURCE_ROOT",Path(__file__).resolve().parents[2]));src=(r/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def mask(t):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),t,flags=re.S)
def extract(name):
 m=mask(src);x=re.search(r'(?:static )?(?:bool|void|int|IOReturn)\s+(?:ItlIwx::\s*)?'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return src[x.start():b]
mock=r'''
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
using UInt64=uint64_t;using UInt32=uint32_t;using IOReturn=int;using bus_size_t=size_t;
constexpr int kIOReturnSuccess=0,kIOReturnNoMemory=1,kIOReturnBadArgument=2;
constexpr int kIODirectionInOut=3,kIODirectionOut=2,kIOMemoryPhysicallyContiguous=0x10,kIOMemoryHostPhysicallyContiguous=0x20,kIOMemoryMapperNone=0x40,kIOMapInhibitCache=0x80,kIODMACommandOutputHost64=64;
#define DMA_BIT_MASK(n) UINT64_MAX
void*kernel_task=nullptr;
struct IOMapper{int id;};
static int fault=0,liveBuffers=0,liveCommands=0,lastOptions=0,syncs=0,allocs=0;
static unsigned lastAlignment=0;static IOMapper*lastMapper=nullptr;
struct IOBufferMemoryDescriptor{
 void*bytes=nullptr;size_t size;bool prepared=false;
 IOBufferMemoryDescriptor(size_t s):size(s){assert(!posix_memalign(&bytes,4096,s));memset(bytes,0xa5,s);++liveBuffers;}
 ~IOBufferMemoryDescriptor(){assert(!prepared);free(bytes);--liveBuffers;}
 static IOBufferMemoryDescriptor*inTaskWithPhysicalMask(void*,int opts,size_t n,uint64_t mask){++allocs;lastOptions=opts;assert(mask==UINT64_MAX);if(fault==1)return nullptr;return new IOBufferMemoryDescriptor(n);}
 int prepare(){if(fault==2)return 1;prepared=true;return 0;}
 int complete(){assert(prepared);prepared=false;return 0;}
 void release(){delete this;}
 void*getBytesNoCopy(){return fault==10?nullptr:bytes;}
};
struct IODMACommand{
 enum{kMapped=1};struct Segment64{uint64_t fIOVMAddr=0,fLength=0;};
 IOBufferMemoryDescriptor*md=nullptr;IOMapper*mapper=nullptr;
 explicit IODMACommand(IOMapper*m):mapper(m){++liveCommands;}
 ~IODMACommand(){assert(!md);--liveCommands;}
 static IODMACommand*withSpecification(int output,unsigned bits,int maxseg,int mode,int maxtrans,unsigned align,IOMapper*m){assert(output==64&&bits==64&&mode==kMapped&&maxseg==0&&maxtrans==0);lastAlignment=align;lastMapper=m;if(fault==3)return nullptr;return new IODMACommand(m);}
 int setMemoryDescriptor(IOBufferMemoryDescriptor*m){if(fault==4)return 1;md=m;assert(md->prepared);return 0;}
 int gen64IOVMSegments(UInt64*off,Segment64*s,UInt32*n){assert(md&&*off==0&&*n==1);if(fault==5)return 1;*off=md->size;*n=1;s->fLength=*off;s->fIOVMAddr=mapper?0x1234500000ULL:0x234500000ULL;
  if(fault==6)*n=2;if(fault==7)--*off;if(fault==8)--s->fLength;if(fault==9)++s->fIOVMAddr;return 0;}
 IOBufferMemoryDescriptor*getIOMemoryDescriptor(){return fault==11?nullptr:md;}
 int synchronize(int dir){assert(md&&dir==kIODirectionOut);++syncs;return fault==12?1:0;}
 void clearMemoryDescriptor(){md=nullptr;}
 void release(){delete this;}
};
struct iwx_dma_info{IOBufferMemoryDescriptor*buffer=nullptr;IODMACommand*cmd=nullptr;void*vaddr=nullptr;uint64_t paddr=0;size_t size=0;};
struct ItlIwx{bool fDma5Selected=false;IOMapper*fDma5Mapper=nullptr;
 int iwx_tx_control_alloc(iwx_dma_info*,bus_size_t,bus_size_t,bool=false);void iwx_dma_contig_free(iwx_dma_info*);
};
'''
test=r'''
int main(){IOMapper mapper{1};
 for(IOMapper*m:{(IOMapper*)nullptr,&mapper})for(size_t n:{size_t(512),size_t(2048),size_t(65536),size_t(82944)})for(size_t a:{size_t(0),size_t(64),size_t(256)})for(bool inhibited:{false,true}){
  ItlIwx d;d.fDma5Selected=true;d.fDma5Mapper=m;iwx_dma_info q;
  assert(!d.iwx_tx_control_alloc(&q,n,a,inhibited));assert(q.size==n&&q.cmd&&q.buffer&&q.vaddr);
  assert(lastMapper==m&&lastAlignment==(a?a:1)&&bool(lastOptions&kIOMapInhibitCache)==inhibited);
  assert(m?(lastOptions&kIOMemoryHostPhysicallyContiguous)&&(lastOptions&kIOMemoryMapperNone):(lastOptions&kIOMemoryPhysicallyContiguous));
  assert(q.paddr==(m?0x1234500000ULL:0x234500000ULL));assert(!memcmp(q.vaddr,std::vector<char>(n,0).data(),n));
  d.iwx_dma_contig_free(&q);assert(!q.vaddr&&!q.cmd&&!q.buffer&&!liveBuffers&&!liveCommands);
 }
 for(int f=1;f<=12;++f)for(IOMapper*m:{(IOMapper*)nullptr,&mapper})for(bool inhibited:{false,true}){
  fault=f;ItlIwx d;d.fDma5Selected=true;d.fDma5Mapper=m;iwx_dma_info q;
  assert(d.iwx_tx_control_alloc(&q,65536,256,inhibited));assert(!q.vaddr&&!q.cmd&&!q.buffer&&!q.paddr&&!liveBuffers&&!liveCommands);
 }
 fault=0;
 for(IOMapper*m:{(IOMapper*)nullptr,&mapper}){iwx_dma_info q;unsigned stage=0;
  assert(allocatePacketBacking2H(&q,4096,64,&stage,m,true)==0);
  assert(!(lastOptions&kIOMapInhibitCache));ItlIwx d;d.iwx_dma_contig_free(&q);assert(!liveBuffers&&!liveCommands);
 }
 {ItlIwx d;iwx_dma_info q;int old=allocs;assert(d.iwx_tx_control_alloc(&q,100,64));assert(allocs==old);
 d.fDma5Selected=true;for(auto pair:std::vector<std::pair<size_t,size_t>>{{0,64},{100,3},{100,size_t(UINT_MAX)+1}}){assert(d.iwx_tx_control_alloc(&q,pair.first,pair.second));assert(allocs==old);}}
 assert(!liveBuffers&&!liveCommands);std::cout<<"PASS actual checked control allocator/free: both mapper arguments, complete aligned IOVM, cached readonly controls versus inhibited isolated first-TB options, initialization, all 12 injected failures, no partial publication/leak, invalid/unresolved selection rejected. Hardware cache/PAT and throughput NOT modeled.\n";
}
'''
print('SOURCE_CPP_SHA256='+hashlib.sha256(src.encode()).hexdigest(),flush=True)
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc25-control-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+'\n'+extract('allocatePacketBacking2H')+'\n'+extract('iwx_tx_control_alloc')+'\n'+extract('iwx_dma_contig_free')+'\n'+test)
 c=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',c,flush=True);subprocess.run(c,check=True);subprocess.run([str(p/'test')],check=True,timeout=60);print('PASS',san)
