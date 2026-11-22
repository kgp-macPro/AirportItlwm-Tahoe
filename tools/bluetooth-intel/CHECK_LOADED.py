#!/usr/bin/env python3
"""Read loaded identities and physical USB selection; never send HCI commands."""
from pathlib import Path
import json,plistlib,re,subprocess,sys
IDS={'BlueToolFixup':'as.acidanthera.BlueToolFixup','IntelBluetoothFirmware':'com.zxystd.IntelBluetoothFirmware','IntelBTPatcher':'com.zxystd.IntelBTPatcher'}
KNOWN={0x26,0x29,0x32,0x33,0x35,0x36,0x38} # composite-device labels, not an allowlist

def physical_bluetooth_devices(roots):
 def bt(d,prefix):return all(d.get(prefix+k)==v for k,v in [('Class',0xe0),('SubClass',1),('Protocol',1)])
 def interface(d):return (d.get('IOObjectClass')=='IOUSBHostInterface'and bt(d,'bInterface'))or any(interface(c)for c in d.get('IORegistryEntryChildren',[]))
 return [d for d in roots if d.get('idVendor')==0x8087 and (d.get('idProduct')in KNOWN or bt(d,'bDevice')or interface(d))]

def compare(text,components):
 rows=[]
 for c in components:
  found=[line for line in text.splitlines()if IDS[c['component']]+' 'in line]
  ok=len(found)==1 and c['uuid'].lower()in found[0].lower()and '('+c['version']+')'in found[0]
  rows.append(dict(component=c['component'],expected_version=c['version'],expected_uuid=c['uuid'],identity_matches=ok))
 return rows

def main():
 if '--self-test'in sys.argv:
  cs=[dict(component=k,version='2.7.5'if k=='BlueToolFixup'else'2.5.3',uuid='12345678-1234-1234-1234-123456789abc')for k in IDS]
  text='\n'.join(IDS[c['component']]+' ('+c['version']+') '+c['uuid']for c in cs)
  assert all(r['identity_matches']for r in compare(text,cs))
  assert not compare(text.replace('BlueToolFixup (2.7.5)','BlueToolFixup (2.7.4)'),cs)[0]['identity_matches']
  for product in KNOWN:assert len(physical_bluetooth_devices([dict(idVendor=0x8087,idProduct=product)]))==1
  generic=dict(idVendor=0x8087,idProduct=0x9999,bDeviceClass=0xe0,bDeviceSubClass=1,bDeviceProtocol=1)
  hub=dict(idVendor=0x8087,idProduct=0x9998,bDeviceClass=9)
  assert physical_bluetooth_devices([generic,hub])==[generic]
  print('PASS read-only identity/USB selector fixtures; no HCI preflight or report required.');return 0
 root=Path(__file__).resolve().parent
 components=json.loads((root/'BUILD_IDENTITY.json').read_text())['components']
 loaded=subprocess.run(['/usr/bin/kmutil','showloaded','--list-only'],capture_output=True,text=True,timeout=30)
 if loaded.returncode:print(loaded.stderr);return 1
 rows=compare(loaded.stdout,components)
 for row in rows:
  print(row['component']+': '+('expected build loaded'if row['identity_matches']else'WRONG OR MISSING BUILD')+'; expected '+row['expected_version']+' / '+row['expected_uuid'])
 usb=subprocess.run(['/usr/sbin/ioreg','-r','-c','IOUSBHostDevice','-a','-l'],capture_output=True,timeout=20)
 if usb.returncode:print('USB query failed.');return 1
 devices=[dict(registry_id=d.get('IORegistryEntryID'),product=hex(d.get('idProduct',0)),firmware_loaded=d.get('FirmwareLoaded')is True,selection=d.get('KGPBLESelection'))for d in physical_bluetooth_devices(plistlib.loads(usb.stdout))]
 boot=subprocess.run(['/usr/sbin/sysctl','-n','kern.bootargs'],capture_output=True,text=True,timeout=10)
 opt_in=bool({'-kgpble','-kgpble25332'}&set(boot.stdout.split()))if not boot.returncode else None
 print(json.dumps(dict(ble_opt_in=opt_in,physical_devices=devices),indent=2))
 print('No separate HCI capability report is required. FirmwareLoaded is informational. Verify native patch activation and AirDrop separately.')
 return 0 if all(r['identity_matches']for r in rows)else 1
if __name__=='__main__':sys.exit(main())
