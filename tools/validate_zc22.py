#!/usr/bin/env python3
"""Bounded source/ownership/sanitizer validation, never a throughput benchmark."""
from pathlib import Path
import subprocess,hashlib,json,datetime,concurrent.futures,sys,os
R=Path(__file__).resolve().parents[1]
O=Path('/Users/kgp/Desktop/AirportItlwm/B1-ZC22-PERF/evidence')
name=sys.argv[1] if len(sys.argv)>1 else 'validation-01'
assert name.startswith('validation-') and '/' not in name
D=O/name;D.mkdir(parents=True)
models=['ZC22_TVQM_HOST_TEST','ZC22_PUBLICATION_HOST_TEST','ZC19_HEALTH_HOST_TEST','ZC22_CONTROL_HOST_TEST','ZC15_RX_CACHE_EXCHANGE_HOST_TEST','ZC16_COMMAND_HOST_TEST','ZC16_REORDER_HOST_TEST','ZC16_LIFECYCLE_HOST_TEST','ZC16_RX_SUSPEND_HOST_TEST','ZC18_OLDSN_HOST_TEST','ZC18_SESSION_HOST_TEST','RX_ZC5_HOST_TEST','ZC7_RX_HANDOFF_HOST_TEST','TX_ZC5_HOST_TEST','ZC12_DRAIN_HOST_TEST','ZC7_ADMISSION_HOST_TEST','ZC8_BATCH_HOST_TEST']
cases=[('ZC22_SOURCE_CHECK',None)]+[(n,san) for n in models for san in ['address,undefined','thread']]
if len(sys.argv)>2:cases=[v for v in cases if v[0] in sys.argv[2:]]
assert cases

def run(case):
 n,san=case;cmd=[sys.executable,'-B',str(R/'tools/host_tests'/ (n+'.py'))]+([san] if san else [])
 log=D/(n+'-'+(san or 'source').replace(',','-')+'.log')
 start=datetime.datetime.now(datetime.timezone.utc).isoformat()
 env=dict(os.environ);env['PYTHONDONTWRITEBYTECODE']='1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1';env['ASAN_OPTIONS']='halt_on_error=1'
 with log.open('wb') as f:p=subprocess.run(cmd,cwd=R,stdout=f,stderr=subprocess.STDOUT,env=env)
 result={'model':n,'sanitizer':san,'command':cmd,'exit_status':p.returncode,'start_utc':start,'end_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'log':str(log),'sha256':hashlib.sha256(log.read_bytes()).hexdigest()}
 print(('PASS' if p.returncode==0 else 'FAIL'),n,san,flush=True)
 if p.returncode:print(log.read_text()[-7000:],flush=True)
 return result
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,cases))
(D/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n')
print('Validation',sum(r['exit_status']==0 for r in results),'/',len(results),flush=True)
sys.exit(1 if any(r['exit_status'] for r in results) else 0)
