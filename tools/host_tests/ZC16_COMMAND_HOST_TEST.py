#!/usr/bin/env python3
"""Current command state machine in preserved KPI/ring mocks; no firmware proof."""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2];src=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();model=(R/'tools/host_tests/POWER_CYCLE_HOST_TEST.cpp').read_text()
def span(s,name):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S);x=re.search(r'(?:void|bool|int)\s+ItlIwx::\s*'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return x.start(),b
for name in ['cmd2KAvailable','cmd2KClaim','cmd2KPrepare','cmd2KFinish','cmd2KDone','cmd2KDoneEnd','cmd2KResetBegin','cmd2KResetEnd','cmd2KQuarantine','cmd2KError','cmd2KObserve','cmd2KReport','cmd2KFree']:
 a,b=span(src,name);c,d=span(model,name);model=model[:c]+src[a:b]+model[d:]
# Preserve the historical hardware/ring mocks explicitly. Their reset/free
# simulates callbacks; current command functions above are never reimplemented.
model=model.replace('struct ItlIwx {','struct ItlIwx {\n void*fDma5Mapper=nullptr;')
model=model.replace('unsigned alignment, unsigned *stage)\n{','unsigned alignment, unsigned *stage, void *mapper=nullptr)\n{\n    assert(mapper==nullptr); // mock implicit mapper, actual qualification tested separately\n')
model=model[:model.index('// Analysis-only counterexamples.')]
model=model.replace('int golden_main(){runExistingTxTests();','int golden_main(){')
model=model.replace('ALL SOURCE-DERIVED COMMAND + EXISTING TX TESTS PASS (mock IOKit; not runtime proof)','CURRENT SOURCE COMMAND STATE MACHINE PASS (historical hardware/ring mocks; NOT current TX-ring implementation or runtime proof)')
model+='\nint main(){return golden_main();}\n'
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc16-command-') as t:
 p=Path(t);(p/'test.cpp').write_text(model);cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')];print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],cwd=p,check=True,timeout=120)
