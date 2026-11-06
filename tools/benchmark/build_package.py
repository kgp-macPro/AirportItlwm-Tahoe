#!/usr/bin/env python3
"""Build the complete flat-layout r13 tool ZIP; no network or driver operation."""
from pathlib import Path
import argparse,hashlib,json,tempfile,shutil,zipfile
B=Path(__file__).resolve().parent
NAME='AirportItlwm-Tahoe-Benchmark-Tools-r13'
REV='r13-release-2.0.0'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(out):
 out.mkdir(parents=True,exist_ok=True)
 runners=sorted(B.glob('AirportItlwm_3x20*PORTABLE_IP_PROMPT.sh'));assert len(runners)==5
 identity=dict(revision=REV,runners={p.name:sha(p)for p in runners},resources={str(p.relative_to(B)):sha(p)for p in (B/'runner_profiles').glob('*')if p.is_file()},historical_3x14_sha256=sha(B/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh'),binary_identity=json.loads((B/'BUILD_IDENTITY.json').read_text()),scope='Offline packaging only. Strict original and release identity profiles retained; AX diagnosis optional on BE200.')
 (B/'RUNNER_IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
 names=[p.name for p in runners]+['AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh','AirportItlwm_IPERF3_WIRED_SERVER.sh','AX_HE_DIAG_CAPTURE.py','VERIFY_TOOLS.py','BUILD_IDENTITY.json','RUNNER_IDENTITY.json','README.md','START_COMMANDS.txt','LICENSE','build_package.py','runner_profiles','evidence/PACKAGE_VALIDATION.json']
 with tempfile.TemporaryDirectory(prefix='tahoe-r13-')as tmp:
  root=Path(tmp)/NAME;root.mkdir()
  for name in names:
   src=B/name;dst=root/name;dst.parent.mkdir(parents=True,exist_ok=True)
   if src.is_dir():shutil.copytree(src,dst,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
   else:shutil.copy2(src,dst)
  files=sorted(p for p in root.rglob('*')if p.is_file());manifest={str(p.relative_to(root)):dict(sha256=sha(p),bytes=p.stat().st_size)for p in files}
  (root/'BUNDLE_MANIFEST.json').write_text(json.dumps(dict(revision=REV,files=manifest),indent=2)+'\n')
  (root/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+str(p.relative_to(root))+'\n'for p in sorted(root.rglob('*'))if p.is_file()))
  archive=out/(NAME+'.zip')
  with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
   for p in sorted(root.rglob('*')):
    if not p.is_file():continue
    info=zipfile.ZipInfo(str(p.relative_to(root.parent)),(1980,1,1,0,0,0));info.create_system=3;info.external_attr=(0o100755 if p.suffix in ['.sh','.py']else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,p.read_bytes())
  with zipfile.ZipFile(archive)as z:assert z.testzip()is None
  result=dict(name=archive.name,sha256=sha(archive),bytes=archive.stat().st_size,revision=REV);print(json.dumps(result,indent=2));return result
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output-dir',type=Path,default=B/'releases');build(ap.parse_args().output_dir)
