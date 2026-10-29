#!/usr/bin/env python3
"""Actual binding resolver/collector with ALL system operations stubbed offline."""
from pathlib import Path
import json,tempfile,subprocess,shutil,plistlib,sys,zipfile,hashlib,os,copy,importlib.util
R=Path(__file__).resolve().parents[2];src=(R/'docs/private-be200/COLLECT_BE200_JOIN.sh').read_text()
spec=importlib.util.spec_from_file_location('resolver',R/'docs/private-be200/resolve_be200_interface.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
hardware=plistlib.loads((R/'tools/host_tests/fixtures/BE200_P5_BINDING.plist').read_bytes())
actual,_=m.resolve(hardware,'en2','ARPT',expected='BE200-GL92-PRIVATE-P5');assert actual['pci']['device-id']=='0x272b'and actual['driver_class']=='AirportItlwm'
def variant(iface,node):
 data=copy.deepcopy(hardware);p=data[0];p['IORegistryEntryName']=node;p['KGP_BE200_CANDIDATE']=m.EXPECTED;p['KGP_RX_EXPERIMENT']='AirportItlwm-Tahoe-'+m.EXPECTED
 d=next(n for n in p['IORegistryEntryChildren']if n.get('IOObjectClass')=='AirportItlwm');wi=next(n for n in d['IORegistryEntryChildren']if n.get('IOObjectClass')=='AirportItlwmInterface');wi['BSD Name']=wi['IORegistryEntryName']=iface
 return data
for iface,node in [('en0','CNVW'),('en1','WIFI'),('en2','ARPT'),('en17','NETW')]:
 data=variant(iface,node);r,_=m.resolve(data,iface,node);assert r['wifi_iface']==iface and r['provider_name']==node
 # Resolver itself supports discovery; targeted collector supplies checked names.
 assert m.resolve(data)[0]['wifi_iface']==iface
 print('PASS SIMULATED alternate binding',iface,node)
bad=[];base=variant('en1','WIFI')
bad.append((base,'en0','WIFI',None));bad.append((base,'en1','ARPT',None));bad.append(([], '', '',None));bad.append((base+copy.deepcopy(base),'','',None))
for kind in ['pci','marker','awdl-only','missing-bsd','missing-id']:
 data=variant('en1','WIFI');p=data[0];d=next(n for n in p['IORegistryEntryChildren']if n.get('IOObjectClass')=='AirportItlwm');wi=next(n for n in d['IORegistryEntryChildren']if n.get('IOObjectClass')=='AirportItlwmInterface')
 if kind=='pci':p['device-id']=bytes.fromhex('23270000')
 if kind=='marker':p['KGP_BE200_CANDIDATE']='wrong'
 if kind=='awdl-only':wi['IOObjectClass']='IO80211P2PInterface';wi['BSD Name']='awdl0'
 if kind=='missing-bsd':wi.pop('BSD Name')
 if kind=='missing-id':wi.pop('IORegistryEntryID')
 bad.append((data,'','',None))
prior=m.resolve(base)[0];changed=copy.deepcopy(base);changed[0]['IORegistryEntryID']+=1;bad.append((changed,'en1','WIFI',prior))
for data,iface,node,prev in bad:
 try:m.resolve(data,iface,node,previous=prev)
 except ValueError:pass
 else:raise AssertionError('Unsafe/ambiguous mapping accepted')
print('PASS',len(bad),'negative resolver/lifetime cases; saved P5 actual binding verified separately')
with tempfile.TemporaryDirectory(prefix='be200-join-tools-')as tmp:
 t=Path(tmp);root=t/'resources';root.mkdir();out=t/'outputs';out.mkdir();plist=t/'fixture.plist'
 for n in ['decode_be200.py','resolve_be200_interface.py','README_FIRST_TEST.txt','COLLECT_BE200_JOIN.sh']:shutil.copy2(R/'docs/private-be200'/n,root/n)
 (root/'BUILD_IDENTITY.txt').write_text('SIMULATED identity for offline collector execution; not hardware output\n')
 def stub(name,text):
  p=t/name;p.write_text('#!/bin/bash\n'+text+'\n');p.chmod(0o755);return str(p)
 mocks={
 '/usr/sbin/scutil':stub('scutil','/bin/cat; printf "SIMULATED Link Active FALSE\\n"'),
 '/usr/sbin/tcpdump':stub('tcpdump','printf "SIMULATED BPF permission denied\\n" >&2; exit 1'),
 '/usr/sbin/ioreg':stub('ioreg','/bin/cat '+str(plist)),
 '/usr/sbin/networksetup':stub('networksetup','printf "SIMULATED network status %s\\n" "$*"'),
 '/usr/bin/kmutil':stub('kmutil','printf "SIMULATED loaded identity\\n"'),
 '/usr/sbin/sysctl':stub('sysctl','printf "SIMULATED boot\\n"'),
 '/usr/sbin/ipconfig':stub('ipconfig','printf "SIMULATED no DHCP %s\\n" "$*"; exit 1'),
 '/sbin/ifconfig':stub('ifconfig','printf "SIMULATED interface %s\\n" "$*"'),
 '/usr/bin/log':stub('log','printf "SIMULATED attempt log\\n"'),
 '/usr/sbin/chown':stub('chown','exit 0'),
 '/bin/sleep':stub('sleep','exit 0')}
 adapted=src.replace('ROOT="$(cd "$(dirname "$0")" && pwd)"','ROOT="'+str(root)+'"')
 for command,path in mocks.items():adapted=adapted.replace(command,path)
 script=t/'run.sh';script.write_text(adapted)
 for iface,node,inp,valid in [('en0','CNVW',b'\n',True),('en1','WIFI',b'\n',True),('en2','ARPT',b'',True),('en17','NETW',b'\n',True),('en0','WIFI',b'\n',False)]:
  plist.write_bytes(plistlib.dumps(variant(iface if valid else'en1',node)))
  env=dict(os.environ,BE200_PASSIVE_CAPTURE='1',BE200_OUTPUT_PARENT=str(out),WIFI_IFACE=iface,IOREG_WIFI_SERVICE=node,PYTHONDONTWRITEBYTECODE='1');env.pop('SUDO_UID',None);env.pop('SUDO_GID',None)
  p=subprocess.run(['/bin/bash',str(script)],input=inp,capture_output=True,timeout=20,env=env);assert p.returncode==(0 if valid else 2),p.stderr.decode()
  z=max(out.glob('*.zip'),key=lambda x:x.stat().st_mtime_ns)
  with zipfile.ZipFile(z)as f:
   assert f.testzip()is None;names=f.namelist();sums=next(n for n in names if n.endswith('/SHA256SUMS.txt'));prefix=sums.rsplit('/',1)[0]+'/'
   for l in f.read(sums).decode().splitlines():
    h,name=l.split('  ',1);assert hashlib.sha256(f.read(prefix+name.removeprefix('./'))).hexdigest()==h
   status=f.read(prefix+'COLLECTION_STATUS.txt').decode().strip()
   if not valid:
    assert status.startswith('ABORT:')and not any(n.endswith('/TCPDUMP.err')or n.endswith('/ATTEMPT.log')for n in names)
   else:
    binding=json.loads(f.read(prefix+'BEFORE.binding.json'));assert binding['wifi_iface']==iface and binding['provider_name']==node
    assert iface.encode()in f.read(prefix+'AFTER.ifconfig.txt')and ('getpacket '+iface).encode()in f.read(prefix+'DHCP.txt')
    assert ('/Interface/'+iface+'/Link').encode()in f.read(prefix+'BEFORE.dynamic-store.txt') or b'SIMULATED Link Active'in f.read(prefix+'BEFORE.dynamic-store.txt')
    assert ('interface='+iface).encode()in f.read(prefix+'PASSIVE_CAPTURE.txt')
    assert b'SIMULATED BPF permission denied'in f.read(prefix+'TCPDUMP.err')
    assert status==('user-finished'if inp else'input-timeout-or-interrupted')
  print('PASS SIMULATED actual collector',iface,node,status,'CRC/full-manifest')
print('No live registry/capture/logstream,settings,traffic,EFI or driver operation executed.')
