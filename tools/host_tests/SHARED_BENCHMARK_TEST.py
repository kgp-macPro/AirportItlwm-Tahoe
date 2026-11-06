#!/usr/bin/env python3
"""Offline shared-runner protocol, strict guards and actual archive/trap fixtures."""
from pathlib import Path
import importlib.util,subprocess,sys,unittest,hashlib,json,shlex,tempfile,zipfile,re
R=Path(__file__).resolve().parents[2]
P=R/'tools/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh';S=P.read_text()
spec=importlib.util.spec_from_file_location('prior_guards',R/'tools/host_tests/ZC25_BENCHMARK_GUARD_TEST.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
g.RUNNER=P;g.SOURCE=S
B=subprocess.check_output(['git','-C',str(R),'show','7dd260fbfb9d612574a6b18fbb042d4963402eda:tools/ZC22_3x14_PORTABLE_IP_PROMPT.sh'],text=True)
TIGHT="""# Shared runner supports these established 10/2 identities explicitly.
if label not in ('ZC13','ZC18'):sys.exit('STOP: candidate has no supported health schema in this shared runner')
if legacy_fields['KGP_RX_EXPERIMENT']!=['AirportItlwm-Tahoe-B1-'+label+'-PERF']:
    sys.exit('STOP: wrong KGP_ZC10 candidate marker')
"""
class SharedGuards(g.BenchmarkGuard):
 def test_legacy_guards_and_benchmark_protocol_unchanged(self):
  # This test compares inherited protocol text; legacy marker tightening is
  # tested independently below and cannot be hidden by the comparison.
  self.assertEqual(S.count(TIGHT),1);previous=g.SOURCE
  try:
   g.SOURCE=S.replace(TIGHT,'');super().test_legacy_guards_and_benchmark_protocol_unchanged()
  finally:g.SOURCE=previous
 def test_all_required_19_identities_and_modes_are_strict(self):
  for label in ['ZC19','ZC20','ZC21','ZC22','ZC23','ZC24','ZC25','ZC26']:
   marker='AirportItlwm-Tahoe-B1-ZC20-TX-CMD-UC' if label=='ZC20' else 'AirportItlwm-Tahoe-B1-'+label+'-PERF'
   for mode,n in [('mapped',2),('conventional',1)]:
    words=g.record(mode=n);src=self.path/'all.plist';src.write_bytes(g.plistlib.dumps(g.fixture(words,marker)))
    cmd=[sys.executable,str(self.guard),str(src),'NONE','NONE',mode,label,'boundary']
    p=subprocess.run(cmd,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stderr)
    src.write_bytes(g.plistlib.dumps(g.fixture(words,'wrong-marker')))
    self.assertNotEqual(subprocess.run(cmd,capture_output=True).returncode,0)
    words[5]=3-n;src.write_bytes(g.plistlib.dumps(g.fixture(words,marker)))
    self.assertNotEqual(subprocess.run(cmd,capture_output=True).returncode,0)
 def test_10_2_markers_still_guarded(self):
  for label in ['ZC13','ZC18']:
   for mode,n in [('mapped',2),('conventional',1)]:
    w=[0]*1364;w[:3]=[10,2,10912];w[5]=n;w[6]=1
    src=self.path/'old.plist';cmd=[sys.executable,str(self.guard),str(src),'NONE','NONE',mode,label,'boundary']
    for marker,good in [('AirportItlwm-Tahoe-B1-'+label+'-PERF',True),('wrong',False)]:
     src.write_bytes(g.plistlib.dumps([{'KGP_ZC10':g.struct.pack('<1364Q',*w),'KGP_RX_EXPERIMENT':marker}]))
     p=subprocess.run(cmd,capture_output=True,text=True);self.assertEqual(p.returncode==0,good,p.stderr)
 def test_measurement_and_evaluation_byte_equivalent_to_frozen_zc22(self):
  def between(s,a,b):return s.split(a,1)[1].split(b,1)[0]
  for a,b in [('check_network() {','\nrun_test() {'),('run_test() {','\nstage="PREFLIGHT"'),
              ('stage="PREFLIGHT"','\nLOADED='),("from pathlib import Path\nimport csv, json, re, sys","\nPY\n")]:
   self.assertEqual(between(S,a,b),between(B,a,b),(a,b))
  self.assertEqual(S.split('stage="INITIAL_SNAPSHOT"\n',1)[1].split("if ! python3 -",1)[0],B.split('stage="INITIAL_SNAPSHOT"\n',1)[1].split("if ! python3 -",1)[0])
  self.assertEqual(S.split('stage="FINAL_SNAPSHOT"\n',1)[1],B.split('stage="FINAL_SNAPSHOT"\n',1)[1])
  for prefix in ['run_test ','sleep ','networkQuality ','ioreg ']:
   self.assertEqual([l.strip() for l in S.splitlines() if l.strip().startswith(prefix)],[l.strip() for l in B.splitlines() if l.strip().startswith(prefix)])
  self.assertIn('record_protocol initial',S);self.assertIn('record_protocol prepared',S)
  self.assertLess(S.index('record_protocol final'),S.index('> "$OUT/RUN_STATUS.txt"'))
  subprocess.run(['/bin/bash','-n',str(P)],check=True)
 def archive_fixture(self,rc=0,mutate=False):
  root=self.path/('archive-'+str(rc)+'-'+str(mutate));root.mkdir();out=root/'run';out.mkdir()
  invoked=root/'runner.sh';invoked.write_text(S)
  record=S.split('# Fixed traffic protocol unchanged',1)[1].split('\nfinish() {',1)[0]
  record='# Fixed traffic protocol unchanged'+record
  finish='finish() {'+S.split('finish() {',1)[1].split('\ntrap finish EXIT',1)[0]
  script='set -Eeuo pipefail\n'+record+'\n'+finish+'\n'+f'''
RUNNER_SOURCE={shlex.quote(str(invoked))}
OUT={shlex.quote(str(out))}
ARCHIVE="$OUT.zip"
LABEL=ZC25
KEXT=/fake/AirportItlwm.kext
UUID=FAKE-UUID
DEVICE=AX210
IFACE=en2
WIFI_IP=''
SERVER=192.0.2.10
PORT=5201
MODE=mapped
GAP_SECONDS=2
IOREG_SERVICE=ARPT
ROOT=/fake
BASE={shlex.quote(str(root))}
completed={42 if rc==0 else 0}
finished_ok={1 if rc==0 else 0}
online_status={'PASS' if rc==0 else 'NOT_RUN'}
stage=FIXTURE
trap finish EXIT
record_protocol initial
WIFI_IP=192.0.2.20
record_protocol prepared
'''
  if mutate:script+='printf "\\n# mutation\\n" >> "$RUNNER_SOURCE"\n'
  script+='exit '+str(rc)+'\n';h=root/'harness.sh';h.write_text(script)
  p=subprocess.run(['/bin/bash',str(h)],capture_output=True,text=True)
  return p,out
 def test_complete_archive_retains_exact_runner_settings_and_hashes(self):
  p,out=self.archive_fixture();self.assertEqual(p.returncode,0,p.stderr)
  with zipfile.ZipFile(str(out)+'.zip') as z:
   self.assertIsNone(z.testzip());prefix=out.name+'/'
   for name in ['EXECUTED_RUNNER.sh','EXECUTED_RUNNER_SHA256.txt','RUNNER_PROVENANCE.json','EFFECTIVE_SETTINGS.json','BENCHMARK_PROTOCOL.txt','SHA256SUMS.txt']:
    self.assertIn(prefix+name,z.namelist())
   content=z.read(prefix+'EXECUTED_RUNNER.sh');self.assertEqual(content,S.encode())
   proof=json.loads(z.read(prefix+'RUNNER_PROVENANCE.json'));self.assertEqual(proof['runner_sha256'],hashlib.sha256(content).hexdigest())
   self.assertEqual([v['phase'] for v in proof['records']],['initial','prepared','final'])
   settings=json.loads(z.read(prefix+'EFFECTIVE_SETTINGS.json'));self.assertEqual(settings['binding'],'192.0.2.20%en2')
   self.assertEqual(settings['rounds'],3);self.assertEqual(len(settings['tests_per_round']),14)
   self.assertEqual(settings['between_iperf_tests_seconds'],2)
   self.assertEqual(settings['tests_per_round'][:4],[{'name':'TCP_TX_P1','seconds':8,'streams':1,'reverse':False},{'name':'TCP_RX_P1','seconds':8,'streams':1,'reverse':True},{'name':'TCP_TX_P4','seconds':8,'streams':4,'reverse':False},{'name':'TCP_RX_P4','seconds':8,'streams':4,'reverse':True}])
   for line in z.read(prefix+'SHA256SUMS.txt').decode().splitlines():
    digest,name=line.split(None,1);self.assertEqual(digest,hashlib.sha256(z.read(prefix+name.removeprefix('./'))).hexdigest())
 def test_interrupted_archive_keeps_provenance_and_incomplete_status(self):
  p,out=self.archive_fixture(rc=130);self.assertEqual(p.returncode,130,p.stderr)
  self.assertIn('STATUS=INCOMPLETE_OR_ONLINE_FAILURE',(out/'RUN_STATUS.txt').read_text())
  self.assertTrue((out/'EXECUTED_RUNNER.sh').is_file());self.assertTrue(Path(str(out)+'.zip').is_file())
 def test_changed_runner_cannot_mark_complete(self):
  p,out=self.archive_fixture(mutate=True);self.assertNotEqual(p.returncode,0)
  self.assertIn('STATUS=INCOMPLETE_OR_ONLINE_FAILURE',(out/'RUN_STATUS.txt').read_text())
  proof=json.loads((out/'RUNNER_PROVENANCE.json').read_text());self.assertFalse(proof['copy_matches_executed_source_at_checks'])
  self.assertEqual((out/'EXECUTED_RUNNER.sh').read_text(),S)
if __name__=='__main__':unittest.main(verbosity=2)
