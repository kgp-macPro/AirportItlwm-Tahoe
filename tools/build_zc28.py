#!/usr/bin/env python3
"""Hard clean-source gate and one pinned Release build. Never deploys a kext."""
from pathlib import Path
import subprocess,sys,os,hashlib,json,datetime,stat
R=Path(__file__).resolve().parents[1]
O=Path('/Users/kgp/Desktop/AirportItlwm/B1-ZC28-SERVICE')
G='/Applications/Xcode.app/Contents/Developer/usr/bin/git'
X='/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild'
C='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang'
SDK=Path('/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk')
BASE='48e9a31e68c328b5f19e5bdc81901e900d350738'
BRANCH='experiment/track-b1-zc28-service-handoff-20261020'
def git(*args):return subprocess.check_output([G,'--no-optional-locks','-C',str(R),*args]).decode().strip()
def sha(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def snapshot():
 assert not git('status','--porcelain=v1','--untracked-files=all'),'Uncommitted or untracked file'
 assert git('branch','--show-current')==BRANCH
 subprocess.run([G,'--no-optional-locks','-C',str(R),'merge-base','--is-ancestor',BASE,'HEAD'],check=True)
 entries=subprocess.check_output([G,'--no-optional-locks','-C',str(R),'ls-tree','-rz','HEAD']).split(b'\0')
 result={}
 for entry in entries:
  if not entry:continue
  meta,name=entry.split(b'\t',1);mode,kind,blob=meta.decode().split();name=name.decode();p=R/name
  assert kind=='blob',('Unmaterialized source input',name)
  if mode=='120000':
   assert p.is_symlink();data=os.readlink(p).encode();assert p.resolve().is_relative_to(R)
  else:
   assert p.is_file() and not p.is_symlink();data=p.read_bytes()
   assert bool(p.stat().st_mode&stat.S_IXUSR)==(mode=='100755'),name
  assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob,name
  result[name]={'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'git_blob':blob,'mode':mode}
 # Ignored files cannot hide effective inputs. After build Xcode's generated
 # workspace/user metadata may exist, but no compiler input may come from it.
 extras=git('ls-files','--others','--ignored','--exclude-standard').splitlines()
 assert all('/project.xcworkspace/' in p or '/xcuserdata/' in p or p.endswith('.DS_Store') for p in extras),extras
 return {'head':git('rev-parse','HEAD'),'tree':git('rev-parse','HEAD^{tree}'),'branch':BRANCH,'status':git('status','--porcelain=v1'),'ignored_non_source_metadata':extras,'files':result}
assert R.name=='TRACK-B1-ZC28-SERVICE' and (R/'.git').is_file()
COMMON=Path(git('rev-parse','--git-common-dir')).resolve()
assert COMMON==R.parent/'TRACK-B1-ZC8-PERF/.git'
assert not (COMMON/'objects/info/alternates').exists()
before=snapshot()
validation=json.loads((O/'evidence/VALIDATION_FINAL.json').read_text())
assert validation['status']=='PASS' and len(validation['checks'])==49
assert validation['head']==before['head'] and validation['tree']==before['tree']
assert set(validation['validated_input_sha256'])==set(before['files']), 'Incomplete validated source manifest'
for check in validation['checks']:
 assert check['exit_status']==0 and sha(Path(check['log']))==check['sha256']
for name,digest in validation['validated_input_sha256'].items():
 assert name in before['files'] and before['files'][name]['sha256']==digest,('changed validated input',name)
assert len(before['files'])>1200 and 'include/FwBinary.cpp' in before['files']
attempt=sys.argv[1] if len(sys.argv)>1 else 'final-01'
assert attempt.startswith('final-') and '/' not in attempt
B=O/'build'/attempt;assert not B.exists();B.mkdir(parents=True)
E=O/'evidence'
manifest=''.join(v['sha256']+'  '+p+'\n' for p,v in sorted(before['files'].items()))
(E/'SOURCE_SHA256SUMS.txt').write_text(manifest)
(E/'BUILD_INPUT.json').write_text(json.dumps(before,indent=2)+'\n')
tool={'xcode':subprocess.check_output([X,'-version'],stderr=subprocess.STDOUT).decode(),'clang':subprocess.check_output([C,'--version']).decode(),'sdk_settings':json.loads((SDK/'SDKSettings.json').read_text()),'sdk_settings_sha256':sha(SDK/'SDKSettings.json'),'sdk_root':str(SDK),'clang_sha256':sha(C),'xcodebuild_sha256':sha(X),'git_sha256':sha(G),'source_manifest_sha256':sha(E/'SOURCE_SHA256SUMS.txt')}
assert 'Xcode 26.5' in tool['xcode'] and SDK.exists()
(E/'TOOLCHAIN.json').write_text(json.dumps(tool,indent=2)+'\n')
cmd=[X,'-project','itlwm.xcodeproj','-target','AirportItlwm-Tahoe','-configuration','Release','ARCHS=x86_64','-jobs','2','SYMROOT='+str(B/'products'),'OBJROOT='+str(B/'intermediates'),'SHARED_PRECOMPS_DIR='+str(B/'precompiled'),'CLANG_MODULE_CACHE_PATH='+str(B/'module-cache'),'CODE_SIGNING_ALLOWED=NO','CODE_SIGNING_REQUIRED=NO','build']
record={'command':cmd,'cwd':str(R),'commit':before['head'],'tree':before['tree'],'start_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'attempt':attempt,'source_manifest_sha256':tool['source_manifest_sha256'],'git_common_dir':str(COMMON)}
(E/'BUILD_COMMAND.json').write_text(json.dumps(record,indent=2)+'\n')
# No injected compiler flags or alternative SDK from the invoking shell.
env={k:v for k,v in os.environ.items() if k not in ['SDKROOT','CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','MACOSX_DEPLOYMENT_TARGET']}
env['DEVELOPER_DIR']='/Applications/Xcode.app/Contents/Developer'
print('BUILD GATE PASS commit',before['head'],'tree',before['tree'],'files',len(before['files']),flush=True)
with (B/'BUILD.log').open('wb') as f:run=subprocess.run(cmd,cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT)
record['exit_status']=run.returncode;record['end_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();record['log']=str(B/'BUILD.log')
(E/'BUILD_RESULT.json').write_text(json.dumps(record,indent=2)+'\n')
after=snapshot()
assert after['head']==before['head'] and after['tree']==before['tree'] and after['files']==before['files']
(E/'BUILD_SOURCE_CONSISTENCY.json').write_text(json.dumps({'commit':after['head'],'tree':after['tree'],'status':after['status'],'files':len(after['files']),'source_sha256_unchanged':True,'git_blobs_verified':True,'ignored_non_source_metadata':after['ignored_non_source_metadata'],'source_manifest_sha256':tool['source_manifest_sha256'],'git_common_dir':str(COMMON)},indent=2)+'\n')
print('BUILD EXIT',run.returncode,'SOURCE/BUILD CORRESPONDENCE PASS',flush=True)
sys.exit(run.returncode)
