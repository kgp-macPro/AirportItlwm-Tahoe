#!/usr/bin/env python3
"""Clean committed private Release build, never installs or publishes."""
from pathlib import Path
import subprocess,hashlib,json,sys,os,datetime
R=Path(__file__).resolve().parents[1];E=Path(sys.argv[1]);V=Path(sys.argv[2]);attempt=sys.argv[3]if len(sys.argv)>3 else'final-01'
G='/Applications/Xcode.app/Contents/Developer/usr/bin/git';X='/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild'
def git(*a):return subprocess.check_output([G,*a],cwd=R,text=True).strip()
assert R.name=='TRACK-BE200-BZ-TRANSPORT-20261029' and git('branch','--show-current')=='experiment/be200-bz-transport-20261029'
assert not git('status','--porcelain=v1','--untracked-files=all'),'Uncommitted sources'
assert git('merge-base','483940988c502379fb6d3dcde885dca83f00b7aa','HEAD')=='483940988c502379fb6d3dcde885dca83f00b7aa'
val=json.loads((V/'RESULT.json').read_text());assert val['status']=='PASS' and val['head']==git('rev-parse','HEAD')
for name,h in val['validated_inputs'].items():assert hashlib.sha256((R/name).read_bytes()).hexdigest()==h,('Changed validated input',name)
assert set(git('ls-files').splitlines())==set(val['validated_inputs'])
B=E/'artifacts/build'/attempt;B.mkdir(parents=True,exist_ok=False)
record={'head':git('rev-parse','HEAD'),'tree':git('rev-parse','HEAD^{tree}'),'parents':git('show','-s','--format=%P'),'branch':git('branch','--show-current'),'source_sha256':val['validated_inputs'],'validation':str(V/'RESULT.json'),'attempt':attempt,'start_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'xcode':subprocess.check_output([X,'-version'],text=True)}
cmd=[X,'-project','itlwm.xcodeproj','-target','AirportItlwm-Tahoe','-configuration','Release','ARCHS=x86_64','-jobs','2','MODULE_VERSION=1.0.0','SYMROOT='+str(B/'products'),'OBJROOT='+str(B/'intermediates'),'SHARED_PRECOMPS_DIR='+str(B/'precompiled'),'CLANG_MODULE_CACHE_PATH='+str(B/'module-cache'),'CODE_SIGNING_ALLOWED=NO','CODE_SIGNING_REQUIRED=NO','build'];record['command']=cmd
(E/'logs'/('BUILD_'+attempt+'.json')).write_text(json.dumps(record,indent=2)+'\n')
env={k:v for k,v in os.environ.items()if k not in ['SDKROOT','CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','MACOSX_DEPLOYMENT_TARGET']};env['DEVELOPER_DIR']='/Applications/Xcode.app/Contents/Developer'
with (E/'logs'/('BUILD_'+attempt+'.log')).open('w')as f:r=subprocess.run(cmd,cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT)
record['exit']=r.returncode;record['end_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();record['status_after']=git('status','--porcelain=v1')
assert not record['status_after']
for n,h in val['validated_inputs'].items():assert hashlib.sha256((R/n).read_bytes()).hexdigest()==h,('Build mutated source',n)
(E/'logs'/('BUILD_'+attempt+'.json')).write_text(json.dumps(record,indent=2)+'\n');print(attempt,'EXIT',r.returncode,'HEAD',record['head'],'TREE',record['tree']);sys.exit(r.returncode)
