#!/usr/bin/env python3
"""2.0.0 host regression gate. Does not load drivers or run network tests."""
from pathlib import Path
import concurrent.futures,datetime,hashlib,json,os,subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=False)
checks=[(n,'address,undefined') for n in ['BE200_TRANSPORT_HOST_TEST','BE200_RECOVERY_HOST_TEST','BE200_QOS_HOST_TEST','BE200_LINK_HOST_TEST','AX_RX_CREDIT_HOST_TEST']]
checks +=[(n,None)for n in ['P10_HE_IE_HOST_TEST','P10_HE_FW_API_TEST','P10R7_AX_FEEDBACK_TEST','P10R8D_AX_HANDOFF_TEST','P10R9_AX_RX_BUDGET_TEST']]
# Retain TSan coverage of pre-existing ownership/lifetime models as well.
checks +=[(n,'thread')for n in ['BE200_TRANSPORT_HOST_TEST','BE200_RECOVERY_HOST_TEST','BE200_QOS_HOST_TEST','BE200_LINK_HOST_TEST','AX_RX_CREDIT_HOST_TEST']]
def run(case):
 n,san=case;cmd=[sys.executable,'-B',str(r/'tools/host_tests'/(n+'.py'))]+([san]if san else[]);log=out/(n+'-'+(san or 'default').replace(',','-')+'.log');env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 with log.open('w')as f:
  try:code=subprocess.run(cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=240).returncode
  except subprocess.TimeoutExpired:code=124
 item=dict(test=n,sanitizer=san,exit=code,log=log.name,sha256=hashlib.sha256(log.read_bytes()).hexdigest());print('PASS'if not code else'FAIL',n,san,flush=True)
 if code:print(log.read_text(errors='replace')[-5000:],flush=True)
 return item
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:results=list(pool.map(run,checks))
result=dict(status='PASS'if all(x['exit']==0 for x in results)else'FAIL',checks=results,time_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),limitations='Actual extracted methods with mocked KPIs/hardware. No physical/firmware/RF qualification.')
(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(result['status'],len(results),'checks');sys.exit(0 if result['status']=='PASS'else 1)
