#!/usr/bin/env python3
"""Extract current gate/drain code into preserved ownership/lifecycle mocks."""
from pathlib import Path
import re,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2];src=(r/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
model=(r/'tools/host_tests/GATE_DRAIN_TEST.cpp').read_text()
def mask(t):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),t,flags=re.S)
def span(t,name):
 m=mask(t);x=re.search(r'(?:bool|void|uint32_t|IOReturn)\s+ItlIwx::\s*'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return x.start(),b
for name in ['drain2MSetup','drain2MRequest','drain2MPause','drain2MArm','drain2MRemove','drain2MFence','drain2MAction','_iwx_start_task','iwx_start']:
 a,b=span(src,name);c,d=span(model,name);model=model[:c]+src[a:b]+model[d:]
model=model.replace('struct ItlIwx {','struct ItlIwx {\n bool mapped=true;bool dma5Mapped(){return mapped;}\n static IOReturn dexterOriginal__iwx_start_task(OSObject*,void*,void*,void*,void*);\n static void dexterOriginal_iwx_start(_ifnet*);')
model=model.replace('attemptAction(Action f,void*a,void*b)','attemptAction(Action f,void*a,void*b=nullptr)')
extra=''
for name in ['dexterOriginal__iwx_start_task','dexterOriginal_iwx_start']:
 a,b=span(src,name);extra+=src[a:b]+'\n'
model=model.replace('int main(){',extra+'\nint mapped_tests(){')
# Existing historical main is no longer special: supply explicit return.
model=model.replace('puts("ALL 2M SOURCE-DERIVED OWNERSHIP/SCHEDULING/RACE TESTS PASS (mock SDK/workloop, not kernel/runtime proof)");','puts("ALL 2M SOURCE-DERIVED OWNERSHIP/SCHEDULING/RACE TESTS PASS (mock SDK/workloop, not kernel/runtime proof)");return 0;')
model+=r"""
int main(){mapped_tests();
 {Test t;t.x.mapped=false;Packet a,b;t.enqueue(a);t.x.com.sc_ic.ic_if.if_snd.packets.push_back(&b);
  contended(t.x,[&]{t.start();});assert(!a.consumed&&t.x.fDrain2MPending);
  t.x.loop.deliver();assert(a.consumed&&b.consumed&&t.x.txCalls==2&&t.x.fDrain2MStats[7]==1);
  t.x.loop.deliver();assert(t.x.txCalls==2);}
 {Test t;t.x.mapped=false;Packet a;t.enqueue(a);contended(t.x,[&]{for(int n=0;n<30;++n)t.start();});
  assert(t.x.fDrain2MStats[2]==1);t.start();t.x.loop.deliver();assert(t.x.txCalls==1);}
 {Test t;t.x.mapped=false;Packet a;t.enqueue(a);contended(t.x,[&]{t.start();});
  auto e=t.x.drain2MPause();t.x.com.sc_ic.ic_if.if_flags=0;t.x.loop.deliver();assert(!a.consumed);
  t.x.drain2MArm(e);t.start();assert(!a.consumed);}
 {Test t;t.x.mapped=false;Packet a;t.enqueue(a);contended(t.x,[&]{t.start();});
  t.x.com.qfullmsk=1;t.x.loop.deliver();assert(!a.consumed&&t.x.com.sc_ic.ic_if.if_snd.ifq_oactive);
  assert(!t.x.fDrain2MPending);}
 assert(liveLocks==0&&liveEvents==0);
 puts("PASS both DMA routes: actual current dispatcher and drain, contended last-batch recovery, coalescing, no duplicate ownership, management-first order, full-ring stop, pause/epoch/fence/detach safety. No throughput test.");
}
"""
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc8-drain-') as t:
 p=Path(t);(p/'test.cpp').write_text(model)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],cwd=p,check=True,timeout=180);print('PASS',san)
