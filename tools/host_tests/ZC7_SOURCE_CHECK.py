#!/usr/bin/env python3
"""Source invariants; does not execute the driver or establish physical behavior."""
from pathlib import Path
import subprocess,re
r=Path(__file__).resolve().parents[2]
def old(f):return subprocess.check_output(['git','-C',str(r),'show','bc8b39bb6ceede8522f5e1c8126d078dd10aac64:'+f])
for f in ['itlwm/hal_iwx/ItlIwx.cpp','itlwm/hal_iwx/ItlIwx.hpp','include/HAL/ItlHalService.hpp']:
 expected=old(f).replace(b'AirportItlwm-Tahoe-B1-ZC5-BOOTFIX1',b'AirportItlwm-Tahoe-B1-ZC7-PERF')
 assert (r/f).read_bytes()==expected,f
print('PASS complete IWX implementation/headers equal functional BOOTFIX1 baseline except marker: DMA, PC1, RX storage/cache/worker, mapped TX and conventional bodies unchanged')
cpp=(r/'AirportItlwm/AirportItlwm.cpp').read_text();start=cpp[cpp.index('IOReturn AirportItlwm::outputStart('):cpp.index('UInt32 AirportItlwm::outputPacket(')]
assert start.index('ifq_reserve(')<start.index('dequeueOutputPackets(1, &m)')
assert 'ifq_cancel(&ticket)' in start and 'getSize' not in start
assert 'ifq_start_pull(&ifq->if_snd);\n            fNetIf->startOutputThread();' in cpp
assert 'ifq_stop_pull(&ifq->if_snd);\n            fNetIf->stopOutputThread();' in cpp
q=(r/'itl80211/openbsd/sys/_ifq.cpp').read_text()
assert 'ifq_set_oactive' not in q[q.index('// ZC7 ADMISSION BEGIN'):q.index('// ZC7 ADMISSION END')]
assert 'ticket->epoch == epoch' in q and 'pullOpen = false; waiting = false; ++epoch' in q
assert 'uint64_t(IOPacketQueue::getSize()) + reserved + count <=' in q
assert 'auto m = IOPacketQueue::dequeue();' in q
assert 'IOSimpleLockUnlock(_lock);\n        wake(iface);' in q
print('PASS reserve before ownership transfer, internal producers share capacity, independent consumer/oactive, epoch cancellation, wake outside lock')
rx=(r/'itl80211/openbsd/sys/_mbuf.cpp').read_text();assert 'while ((m = ml_dequeue(ml)) != nullptr)' in rx and 'MBUF_LIST_FOREACH(ml, m)' not in rx
rx=(r/'itl80211/openbsd/net80211/ieee80211_input.c').read_text();rx=rx[rx.index('void\nieee80211_amsdu_decap('):];rx=rx[:rx.index('\n/*')]
assert rx.index('ieee80211_amsdu_decap_validate(')<rx.index('mbuf_split(')
assert 'mbuf_len(m) < ETHER_HDR_LEN + LLC_SNAPFRAMELEN' in rx
assert 'n = NULL;' in rx and 'mbuf_split(m, len, MBUF_DONTWAIT, &n) != 0' in rx
print('PASS RX detaches nextpkt before handoff; A-MSDU validates while remainder is still owned, explicit KPI error handling')
for f in ['AirportItlwm/AirportItlwm.cpp','itlwm/hal_iwx/ItlIwx.cpp','itlwm/hal_iwx/ItlIwx.hpp','include/HAL/ItlHalService.hpp']:
 s=(r/f).read_text();assert not re.search(r'getPerf1|fPerf1|perf1Publish|HAL/Perf1',s),f
print('PASS PERF1 runtime field/calls/publisher absent, not merely hidden')
for f in ['itlwm.xcodeproj/project.pbxproj','AirportItlwm/Info.plist','AirportItlwm/AirportItlwm-Sonoma-Info.plist','itl80211/openbsd/net80211/ieee80211_pae_output.c']:
 assert (r/f).read_bytes()==old(f),f
subprocess.run(['git','-C',str(r),'diff','--check'],check=True)
print('PASS build/dependencies/version/EAPOL producer preserved; SOURCE CHECK PASS')
