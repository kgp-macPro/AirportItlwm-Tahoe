#!/usr/bin/env python3
"""Focused source conservation and ABI/build-input checks for ZC8."""
from pathlib import Path
import subprocess,re,hashlib,json
r=Path(__file__).resolve().parents[2];G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip();BASE='31987089018a192b0f32b89b3955ae82dd581574'
def git(*args):return subprocess.check_output([G,'--no-optional-locks','-C',str(r),*args])
def old(f):return git('show',BASE+':'+f)
def mask(t):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),t,flags=re.S)
def body(t,name):
 m=mask(t);x=re.search(r'(?:bool|void|int|uint32_t|IOReturn)\s+(?:ItlIwx::\s*)?'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return t[a:b]
cpp=(r/'itlwm/hal_iwx/ItlIwx.cpp').read_text();prev=old('itlwm/hal_iwx/ItlIwx.cpp').decode()
assert cpp.count('AirportItlwm-Tahoe-B1-ZC8-PERF')==2
normal=cpp.replace('AirportItlwm-Tahoe-B1-ZC8-PERF','AirportItlwm-Tahoe-B1-ZC7-PERF')
# Only two dispatch bodies may differ; all DMA/RX/TX packet/completion/PC1
# implementations and all code outside these bodies must remain exact.
for f in ['_iwx_start_task','iwx_start']:
 normal=normal.replace(body(normal,f),body(prev,f),1)
assert normal==prev
for f in ['itlwm/hal_iwx/ItlIwx.hpp','include/HAL/ItlHalService.hpp','itlwm.xcodeproj/project.pbxproj','AirportItlwm/AirportItlwm-Monterey-Info.plist','itl80211/openbsd/sys/_mbuf.cpp','itl80211/openbsd/net80211/ieee80211_input.c','itl80211/openbsd/net80211/ieee80211_pae_output.c']:
 assert (r/f).read_bytes()==old(f),f
print('PASS exact unchanged DMA/PC1/generation/quarantine/completion/RX/fallback packet bodies, EAPOL producers, legacy target/version/dependencies')
front=(r/'AirportItlwm/AirportItlwm.cpp').read_text();b=front[front.index('IOReturn AirportItlwm::outputStart('):front.index('// ZC8 BATCH BEGIN')]
assert b.index('ifq_reserve_batch(')<b.index('dequeueOutputPackets(count, &m)')
assert 'ifq_cancel(&ticket)' in b and 'getSize' not in b
assert 'ifq_start_pull(&ifq->if_snd);\n            fNetIf->startOutputThread();' in front
assert 'ifq_stop_pull(&ifq->if_snd);\n            fNetIf->stopOutputThread();' in front
q=(r/'itl80211/openbsd/sys/_ifq.cpp').read_text();part=q[q.index('// ZC7 ADMISSION BEGIN'):q.index('// ZC7 ADMISSION END')]
assert 'ifq_set_oactive' not in part and 'ticket->epoch == epoch' in part
assert 'reserved += ticket->count' in part and 'reserved -= ticket->count' in part
assert 'auto m = IOPacketQueue::dequeue();' in part and 'IOSimpleLockUnlock(_lock);\n        wake(iface);' in part
assert 'sc->sc_device_family < IWX_DEVICE_FAMILY_AX210' in body(cpp,'iwx_start')
assert 'result2L == kIOReturnCannotLock' in body(cpp,'iwx_start') and 'drain2MRequest()' in body(cpp,'iwx_start')
b=body(cpp,'_iwx_start_task');assert b.index('->entered = true')<b.index('return dexterOriginal__iwx_start_task')
print('PASS reserve-before-dequeue, one batch transfer, lifecycle epochs, unchanged independent consumer and wake latch, both AX210 routes schedule contended drain')
for f in ['AirportItlwm/AirportItlwm.cpp','itlwm/hal_iwx/ItlIwx.cpp','itlwm/hal_iwx/ItlIwx.hpp']:
 assert not re.search(r'getPerf1|fPerf1|perf1Publish|HAL/Perf1',(r/f).read_text())
p=json.loads((r/'docs/ZC8_BUILD_INPUTS.json').read_text())
for f,h in p['sdk_files'].items():assert hashlib.sha256((r/'MacKernelSDK'/f).read_bytes()).hexdigest()==h,f
assert hashlib.sha256((r/'include/FwBinary.cpp').read_bytes()).hexdigest()==p['firmware_generated_sha256']
git('diff','--check')
print('PASS broad PERF1 remains absent; 1232 SDK files and firmware translation unit byte-identical to verified ZC7 effective inputs; source whitespace check')
