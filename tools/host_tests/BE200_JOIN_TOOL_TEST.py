#!/usr/bin/env python3
"""Execute the actual collector with read-only system calls replaced by fixtures.
No live capture/traffic/installation; production script remains unchanged.
"""
from pathlib import Path
import tempfile,subprocess,shutil,plistlib,sys,zipfile,hashlib
R=Path(__file__).resolve().parents[2];src=(R/'docs/private-be200/COLLECT_BE200_JOIN.sh').read_text()
with tempfile.TemporaryDirectory(prefix='be200-join-tools-')as tmp:
 t=Path(tmp);root=t/'resources';root.mkdir();out=t/'outputs';out.mkdir()
 for n in ['decode_be200.py','README_FIRST_TEST.txt','COLLECT_BE200_JOIN.sh']:shutil.copy2(R/'docs/private-be200'/n,root/n)
 (root/'BUILD_IDENTITY.txt').write_text('SIMULATED identity for offline collector execution; not hardware output\n')
 plist=t/'fixture.plist';plist.write_bytes(plistlib.dumps([{'IORegistryEntryName':'ARPT','KGP_BE200_CANDIDATE':'SIMULATED-P5','ItlwmMldAssocStep':7}]))
 def stub(name,text):
  p=t/name;p.write_text('#!/bin/bash\n'+text+'\n');p.chmod(0o755);return str(p)
 mocks={
 '/usr/sbin/ioreg':stub('ioreg','/bin/cat '+str(plist)),
 '/usr/sbin/networksetup':stub('networksetup','printf "SIMULATED network status\\n"'),
 '/usr/bin/kmutil':stub('kmutil','printf "SIMULATED loaded identity\\n"'),
 '/usr/sbin/sysctl':stub('sysctl','printf "SIMULATED boot\\n"'),
 '/usr/sbin/ipconfig':stub('ipconfig','printf "SIMULATED no DHCP\\n"; exit 1'),
 '/sbin/ifconfig':stub('ifconfig','printf "SIMULATED interface\\n"'),
 '/usr/bin/log':stub('log','printf "SIMULATED attempt log\\n"'),
 '/bin/sleep':stub('sleep','exit 0')}
 adapted=src.replace('ROOT="$(cd "$(dirname "$0")" && pwd)"','ROOT="'+str(root)+'"').replace('PARENT="$HOME/Desktop/AirportItlwm/EVIDENCE/BE200_JOIN"','PARENT="'+str(out)+'"')
 for command,path in mocks.items():adapted=adapted.replace(command,path)
 script=t/'run.sh';script.write_text(adapted)
 for inp in [b'\n',b'']:
  p=subprocess.run(['/bin/bash',str(script)],input=inp,capture_output=True,timeout=20);assert p.returncode==0,p.stderr.decode()
  archives=sorted(out.glob('*.zip'),key=lambda x:x.stat().st_mtime_ns);z=archives[-1]
  with zipfile.ZipFile(z)as f:
   assert f.testzip()is None;names=f.namelist();assert any(n.endswith('/BEFORE.plist')for n in names)and any(n.endswith('/AFTER.plist')for n in names)
   sums=next(n for n in names if n.endswith('/SHA256SUMS.txt'));prefix=sums.rsplit('/',1)[0]+'/'
   for l in f.read(sums).decode().splitlines():
    h,name=l.split('  ',1);assert hashlib.sha256(f.read(prefix+name.removeprefix('./'))).hexdigest()==h
   status=f.read(next(n for n in names if n.endswith('/COLLECTION_STATUS.txt'))).decode().strip()
   assert status==('user-finished'if inp else'input-timeout-or-interrupted')
  print('PASS SIMULATED collector',status,'before/after/logs/original resources/manifest/ZIP')
print('No real registry,network,kmutil,logstream,settings,traffic or EFI operation was executed.')
