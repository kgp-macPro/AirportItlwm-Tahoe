#!/usr/bin/env python3
"""Native packaging/setup tests. No kext loading, traffic, EFI, GUI or reboot.
Small synthetic files test error paths in literal A/B functions; the final
real-package test checks actual bytes/permissions with native unzip and Bash.
"""
from pathlib import Path
import base64,copy,hashlib,io,json,os,plistlib,re,shlex,shutil,stat,struct,subprocess,tempfile,unittest,uuid,zipfile
from unittest import mock
R=Path(os.environ['AIRPORT_BETA_REVIEW'])
def module(p):
 s=p.read_text().split("<<'AIRPORT_BETA_PY'\n",1)[1].rsplit('\nAIRPORT_BETA_PY',1)[0];d={'__name__':'local_packaging_test','__file__':str(p)};exec(compile(s,str(p),'exec'),d);return d
A0=module(R/'A_PACK_BETA.command');B0=module(R/'SETUP_AirportItlwm_Beta.command')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def macho(u):return struct.pack('<IiiIIIII',0xfeedfacf,0x01000007,3,11,1,24,0,0)+struct.pack('<II',0x1b,24)+uuid.UUID(u).bytes
class PackageTests(unittest.TestCase):
 def setUp(self):
  self.t=tempfile.TemporaryDirectory(prefix='airport-beta-test-');self.addCleanup(self.t.cleanup);self.p=Path(self.t.name);self.home=self.p/'producer';self.desktop=self.home/'Desktop';self.desktop.mkdir(parents=True);self.dest=self.p/'tester desktop';self.dest.mkdir();self.enc=self.p/'enclosures';self.enc.mkdir();self.a=dict(A0);self.b=dict(B0)
  # Functions keep their original globals: copy module per fixture, then set
  # only input constants. No queue/driver/packet implementation is modeled.
  self.a=module(R/'A_PACK_BETA.command');self.b=module(R/'SETUP_AirportItlwm_Beta.command')
  info=plistlib.dumps({'CFBundleIdentifier':'com.zxystd.AirportItlwm','CFBundleVersion':'1.0.0'});info_sha=hashlib.sha256(info).hexdigest();spec=[]
  for label,rel,u,_ in A0['SPECS']:
   p=self.desktop/rel/'Contents';(p/'MacOS').mkdir(parents=True);(p/'Info.plist').write_bytes(info);x=p/'MacOS/AirportItlwm';x.write_bytes(macho(u));x.chmod(0o755);spec.append((label,rel,u,sha(x)))
  self.a['SPECS']=spec;self.b['SPECS']=spec
  for x in [self.a,self.b]:x['KGP_INFO_SHA']=info_sha;x['EXTERNAL_INFO_SHA']=info_sha
  runner=self.desktop/'AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh';runner.parent.mkdir(parents=True);runner.write_bytes(b'fixture unchanged runner\n');runner.chmod(0o755)
  self.a['RUNNER_SHA']=sha(runner);self.b['RUNNER_SHA']=sha(runner)
  refs=[]
  for name,_ in A0['REFERENCES']:
   p=self.desktop/'AirportItlwm/TESTS'/name;p.parent.mkdir(exist_ok=True)
   with zipfile.ZipFile(p,'w') as z:z.writestr('synthetic_reference.txt',name)
   refs.append((name,sha(p)))
  self.a['REFERENCES']=refs;self.b['REFERENCES']=refs
  enc={}
  for n in A0['ENCLOSURE_HASHES']:
   rel=n.removeprefix('AirportItlwm/').removeprefix('SOURCES/');p=self.enc/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(('SYNTHETIC FIXTURE ONLY '+n).encode());enc[n]=sha(p)
  self.a['ENCLOSURE_HASHES']=enc;self.b['ENCLOSURE_HASHES']=enc
  self.resources=copy.deepcopy(A0['RESOURCES']);self.pkg=self.p/A0['PACKAGE_NAME']
 def build(self):return self.a['build'](home=self.home,resources=self.resources,enclosure_dir=self.enc,output=self.pkg)
 def extract(self):
  self.build();p=self.p/'extracted';p.mkdir();self.b['verify_and_extract'](self.pkg,p);return p
 def rewrite(self,fn):
  values={};infos={}
  with zipfile.ZipFile(self.pkg) as z:
   for i in z.infolist():values[i.filename]=z.read(i.filename);infos[i.filename]=copy.copy(i)
  fn(values,infos)
  p=self.p/'modified.zip'
  with zipfile.ZipFile(p,'w') as z:
   for n,v in values.items():z.writestr(infos.get(n,zipfile.ZipInfo(n)),v)
  return p
 def alter_rehash(self,n,data,mode=None):
  root=A0['PACKAGE_ROOT']+'/'
  def f(v,i):
   v[root+n]=data;m=json.loads(v[root+'PACKAGE_MANIFEST.json']);m['files'][n]['sha256']=hashlib.sha256(data).hexdigest();m['files'][n]['bytes']=len(data)
   if mode is not None:i[root+n].external_attr=(stat.S_IFREG|mode)<<16;m['files'][n]['mode']=mode
   v[root+'PACKAGE_MANIFEST.json']=json.dumps(m).encode()
  return self.rewrite(f)
 def reject(self,p):
  before=self.b['tree_signature'](self.dest)
  with self.assertRaises((ValueError,OSError,KeyError)):self.b['setup'](p,self.dest)
  self.assertEqual(self.b['tree_signature'](self.dest),before)
 def test_01_shell_and_python_compile(self):
  for n in ['A_PACK_BETA.command','SETUP_AirportItlwm_Beta.command']:
   subprocess.run(['/bin/bash','-n',str(R/n)],check=True);self.assertTrue((R/n).stat().st_mode&0o111)
 def test_02_literal_A_zip_B(self):
  p,m,old=self.build();self.assertIsNone(old);r=self.b['setup'](p,self.dest);self.assertIsNone(r['backup'])
  self.assertEqual((self.dest/'AirportItlwm/README_TESTS.txt').read_bytes(),(R/'README_TESTS.txt').read_bytes());self.assertTrue((self.dest/'Laobamac/53/AirportItlwm.kext/Contents/MacOS/AirportItlwm').stat().st_mode&0o111)
 def test_03_setup_without_original_zip(self):
  p=self.extract();self.pkg.unlink();self.b['setup'](p,self.dest);self.assertTrue((self.dest/'AirportItlwm/SOURCES').is_dir())
 def test_04_idempotence(self):
  self.build();self.b['setup'](self.pkg,self.dest);p=self.dest/'AirportItlwm/README_TESTS.txt';stamp=p.stat().st_mtime_ns;r=self.b['setup'](self.pkg,self.dest);self.assertFalse(r['installed_or_updated']);self.assertIsNone(r['backup']);self.assertEqual(p.stat().st_mtime_ns,stamp)
 def test_05_tests_and_unmanaged_files_preserved(self):
  p=self.dest/'AirportItlwm/TESTS/my_unfavorable_original.zip';p.parent.mkdir(parents=True);p.write_bytes(b'KEEP');u=self.dest/'notes.txt';u.write_bytes(b'KEEP TOO');self.build();self.b['setup'](self.pkg,self.dest);self.assertEqual(p.read_bytes(),b'KEEP');self.assertEqual(u.read_bytes(),b'KEEP TOO')
 def test_06_differing_bundle_and_tool_backed_up_whole(self):
  p=self.dest/self.a['SPECS'][0][1];p.mkdir(parents=True);(p/'old.marker').write_bytes(b'OLD');t=self.dest/'AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh';t.parent.mkdir(parents=True);t.write_bytes(b'OLD SCRIPT');self.build();r=self.b['setup'](self.pkg,self.dest);b=Path(r['backup']);self.assertEqual((b/self.a['SPECS'][0][1]/'old.marker').read_bytes(),b'OLD');self.assertEqual((b/t.relative_to(self.dest)).read_bytes(),b'OLD SCRIPT');self.assertFalse((p/'old.marker').exists())
 def test_07_readonly_managed_file_backed_up(self):
  p=self.dest/'AirportItlwm/README_TESTS.txt';p.parent.mkdir();p.write_bytes(b'old');p.chmod(0o444);self.build();r=self.b['setup'](self.pkg,self.dest);self.assertEqual((Path(r['backup'])/'AirportItlwm/README_TESTS.txt').read_bytes(),b'old')
 def test_08_destination_symlink_rejected(self):
  p=self.dest/'AirportItlwm';p.symlink_to(self.home,target_is_directory=True);self.build()
  with self.assertRaises(ValueError):self.b['setup'](self.pkg,self.dest)
  self.assertTrue(p.is_symlink())
 def test_09_duplicate_zip_members(self):
  self.build();p=self.p/'dup.zip';shutil.copy2(self.pkg,p)
  with zipfile.ZipFile(p,'a') as z:z.writestr(A0['PACKAGE_ROOT']+'/README_START_HERE.txt',b'dup')
  self.reject(p)
 def test_10_path_traversal_and_absolute(self):
  self.build()
  for name in ['../escape','/absolute','bad\\name','other-top/file']:
   def f(v,i):v[name]=b'x'
   self.reject(self.rewrite(f))
 def test_11_symlink_zip_member(self):
  self.build();name=A0['PACKAGE_ROOT']+'/README_START_HERE.txt'
  def f(v,i):i[name].external_attr=(stat.S_IFLNK|0o777)<<16
  self.reject(self.rewrite(f))
 def test_12_binary_changed_even_with_rehashed_manifest(self):
  self.build();self.reject(self.alter_rehash(self.a['SPECS'][0][1]+'/Contents/MacOS/AirportItlwm',macho('FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF')))
 def test_13_info_changed_even_with_rehashed_manifest(self):
  self.build();self.reject(self.alter_rehash(self.a['SPECS'][0][1]+'/Contents/Info.plist',b'wrong'))
 def test_14_runner_changed_even_with_rehashed_manifest(self):
  self.build();self.reject(self.alter_rehash('AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh',b'wrong'))
 def test_15_source_changed_even_with_rehashed_manifest(self):
  self.build();self.reject(self.alter_rehash(next(iter(self.a['ENCLOSURE_HASHES'])),b'wrong'))
 def test_16_missing_source(self):
  self.build();name=A0['PACKAGE_ROOT']+'/'+next(iter(self.a['ENCLOSURE_HASHES']))
  def f(v,i):del v[name]
  self.reject(self.rewrite(f))
 def test_17_reference_changed(self):
  self.build();self.reject(self.alter_rehash('AirportItlwm/REFERENCE_RESULTS/'+self.a['REFERENCES'][0][0],b'wrong'))
 def test_18_missing_or_extra_reference(self):
  self.build();root=A0['PACKAGE_ROOT']+'/'
  for name in ['AirportItlwm/REFERENCE_RESULTS/extra.zip','AirportItlwm/REFERENCE_RESULTS/'+self.a['REFERENCES'][0][0]]:
   def f(v,i):
    if root+name in v:del v[root+name]
    else:v[root+name]=b'extra'
   self.reject(self.rewrite(f))
 def test_19_unknown_extracted_file(self):
  p=self.extract();(p/'unknown').write_bytes(b'bad');self.reject(p)
 def test_20_finder_metadata_ignored(self):
  p=self.extract();(p/'.DS_Store').write_bytes(b'Finder');self.b['setup'](p,self.dest);self.assertFalse((self.dest/'.DS_Store').exists())
 def test_21_executable_permission_required(self):
  self.build();n=self.a['SPECS'][0][1]+'/Contents/MacOS/AirportItlwm';data=(self.desktop/n).read_bytes();self.reject(self.alter_rehash(n,data,0o644))
 def test_22_copy_error_before_mutation(self):
  self.build()
  with mock.patch.object(shutil,'copy2',side_effect=OSError('injected copy failure')):self.reject(self.pkg)
 def rollback(self,error):
  self.build();old=self.dest/self.a['SPECS'][0][1];old.mkdir(parents=True);(old/'old').write_bytes(b'KEEP');before=self.b['tree_signature'](self.dest);real=os.replace;failed=False
  def replace(src,dst):
   nonlocal failed
   if not failed and '.airport_beta_stage_' in str(src) and self.a['SPECS'][1][1] in str(src):failed=True;raise error
   return real(src,dst)
  with mock.patch.object(os,'replace',side_effect=replace):
   with self.assertRaises(type(error)):self.b['setup'](self.pkg,self.dest)
  self.assertTrue(failed);self.assertEqual(self.b['tree_signature'](self.dest),before)
 def test_23_mid_replacement_failure_rolls_back(self):self.rollback(OSError('injected rename failure'))
 def test_24_keyboard_interrupt_rolls_back(self):self.rollback(KeyboardInterrupt())
 def test_25_manifest_wrong_type(self):
  self.build();root=A0['PACKAGE_ROOT']+'/'
  def f(v,i):v[root+'PACKAGE_MANIFEST.json']=b'[]'
  self.reject(self.rewrite(f))
 def test_26_A_missing_binary_no_output(self):
  (self.desktop/self.a['SPECS'][0][1]/'Contents/MacOS/AirportItlwm').unlink()
  with self.assertRaises(ValueError):self.build()
  self.assertFalse(self.pkg.exists())
 def test_27_A_wrong_runner_no_output(self):
  (self.desktop/'AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh').write_bytes(b'bad')
  with self.assertRaises(ValueError):self.build()
  self.assertFalse(self.pkg.exists())
 def test_28_A_missing_source_no_output(self):
  next(p for p in self.enc.rglob('*') if p.is_file()).unlink()
  with self.assertRaises(ValueError):self.build()
  self.assertFalse(self.pkg.exists())
 def test_29_A_existing_package_backup(self):
  self.build();old=self.pkg.read_bytes();_,_,backup=self.build();self.assertEqual(backup.read_bytes(),old)
 def test_30_open_instructions_target_only(self):
  with mock.patch.dict(os.environ,{},clear=True),mock.patch.object(subprocess,'run',return_value=type('Result',(),{'returncode':0})()) as run:
   self.b['open_instructions']('/tmp/quick start.txt');run.assert_called_once_with(['/usr/bin/open','-t','/tmp/quick start.txt'],check=False)
 def test_31_real_macho_parser_bounds(self):
  for d in [b'',bytes(32),macho(self.a['SPECS'][0][2])[:-1]]:
   with self.assertRaises((ValueError,struct.error)):self.b['macho_uuid'](d)
 def test_32_all_documented_commands_real_bash_frontend(self):
  text=(R/'README_TESTS.txt').read_text();calls=[shlex.split(l)[2:] for l in text.splitlines() if l.startswith('/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/')];unique=[]
  for c in calls:
   if c not in unique:unique.append(c)
  self.assertEqual(len(unique),12)
  runner=Path('/Users/kgp/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh').read_text();prefix=runner.split('[[ -f "$EXECUTABLE" ]]',1)[0];code=prefix+"\nprintf '%s|%s|%s|%s|%s' \"$LABEL\" \"$MODE\" \"$KEXT\" \"$IFACE\" \"$DEVICE\"\n"
  for c in unique:
   env={k:v for k,v in os.environ.items() if k not in ['BENCHMARK_PROFILE','EXPECTED_MODE','CANDIDATE_LABEL','AIRPORTITLWM_ROOT','LAOBAMAC_KEXT','EXTERNAL_SOURCE_REF','LAOBAMAC_SOURCE_REF','EXTERNAL_DECLARED_MODE']};env.update(HOME=str(self.home),IPERF_SERVER='192.0.2.33',WIFI_IFACE='en9',DEVICE_LABEL='AX211')
   r=subprocess.run(['/bin/bash','-c',code,'documented-command',*c],env=env,capture_output=True,text=True);self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertIn('|en9|AX211',r.stdout)
   if c[0]=='LAOBAMAC':self.assertIn('|unverified|'+str(self.desktop/'Laobamac/53/AirportItlwm.kext'),r.stdout)
   else:self.assertIn(c[0]+'|'+c[1]+'|',r.stdout)
 def test_33_repo_and_embedded_guide_identical(self):
  b=base64.b64decode(A0['RESOURCES']['AirportItlwm/README_TESTS.txt']['base64']);self.assertEqual(b,(R/'README_TESTS.txt').read_bytes());self.assertIn(b'QUICK START',b);self.assertIn(b'ROLLBACK',b);self.assertIn(b'Python 3.9',b)
 def test_34_real_package_native_unzip_and_bash(self):
  package=R/A0['PACKAGE_NAME'];self.assertTrue(package.is_file());home=self.p/'ordinary tester home';downloads=home/'Downloads';downloads.mkdir(parents=True);(home/'Desktop/AirportItlwm/TESTS').mkdir(parents=True);sentinel=home/'Desktop/AirportItlwm/TESTS/keep-old-result.zip';sentinel.write_bytes(b'PRESERVE ACTUAL TESTS')
  subprocess.run(['/usr/bin/unzip','-q',str(package),'-d',str(downloads)],check=True)
  setup=downloads/A0['PACKAGE_ROOT']/'SETUP_AirportItlwm_Beta.command';self.assertTrue(setup.stat().st_mode&0o111)
  r=subprocess.run(['/bin/bash',str(setup)],env={**os.environ,'HOME':str(home),'AIRPORT_BETA_TEST_NO_OPEN':'1'},capture_output=True,text=True);self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertIn('OPEN QUICK START:',r.stdout);self.assertEqual(sentinel.read_bytes(),b'PRESERVE ACTUAL TESTS')
  for label,rel,u,h in B0['SPECS']:self.assertEqual(sha(home/'Desktop'/rel/'Contents/MacOS/AirportItlwm'),h)
  self.assertEqual(sha(home/'Desktop'/B0['SINGLES'][0]),B0['RUNNER_SHA']);self.assertEqual((home/'Desktop/AirportItlwm/README_TESTS.txt').read_bytes(),(R/'README_TESTS.txt').read_bytes())
  # No original ZIP in Downloads: extraction-alone workflow was actually used.
  self.assertFalse((downloads/A0['PACKAGE_NAME']).exists())
if __name__=='__main__':unittest.main(verbosity=2)
