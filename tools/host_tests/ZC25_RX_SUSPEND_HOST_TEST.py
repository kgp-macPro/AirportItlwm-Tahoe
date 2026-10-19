#!/usr/bin/env python3
"""Actual retained RX pool across simulated positive-stop/rearm; no hardware claim."""
from pathlib import Path
import ast,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2];s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();h=(R/'itlwm/hal_iwx/ItlIwx.hpp').read_text()
a=ast.parse((R/'tools/host_tests/ZC25_RX_POOL_HOST_TEST.py').read_text());mock=next(ast.literal_eval(n.value) for n in a.body if isinstance(n,ast.Assign) and any(isinstance(x,ast.Name) and x.id=='mock' for x in n.targets));mock=mock[:mock.index('int main(){')]
b=s[s.index('// B1-ZC5 BEGIN:'):s.index('// B1-ZC5 END')];i=b.index('bool ItlIwx::rxZcSetup');mock=mock.replace('STORAGE',b[:i]).replace('METHODS',b[i:]).replace('HEADER',h[h.index('    // B1-ZC4:'):h.index('    // PC1 control is serialized')])
test=r'''
int main(){for(bool explicitMapper:{false,true}){
 ItlIwx d;auto*mapper=explicitMapper?new IOMapper:nullptr;d.fDma5Mapper=mapper;initial(d);std::deque<mbuf_t>held;
 for(unsigned cycle=0;cycle<40;++cycle){
  // Host leases span many transitions. Refill is sometimes pending at suspend.
  for(unsigned k=0;k<768;++k){auto*m=receive(d,k%512);if(k%13==0){auto*c=clone(m);mbuf_freem(m);m=c;}held.push_back(m);if(k%128==127)pump(1);}
  auto*p=d.fRxZcPool;unsigned cmdBefore=liveCMD,completeBefore=completes;terminal(d);
  assert(!d.fRxZcRunning&&d.fRxZcTerminal);assert(completes==completeBefore&&liveCMD==cmdBefore);
  for(unsigned slot=0;slot<512;++slot){auto idx=d.fRxZcSlot[slot];assert(p->entry[idx].state==p->Posted);for(auto m:held)assert(m->data!=p->entry[idx].storage->bytes);}
  // Ordinary suspend retains the pool; its independent worker may safely
  // replenish off-ring completed entries without hardware or host reuse.
  pump();verify(d);assert(p->cacheCount<=p->cacheLimit&&p->working==0);
  ++d.com.sc_generation;assert(d.rxZcArm());assert(d.fRxZcGeneration==unsigned(d.com.sc_generation));d.fPc1Suspended=0;
  if(held.size()>5000){for(unsigned k=0;k<2000;++k){mbuf_freem(held.front());held.pop_front();}}pump();
 }
 // Outstanding original+clone lease survives terminal teardown and a new pool.
 auto*m=held.front();held.pop_front();auto*c=clone(m);terminal(d);gate(d,true);assert(d.rxZcFree());gate(d,false);pump();assert(!liveCMD&&!liveMD);
 for(auto x:held)mbuf_freem(x);assert(liveClusters==1);mbuf_freem(m);assert(liveClusters==1);mbuf_freem(c);if(mapper)mapper->release();empty();
}
std::cout<<"PASS actual RX retention/exchange, 40 repeated stop/rearm transitions per mapper binding, host clones across epochs, interrupted refill, ring wrap, bounded cache, terminal teardown, final reference release. STOP_MASTER/hardware power is mocked.\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc16-rx-suspend-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+test);cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')];print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=180)
