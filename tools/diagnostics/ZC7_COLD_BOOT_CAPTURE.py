#!/usr/bin/env python3
"""KGP runs once after a genuine cold boot, BEFORE manual Wi-Fi join.
Read-only commands only. Does NOT join, toggle, reboot, alter preferences or test traffic.
Run with python3; optional sudo grants wdutil/log visibility, not configuration changes.
"""
from pathlib import Path
import datetime,subprocess,json,hashlib,os
base=Path('/Users/kgp/Desktop/AirportItlwm/B1-ZC7-PERF')
out=base/('cold-boot-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
out.mkdir(exist_ok=False)
commands=[
 ['sw_vers'],['sysctl','kern.boottime','kern.bootsessionuuid'],['uptime'],
 ['kmutil','showloaded'],['ifconfig','en2'],['ipconfig','getifaddr','en2'],
 ['ipconfig','getpacket','en2'],['networksetup','-listallhardwareports'],
 ['networksetup','-getairportnetwork','en2'],['wdutil','info'],
 ['system_profiler','SPAirPortDataType','-detailLevel','mini'],
 ['ioreg','-l','-w','0','-r','-c','AirportItlwm'],
 ['ioreg','-l','-w','0','-r','-c','IOPCIDevice'],
 ['log','show','--last','20m','--style','compact','--info','--predicate',
  '(process == "airportd") OR (process == "wifid") OR (process == "kernel" AND (eventMessage CONTAINS[c] "iwx" OR eventMessage CONTAINS[c] "AirportItlwm" OR eventMessage CONTAINS[c] "KGP_" OR eventMessage CONTAINS[c] "80211"))']]
records=[]
for i,cmd in enumerate(commands):
 p=out/(str(i).zfill(2)+'-'+cmd[0]+'.txt');started=datetime.datetime.now().isoformat()
 try:
  r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
  p.write_bytes(r.stdout);code=r.returncode
 except subprocess.TimeoutExpired as e:p.write_bytes((e.stdout or b'')+b'\nCAPTURE TIMEOUT\n');code='TIMEOUT'
 except OSError as e:p.write_text(str(e)+'\n');code='UNAVAILABLE'
 records.append({'command':cmd,'started':started,'status':code,'file':p.name})
(out/'CAPTURE.json').write_text(json.dumps({'note':'Before manual join; no recovery actions. Record whether genuine cold power-off, time since login and observed UI state separately. Private/redacted OS logs may not show auto-join requests.','uid':os.getuid(),'commands':records},indent=2)+'\n')
(out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
print(out)
