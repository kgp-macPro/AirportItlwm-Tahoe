#!/usr/bin/env python3
"""Source-derived ownership test, not a throughput model or DMA proof.
Compare frozen ZC13 with this worktree using the inherited IOKit/lifetime harness.
"""
from pathlib import Path
import ast,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2]
base='9d6942e8dedc482a56b0ba248e778735c4dd2999'
git=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
harness=ast.parse((R/'tools/host_tests/ZC25_RX_POOL_HOST_TEST.py').read_text())
mock=next(ast.literal_eval(n.value) for n in harness.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='mock' for t in n.targets))
mock=mock[:mock.index('int main(){')]
test=r'''
int main(){
 for(bool explicitMapper:{false,true})for(unsigned cursor:{0u,2040u})
 for(int scenario:{0,1,2,3,4}){
  ItlIwx d;IOMapper*mapper=explicitMapper?new IOMapper:nullptr;d.fDma5Mapper=mapper;
  gate(d,false);assert(d.rxZcSetup());auto*p=d.fRxZcPool;
  // Fixture uses a single exclusively worker-owned, off-ring Entry. No DMA is
  // posted: repeated completed packet leases populate the bounded cache.
  auto&e=p->entry[0];e.state=p->Working;std::vector<mbuf_t>held;
  for(unsigned n=0;n<p->cacheLimit;++n){held.push_back(e.view);e.view=nullptr;
   assert(p->replenish(e)==0);e.state=p->Working;}
  assert(p->cacheCount==p->cacheLimit&&liveCMD==4096);
  p->cacheCursor=cursor;
  // 0: hit; 1: all held; 2: returned beyond this bounded scan; 3: hit/header
  // failure and retry; 4: original mbuf freed but a clone still owns the lease.
  unsigned target=(cursor+(scenario==2?100:7))%p->cacheLimit;
  auto*returned=p->cache[target].storage;auto*returnedCommand=p->cache[target].command;
  mbuf_t cloneHeld=nullptr;
  if(scenario!=1){auto it=std::find_if(held.begin(),held.end(),[&](mbuf_t m){return m&&m->data==returned->bytes;});assert(it!=held.end());
   if(scenario==4)cloneHeld=clone(*it);mbuf_freem(*it);*it=nullptr;}
  auto*incoming=e.storage;auto*incomingCommand=e.command;held.push_back(e.view);e.view=nullptr;
  assert(!incoming->returned); // The live host lease is retained, never reused!
  uint64_t prep=prepares,done=completes,hits=p->cacheStats[6],retained=p->cacheStats[8],refused=p->cacheStats[9],scans=p->cacheStats[11];
  bool hit=scenario==0||scenario==3;
  if(scenario==3){failStage=8;failAfter=0;failCalls=0;}
  unsigned result=p->replenish(e);assert(result==(scenario==3?1u:0u));
  failStage=0;failCalls=0;
  if(hit){
   assert(e.storage==returned&&e.command==returnedCommand);
   assert(prepares==prep&&p->cacheStats[6]==hits+1);
#if EXPECT_EXCHANGE
   assert(completes==done&&p->cacheCount==2048&&liveCMD==4096);
   assert(p->cache[target].storage==incoming&&p->cache[target].command==incomingCommand);
   assert(p->cache[target].state==p->Retired&&!p->cache[target].view);
   assert(p->cacheStats[8]==retained+1&&p->cacheStats[9]==refused);
#else
   assert(completes==done+1&&p->cacheCount==2047&&liveCMD==4095);
   assert(p->cacheStats[8]==retained&&p->cacheStats[9]==refused+1);
#endif
  }else{
   assert(prepares==prep+1&&completes==done+1&&p->cacheCount==2048&&liveCMD==4096);
   assert(p->cacheStats[6]==hits&&p->cacheStats[9]==refused+1);
  }
  assert(p->cacheStats[11]-scans<=p->batch); // no additional lookup on miss
  if(scenario==3){assert(!e.view&&e.commandPrepared&&!p->failed());
   assert(p->replenish(e)==0);assert(prepares==prep);}
  assert(e.view&&e.storage!=incoming); // no host-held storage reused
  if(cloneHeld)mbuf_freem(cloneHeld);
  // Teardown with outstanding host leases, including the exchanged incoming.
  // Mappings are released once; storage lasts until final callback.
  assert(d.rxZcFree());pump();assert(!liveCMD&&!liveMD);
  for(auto m:held)if(m)mbuf_freem(m);
  if(mapper)mapper->release();empty();
 }
 assert(!liveObjects&&lateCopies==0);
 std::cout<<"PASS "<<EXPECT_EXCHANGE<<": actual full-cache hit/exchange, bounded miss, cursor wrap, final clone, header failure/retry, explicit/implicit mapper, late-host teardown; no added mappings or double completion.\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
for label,src,expected in [('ZC13-control',subprocess.check_output([git,'--no-optional-locks','-C',str(R),'show',base+':itlwm/hal_iwx/ItlIwx.cpp']).decode(),0),('ZC15',(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text(),1)]:
 block=src[src.index('// B1-ZC5 BEGIN:'):src.index('// B1-ZC5 END')];cut=block.index('bool ItlIwx::rxZcSetup');storage,methods=block[:cut],block[cut:]
 hpp=(R/'itlwm/hal_iwx/ItlIwx.hpp').read_text();header=hpp[hpp.index('    // B1-ZC4:'):hpp.index('    // PC1 control is serialized')]
 text=mock.replace('STORAGE',storage).replace('HEADER',header).replace('METHODS',methods)
 with tempfile.TemporaryDirectory(prefix='zc15-cache-'+label+'-') as td:
  p=Path(td);(p/'test.cpp').write_text('#define EXPECT_EXCHANGE '+str(expected)+'\n'+text+test)
  cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
  print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
print('PASS actual-source before/after ownership comparison',san)
