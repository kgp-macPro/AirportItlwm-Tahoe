#!/usr/bin/env python3
"""Deterministic tool-only ZIP. Never reads, builds or packages a kext."""
from pathlib import Path
import argparse, hashlib, json, tempfile, shutil, zipfile

B=Path(__file__).resolve().parent
REV='shared-2026-10-31-r11-public-be200'
NAME='AirportItlwm-Shared-Benchmark-r11'
RUNNER='AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh'

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def build(out):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    profiles=json.loads((B/'runner_profiles/profiles.json').read_text())
    identity={'revision':REV,'protocol':'AIRPORTITLWM_3X14_NO_PROFILER_V1','runner_sha256':sha(B/RUNNER),
              'resource_sha256':{p.name:sha(p)for p in sorted((B/'runner_profiles').glob('*'))if p.is_file()},
              'server_sha256':sha(B/'AirportItlwm_IPERF3_WIRED_SERVER.sh'),
              'frozen_references':{str(p.relative_to(B)):sha(p)for p in sorted((B/'reference').rglob('*'))if p.is_file()and '__pycache__'not in p.parts},
              'public_alpha2_profile':profiles['BE200-ALPHA2'],
              'scope':'Tool-only; unchanged traffic protocol, no driver/binary/EFI/root-patch operations.'}
    (B/'RUNNER_IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
    with tempfile.TemporaryDirectory(prefix='airport-tool-package-')as tmp:
        root=Path(tmp)/NAME;tools=root/'TOOLS';tools.mkdir(parents=True)
        skip={'releases','__pycache__','.DS_Store'}
        root_only={'install_tools.py','SETUP_AirportItlwm_Benchmark.command','build_package.py'}
        for p in sorted(B.rglob('*')):
            rel=p.relative_to(B)
            if not p.is_file()or any(x in skip for x in rel.parts)or rel.name.endswith('.pyc')or (len(rel.parts)==1 and p.name in root_only):continue
            dest=tools/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
        for name in ['install_tools.py','SETUP_AirportItlwm_Benchmark.command']:shutil.copy2(B/name,root/name)
        (root/'SETUP_AirportItlwm_Benchmark.command').chmod(0o755)
        (root/'README_START_HERE.txt').write_text('AIRPORTITLWM SHARED BENCHMARK r11 — TOOLS ONLY\n\nExtract this entire folder in Downloads. Paste:\n\n/bin/bash "$HOME/Downloads/'+NAME+'/SETUP_AirportItlwm_Benchmark.command"\n\nIt installs Desktop tools and opens TOOLS/README.md. Differing old files are backed up; TESTS is preserved. No kext, EFI, root patch or network setting is changed. No test starts automatically.\n\nThe old beta setup still supplies r7: run this setup afterwards. Read TOOLS/README.md for prerequisites, all candidate commands, wired-server binding and return instructions. Python3.9+, existing iperf3 and Command Line Tools are required.\n')
        files=sorted(p for p in root.rglob('*')if p.is_file())
        (root/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(root)}\n'for p in files))
        archive=out/(NAME+'.zip')
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
            for p in sorted(root.rglob('*')):
                if not p.is_file():continue
                info=zipfile.ZipInfo(str(p.relative_to(root.parent)),date_time=(1980,1,1,0,0,0));info.create_system=3
                mode=0o755 if p.suffix=='.command'else 0o644
                info.external_attr=(0o100000|mode)<<16;info.compress_type=zipfile.ZIP_DEFLATED
                z.writestr(info,p.read_bytes())
        checksum=sha(archive);archive.with_suffix('.zip.sha256').write_text(checksum+'  '+archive.name+'\n')
        with zipfile.ZipFile(archive)as z:assert z.testzip()is None
        result={'revision':REV,'filename':archive.name,'bytes':archive.stat().st_size,'sha256':checksum,'runner_sha256':identity['runner_sha256']}
        print(json.dumps(result,indent=2));return result

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output-dir',type=Path,default=B/'releases');build(ap.parse_args().output_dir)
