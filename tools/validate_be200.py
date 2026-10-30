#!/usr/bin/env python3
"""Offline/source checks only; NO driver deployment, live collection or network."""
from pathlib import Path
import sys,subprocess,concurrent.futures,json,hashlib,os,datetime,importlib.util,plistlib,struct
R=Path(__file__).resolve().parents[1];O=Path(sys.argv[1]);O.mkdir(parents=True,exist_ok=False)
checks=[('BE200_RECOVERY_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_LINK_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_CONNECTION_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_PNVM_COMPLETION_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_PNVM_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_BOOT_SYNC_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_TRANSPORT_HOST_TEST',s)for s in ['address,undefined','thread']]+[('BE200_LIFECYCLE_HOST_TEST',s)for s in ['address,undefined','thread']]
checks +=[(n,'address,undefined') for n in ['ZC16_COMMAND_HOST_TEST','ZC16_REORDER_HOST_TEST','ZC28_BATCH_HOST_TEST','ZC7_ADMISSION_HOST_TEST','ZC28_SERVICE_OBSERVER_HOST_TEST','ZC28_DRAIN_HOST_TEST','ZC16_LIFECYCLE_HOST_TEST','ZC28_RATE_UPDATE_HOST_TEST']]+[('ZC28_RX_DECODER_TEST',None),('BE200_JOIN_TOOL_TEST',None)]
def run(v):
 n,san=v;cmd=[sys.executable,'-B',str(R/'tools/host_tests'/(n+'.py'))]+([san]if san else[]);p=O/(n+'-'+(san or 'python').replace(',','-')+'.log');env=dict(os.environ);env['PYTHONDONTWRITEBYTECODE']='1';env['ASAN_OPTIONS']='halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
 with p.open('w') as f:x=subprocess.run(cmd,cwd=R,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=240)
 rec={'test':n,'sanitizer':san,'command':cmd,'exit':x.returncode,'log':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()};print('PASS'if not x.returncode else'FAIL',n,san,flush=True);return rec
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:results=list(pool.map(run,checks))
cmd=[sys.executable,'-B',str(R/'tools/BE200_FW_ABI_TEST.py'),str(O/'firmware-abi')]
with (O/'firmware-abi.log').open('w')as f:x=subprocess.run(cmd,cwd=R,stdout=f,stderr=subprocess.STDOUT,timeout=90)
results.append({'test':'GL92 production selector / current29 wire assertions / SDK slot / embedded payloads','exit':x.returncode,'log':str(O/'firmware-abi.log')})
# Offline collector/decoder corruption/unknown/absent and schema tests.
subprocess.run(['/bin/bash','-n',str(R/'docs/private-be200/COLLECT_BE200.sh')],check=True)
spec=importlib.util.spec_from_file_location('bzdecode',R/'docs/private-be200/decode_be200.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
w=[200,1,384]+list(range(45));raw=struct.pack('<48Q',*w);node={'IORegistryEntryName':'ARPT','KGP_BE200_TRANSPORT':raw}
assert m.decode([{'children':[node]}])['snapshots'][0]['raw_hex']==raw.hex();assert m.decode([])['status']=='NO_BZ_WATCHDOG_RECORD'
boot=m.decode({'IORegistryEntryName':'ARPT','vendor-id':bytes.fromhex('86800000'),'device-id':bytes.fromhex('2b270000'),'KGP_BE200_CANDIDATE':'BE200-GL92-PRIVATE-P2','KGP_BE200_STAGE':'hal:firmware-context','KGP_BE200_ERROR':5,'KGP_BE200_BOOT_SYNC_RESULT':0xe00002c2,'ItlwmPnvmName':'iwlwifi-gl-c0-fm-c0-.pnvm','ItlwmPnvmFailStep':2,'ItlwmPnvmReadError':22})
assert boot['providers'][0]['properties']['KGP_BE200_BOOT_SYNC_RESULT']==0xe00002c2 and boot['providers'][0]['pci_identity']['device-id']=='0x272b'
assert boot['providers'][0]['properties']['ItlwmPnvmReadError']==22 and boot['providers'][0]['properties']['ItlwmPnvmFailStep']==2
for bad in [b'',raw[:-1],struct.pack('<48Q',199,*w[1:])]:
 try:m.decode({'KGP_BE200_TRANSPORT':bad})
 except ValueError:pass
 else:raise AssertionError('Malformed trace accepted')
subprocess.run(['/bin/bash','-n',str(R/'docs/private-be200/COLLECT_BE200_JOIN.sh')],check=True)
# Actual decoder, simulated stage/fault history including reset boundary and wrap.
trace=[201,1,4160,70,64,6,64,0]+[0]*512
for serial in range(7,71):
 i=(serial-1)%64;trace[8+i*8:16+i*8]=[serial,serial*1000000,1 if serial<60 else 3,1 if serial<60 else 2,10 if serial==59 else 2,3,100,0]
blob=struct.pack('<520Q',*trace);history=m.connection(blob)
assert history['overwritten']==6 and len(history['events'])==64 and history['events'][0]['serial']==7 and history['events'][-1]['reset_epoch']==2
assert m.decode({'KGP_BE200_CONNECTION':blob})['connection_histories'][0]['raw_hex']==blob.hex()
for which in range(5):
 bad=list(trace)
 if which==0:bad[0]=202
 if which==1:bad[4]=63
 if which==2:bad[8]=0
 if which==3:bad[5]=0
 if which==4:bad[6]=65
 try:m.connection(struct.pack('<520Q',*bad))
 except ValueError:pass
 else:raise AssertionError('Malformed connection trace accepted')
results.append({'test':'both collector syntax / actual decoder unknown,truncated,wrap,overwrite,missing-event,epoch fixtures','exit':0})
# Source/data preservation boundaries, not a regex substitute for transport tests.
G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip()
unchanged=['itl80211/openbsd/sys/_ifq.cpp','itl80211/openbsd/sys/_ifq.h','tools/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh']
for name in unchanged:assert (R/name).read_bytes()==subprocess.check_output([G,'show','483940988c502379fb6d3dcde885dca83f00b7aa:'+name],cwd=R)
# Only the audited link handoff + explicit initialization may differ in legacy frontend.
front=(R/'AirportItlwm/AirportItlwm.cpp').read_text()
old=subprocess.check_output([G,'show','483940988c502379fb6d3dcde885dca83f00b7aa:AirportItlwm/AirportItlwm.cpp'],cwd=R,text=True)
a=front.index('// P6: IO80211Interface arms');b=front.index('\nvoid AirportItlwm::releaseAll()',a)
c=old.index('bool AirportItlwm::\nsetLinkStatus(');d=old.index('\nvoid AirportItlwm::releaseAll()',c)
init='    currentStatus = 0;\n    fLinkStateHandoff = false;\n    fLinkStateCallbackSeen = false;\n    fLinkStateCallbackOK = false;\n'
assert front.count(init)==1
assert (front[:a]+front[b:]).replace(init,'')==old[:c]+old[d:]
results.append({'test':'legacy frontend byte-preserved except audited link handoff/init; shared queues/frozen runner unchanged','exit':0})
files=subprocess.check_output([G,'ls-files','-z','--cached','--others','--exclude-standard'],cwd=R).split(b'\0');manifest={}
for f in files:
 if not f:continue
 p=R/os.fsdecode(f);manifest[os.fsdecode(f)]=hashlib.sha256(p.read_bytes()).hexdigest()
r={'status':'PASS'if all(x['exit']==0 for x in results)else'FAIL','timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output([G,'rev-parse','HEAD'],cwd=R,text=True).strip(),'checks':results,'validated_inputs':manifest,'limitations':'Mock KPI/MMIO/DMA, compiler ABI and source checks only. No hardware, physical throughput, actual cache policy, firmware acceptance or sleep/wake qualification.'}
(O/'RESULT.json').write_text(json.dumps(r,indent=2)+'\n');print(r['status'],sum(x['exit']==0 for x in results),'/',len(results));sys.exit(0 if r['status']=='PASS'else 1)
