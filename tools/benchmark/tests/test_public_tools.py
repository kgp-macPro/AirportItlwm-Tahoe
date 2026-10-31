from pathlib import Path
import unittest, tempfile, subprocess, zipfile, json, hashlib, importlib.util, copy, plistlib, re

B=Path(__file__).resolve().parents[1];ROOT=B.parents[1]
spec=importlib.util.spec_from_file_location('installer',B/'install_tools.py');installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
spec=importlib.util.spec_from_file_location('profile',B/'runner_profiles/shared_profile.py');profile=importlib.util.module_from_spec(spec);spec.loader.exec_module(profile)

class PublicTools(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();cls.base=Path(cls.tmp.name)
  cls.build=json.loads(subprocess.check_output(['python3',str(B/'build_package.py'),'--output-dir',str(cls.base/'out')],text=True))
  cls.archive=cls.base/'out'/cls.build['filename']
  with zipfile.ZipFile(cls.archive)as z:z.extractall(cls.base/'extract')
  cls.package=cls.base/'extract'/'AirportItlwm-Shared-Benchmark-r11'
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_01_archive_complete_and_reproducible(self):
  with zipfile.ZipFile(self.archive)as z:
   self.assertIsNone(z.testzip());name='AirportItlwm-Shared-Benchmark-r11/SETUP_AirportItlwm_Benchmark.command'
   self.assertEqual((z.getinfo(name).external_attr>>16)&0o777,0o755)
   self.assertFalse(any('.kext' in n for n in z.namelist()))
  result=json.loads(subprocess.check_output(['python3',str(B/'build_package.py'),'--output-dir',str(self.base/'again')],text=True))
  self.assertEqual(result,self.build)
  for n in ['shared_profile.py','decode_be200.py','profiles.json']:
   self.assertTrue((self.package/'TOOLS/runner_profiles'/n).exists())
 def test_02_fresh_setup_only_tools(self):
  with tempfile.TemporaryDirectory()as t:
   home=Path(t);r=installer.install(self.package,home);root=home/'Desktop/AirportItlwm'
   self.assertTrue((root/'TESTS').is_dir());self.assertTrue((root/'BACKUPS').is_dir())
   self.assertEqual((root/'TOOLS/runner_profiles/profiles.json').read_bytes(),(B/'runner_profiles/profiles.json').read_bytes())
   self.assertFalse(r['kext_or_boot_changes']);self.assertEqual(r['saved_previous_files'],[])
 def test_03_beta_r7_custom_files_and_results_preserved(self):
  with tempfile.TemporaryDirectory()as t:
   home=Path(t);root=home/'Desktop/AirportItlwm';tools=root/'TOOLS';tools.mkdir(parents=True)
   runner=tools/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh';old=(B/'reference/AirportItlwm_3x14_PORTABLE_IP_PROMPT.r7-service.sh').read_bytes();runner.write_bytes(old)
   (root/'TESTS').mkdir();(root/'TESTS/original.zip').write_bytes(b'untouched result')
   (tools/'private.txt').write_text('preserve me');(tools/'runner_profiles').mkdir();(tools/'runner_profiles/profiles.json').write_text('custom older file')
   report=installer.install(self.package,home);backup=Path(report['backup'])
   self.assertEqual((backup/'TOOLS'/runner.name).read_bytes(),old)
   self.assertEqual((backup/'TOOLS/runner_profiles/profiles.json').read_text(),'custom older file')
   self.assertEqual((tools/'private.txt').read_text(),'preserve me')
   self.assertEqual((root/'TESTS/original.zip').read_bytes(),b'untouched result')
   self.assertEqual(installer.install(self.package,home)['saved_previous_files'],[])
 def test_04_modified_payload_rejected_before_write(self):
  import shutil
  with tempfile.TemporaryDirectory()as t:
   p=Path(t);shutil.copytree(self.package,p/'bad');(p/'bad/TOOLS/runner_profiles/profiles.json').write_text('tampered')
   with self.assertRaises(ValueError):installer.install(p/'bad',p/'home')
   self.assertFalse((p/'home/Desktop').exists())
 def test_05_symlink_or_parent_conflict_rejected(self):
  for symlink in [True,False]:
   with tempfile.TemporaryDirectory()as t:
    home=Path(t);root=home/'Desktop/AirportItlwm';tools=root/'TOOLS';tools.mkdir(parents=True)
    conflict=tools/'runner_profiles'
    if symlink:conflict.symlink_to(home/'outside')
    else:conflict.write_text('file must survive')
    (tools/'unrelated').write_text('intact')
    with self.assertRaises(ValueError):installer.install(self.package,home)
    self.assertEqual((tools/'unrelated').read_text(),'intact')
    if not symlink:self.assertEqual(conflict.read_text(),'file must survive')
 def test_06_source_guides_identical_no_private_install_paths(self):
  self.assertEqual((B/'README.md').read_bytes(),(ROOT/'docs/benchmark/README.md').read_bytes())
  for p in [B/'README.md',B/'START_COMMANDS.txt',B/'install_tools.py',B/'SETUP_AirportItlwm_Benchmark.command']:
   self.assertNotIn('/Users/kgp/',p.read_text())
 def test_07_public_profile_generic_binding_and_old_pins(self):
  data=plistlib.loads((B/'tests/fixtures/P7_BOUNDARY.plist').read_bytes());p=data[0];s=profile.profile('BE200-ALPHA2')
  p['KGP_BE200_CANDIDATE']=s['registry_candidate'];p['KGP_RX_EXPERIMENT']=s['marker']
  info={'CFBundleIdentifier':'com.zxystd.AirportItlwm','IOKitPersonalities':{'test':{'IOClass':'AirportItlwm'}}}
  for node,iface in [('CNVW','en0'),('WIFI','en17'),('NET0','en1')]:
   d=copy.deepcopy(data);d[0]['IORegistryEntryName']=node;i=d[0]['IORegistryEntryChildren'][0]['IORegistryEntryChildren'][0];i['BSD Name']=i['IORegistryEntryName']=iface
   r,_=profile.resolve(d,info,'be200','BE200-ALPHA2');self.assertEqual(r['interface_name'],iface)
   with self.assertRaises(ValueError):profile.resolve(d,info,'be200','BE200-ALPHA2',iface='awdl0')
  for old in ['BE200-ALPHA1','BE200-P8']:
   self.assertEqual(profile.profile(old),json.loads((B/'reference/r10/runner_profiles/profiles.json').read_text())[old])
  with self.assertRaises(ValueError):profile.resolve(data,info,'external','LAOBAMAC_53')
 def test_08_readme_order_collapsed_rows_and_anchors(self):
  text=(ROOT/'README.md').read_text();old=json.loads((B/'tests/fixtures/historical_rows.json').read_text())
  order=['## Project overview and qualified 1.0.0 release','## CI and the official release','## Lineage and documentation','## Current BE200 alpha testing campaign','## Optional reference comparisons: AirportItlwm-Tahoe 1.1.0 Beta','## Validation testing: AX201, AX210, AX211 and BE200','## Selected historical best results — original whole-test measurements','## Credits']
  self.assertEqual([text.index(x)for x in order],sorted(text.index(x)for x in order))
  for row in old:self.assertEqual(text.splitlines().count(row),1)
  result=text.split(order[-2],1)[1].split(order[-1],1)[0]
  self.assertIn('<details>\n<summary>',result);self.assertNotIn('<details open',result)
  inside=result.split('</summary>',1)[1].split('</details>',1)[0]
  self.assertTrue(all(row in inside for row in old));self.assertEqual(len([r for r in inside.splitlines()if r.startswith('| BE200 Alpha')]),32)
  self.assertIn('#### Compact run references for the selected rows',inside)
  self.assertIn('### AX201 validation commands',text);self.assertIn('### Submit beta results',text)
 def test_09_best_rows_same_measurement_not_means(self):
  data=json.loads((ROOT/'docs/benchmark/BE200_RESULT_SELECTION.json').read_text())
  self.assertEqual(len(data['selected_rows']),32);self.assertEqual(len(data['all_84_completed_test_summaries']),84)
  for row in data['selected_rows']:
   if row['protocol']=='networkQuality':continue
   candidates=[r for r in data['all_84_completed_test_summaries']if r['build']==row['build']and r['case']==row['case']]
   self.assertEqual(row,max(candidates,key=lambda r:r['receiver_mbps']))
   self.assertEqual(row['receiver_mbps'],row['receiver_end_original']['bits_per_second']/1e6)
   if row['protocol']=='UDP':
    self.assertEqual(row['loss_percent'],row['receiver_end_original']['lost_percent'])
    self.assertEqual(row['lost_datagrams'],row['receiver_end_original']['lost_packets'])
    self.assertEqual(row['total_datagrams'],row['receiver_end_original']['packets'])


 def test_10_ax_profiles_portable_binding_identity_and_modes(self):
  import struct, os, sys
  text=(B/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh').read_text()
  guard=text.split('check_snapshot() {',1)[1].split('\nexternal_identity_snapshot()',1)[0].split("<<'PY'\n",1)[1].split('\nPY\n',1)[0]
  info={'CFBundleIdentifier':'com.zxystd.AirportItlwm','IOKitPersonalities':{'test':{'IOClass':'AirportItlwm'}}}
  for label,device in [('AX201-ALPHA2',0x43f0),('AX210-ALPHA2',0x2725),('AX211-ALPHA2',0x51f0)]:
   spec=profile.profile(label);self.assertEqual(spec['uuid'],profile.profile('BE200-ALPHA2')['uuid']);self.assertEqual(spec['executable_sha256'],profile.profile('BE200-ALPHA2')['executable_sha256'])
   for mode,value in [('mapped',2),('conventional',1)]:
    with tempfile.TemporaryDirectory()as t:
     t=Path(t);d=plistlib.loads((B/'tests/fixtures/P7_BOUNDARY.plist').read_bytes());d[0]['device-id']=device.to_bytes(4,'little');d[0]['KGP_RX_EXPERIMENT']=spec['marker']
     d[0].pop('KGP_BE200_CANDIDATE');w=list(struct.unpack('<32Q',d[0]['KGP_ZC19']));w[5]=value;d[0]['KGP_ZC19']=struct.pack('<32Q',*w)
     self.assertEqual(profile.resolve(d,info,'kgp',label)[0]['pci']['device-id'],'0x'+format(device,'04x'))
     src=t/'snapshot.plist';src.write_bytes(plistlib.dumps(d));env=dict(os.environ,BENCHMARK_PROFILE='kgp',BENCHMARK_CANDIDATE_PROFILE_JSON=str(B/'runner_profiles/profiles.json'))
     result=subprocess.run([sys.executable,'-c',guard,str(src),'NONE','NONE',mode,label,'boundary'],env=env,capture_output=True,text=True)
     self.assertEqual(result.returncode,0,result.stderr)
     result=subprocess.run([sys.executable,'-c',guard,str(src),'NONE','NONE','conventional'if mode=='mapped'else'mapped',label,'boundary'],env=env,capture_output=True,text=True)
     self.assertNotEqual(result.returncode,0)
     d[0]['device-id']=(0x272b).to_bytes(4,'little')
     with self.assertRaises(ValueError):profile.resolve(d,info,'kgp',label)

 def test_11_ax_cli_profile_separation_and_portable_paths(self):
  import os
  runner=(B/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh').read_text()
  code=runner.split('[[ -f "$EXECUTABLE" ]]',1)[0]+"\nprintf 'SELECTED=%s|%s|%s|%s\\n' \"$PROFILE\" \"$LABEL\" \"$MODE\" \"$KEXT\"\n"
  for label in ['AX201-ALPHA2','AX210-ALPHA2','AX211-ALPHA2']:
   env={k:v for k,v in os.environ.items()if k not in ['CANDIDATE_LABEL','BENCHMARK_PROFILE','EXPECTED_MODE','CANDIDATE_KEXT','BE200_KEXT']}
   env.update(IPERF_SERVER='192.0.2.33',AIRPORTITLWM_ROOT='/portable/root')
   for mode in ['mapped','conventional']:
    r=subprocess.run(['/bin/bash','-c',code,'fixture',label,mode],env=env,capture_output=True,text=True)
    self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    self.assertIn('SELECTED=kgp|'+label+'|'+mode+'|/portable/root/CANDIDATES/ALPHA2/AirportItlwm.kext',r.stdout)
   for kind in ['external','be200']:
    r=subprocess.run(['/bin/bash','-c',code,'fixture',label,'mapped'],env=dict(env,BENCHMARK_PROFILE=kind),capture_output=True,text=True)
    self.assertNotEqual(r.returncode,0)

if __name__=='__main__':unittest.main(verbosity=2)
