#!/usr/bin/env python3
"""Exercise the actual passive AX handoff; no hardware or network traffic."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def extract(marker):
    a=s.index(marker);b=s.index('{',a);i=b+1;depth=1
    while depth:
        depth+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[a:i]
method=extract('int ItlIwx::axInputHandoff')
assert 'mbuf_' not in method[method.index('const int result=if_input'):]
code=r'''
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <functional>
#include <cstdio>
struct mbuf { mbuf *next=nullptr; size_t size=0,linear=0;uint32_t flags=0;uint8_t bytes[256]={};};
using mbuf_t=mbuf*;
struct mbuf_list {mbuf_t ml_head=nullptr;unsigned ml_len=0;};
struct _ifnet{};
struct iwx_softc {int sc_generation=1;struct { _ifnet ic_if;} sc_ic;};
#define ml_len(m) ((m)->ml_len)
mbuf_t mbuf_nextpkt(mbuf_t m){return m->next;}
size_t mbuf_pkthdr_len(mbuf_t m){return m->size;}
size_t mbuf_len(mbuf_t m){return m->linear;}
void *mbuf_data(mbuf_t m){return m->bytes;}
uint32_t mbuf_flags(mbuf_t m){return m->flags;}
unsigned handoffs=0;int returned=0;std::function<void()> hook;
int if_input(_ifnet*,mbuf_list *ml){++handoffs;while(ml->ml_head){auto *m=ml->ml_head;ml->ml_head=m->next;delete m;}ml->ml_len=0;auto cb=hook;if(cb)cb();return returned;}
class ItlIwx {public:
bool bz=false,he=true;uint32_t fPc1ResetEpoch=1;uint64_t now=1000;
uint64_t fAxInputTicket=0,fAxInputEvents[64][10]={};
bool axHeDiagActive(iwx_softc*){return !bz&&he;}
uint64_t time2GNow(){return ++now;}
int axInputHandoff(iwx_softc*,mbuf_list*,unsigned);
};
'''+method+r'''
mbuf *packet(unsigned kind=0){auto *m=new mbuf;m->size=m->linear=55;m->bytes[12]=8;m->bytes[14]=0x45;m->bytes[16]=0;m->bytes[17]=41;m->bytes[23]=6;m->bytes[26]=192;m->bytes[27]=168;m->bytes[28]=178;m->bytes[29]=197;m->bytes[30]=192;m->bytes[31]=168;m->bytes[32]=178;m->bytes[33]=135;m->bytes[34]=0x14;m->bytes[35]=0x51;m->bytes[36]=0xc3;m->bytes[37]=0xb3;m->bytes[38]=0x39;m->bytes[39]=0x94;m->bytes[40]=0xcc;m->bytes[41]=0xa7;m->bytes[42]=0x66;m->bytes[43]=3;m->bytes[44]=0x30;m->bytes[45]=0x69;m->bytes[46]=0x50;m->bytes[47]=0x18;m->bytes[54]=13;
switch(kind){case 1:m->bytes[23]=17;break;case 2:m->linear=40;break;case 3:m->bytes[14]=0x65;break;case 4:m->bytes[35]=1;break;case 5:m->bytes[46]=0xf0;break;case 6:m->bytes[20]=0x20;break;case 7:m->size=1200;break;case 8:m->bytes[17]=10;break;}return m;}
void one(ItlIwx &d,iwx_softc &sc,unsigned kind=0){mbuf_list ml{packet(kind),1};unsigned before=handoffs;bool nested=bool(hook);assert(d.axInputHandoff(&sc,&ml,1)==returned);assert(handoffs>=before+1&&!ml.ml_head&&!ml.ml_len);if(!nested)assert(handoffs==before+1);}
int main(){iwx_softc sc;ItlIwx d;d.bz=true;returned=19;one(d,sc);assert(!d.fAxInputTicket);d.bz=false;d.he=false;one(d,sc);assert(!d.fAxInputTicket);d.he=true;returned=0;
for(unsigned k=1;k<=8;++k)one(d,sc,k);assert(!d.fAxInputTicket);
one(d,sc);auto *w=d.fAxInputEvents[0];assert(w[0]==1&&w[1]&&w[2]>w[1]&&w[3]==((1ULL<<32)|1));assert(w[4]==0x3994cca766033069ULL);assert(w[5]==0xc0a8b2c5c0a8b287ULL);assert((w[6]&0xffff)==5201&&((w[6]>>16)&0xffff)==50099);assert((w[7]&65535)==41&&((w[7]>>16)&65535)==1&&w[8]==0x0d000000);
for(unsigned i=0;i<70;++i)one(d,sc);assert(d.fAxInputTicket==71&&d.fAxInputEvents[6][0]==71);
// Simulate gate-reentrant work overwriting the first record. The outer
// return must not complete/retimestamp a newer ticket that reused its slot.
ItlIwx r;hook=[&](){hook=nullptr;for(unsigned i=0;i<65;++i)one(r,sc);};one(r,sc);assert(r.fAxInputTicket==66);assert(r.fAxInputEvents[0][0]==65&&r.fAxInputEvents[0][2]<r.now);
// Empty delivery is still exactly one original call, no diagnostic clock.
mbuf_list empty;auto old=d.now;auto before=handoffs;assert(d.axInputHandoff(&sc,&empty,3)==0&&handoffs==before+1&&d.now==old);
puts("PASS actual AX handoff parser, exact one delivery/return, consumed storage, malformed/UDP skips, BZ/HE-off bypass, ring wrap/reentry; ASan/UBSan");}
'''
with tempfile.TemporaryDirectory(prefix='p10r8d-host-') as t:
    p=Path(t);(p/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++14','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
