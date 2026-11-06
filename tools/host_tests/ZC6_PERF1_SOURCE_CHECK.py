#!/usr/bin/env python3
"""Frozen-base comparison and passive-instrumentation scope checks (no build)."""
from pathlib import Path
import subprocess,re,hashlib,os,json
W=Path(__file__).resolve().parents[2];BASE='bc8b39bb6ceede8522f5e1c8126d078dd10aac64'
def git(*a):return subprocess.check_output(['git','-C',str(W),*a],env={**os.environ,'GIT_OPTIONAL_LOCKS':'0'}).decode()
def methods(s):
 mask=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S);out={}
 for x in re.finditer(r'ItlIwx::\s*(\w+)\s*\(',mask):
  a=mask.find('{',x.end());semi=mask.find(';',x.end())
  if a<0 or 0<=semi<a:continue
  b=a+1;n=1
  while n:n+=(mask[b]=='{')-(mask[b]=='}');b+=1
  out[x[1]]=(s[a:b],s.count('\n',0,x.start())+1)
 return out
C='itlwm/hal_iwx/ItlIwx.cpp';old=git('show',BASE+':'+C);new=(W/C).read_text();a=methods(old);b=methods(new)
allowed={'dma5Select','iwx_alloc_rx_ring','drain2MRequest','drain2MAction','iwx_reset_tx_ring','iwx_rx_frame','iwx_rx_mpdu_mq','iwx_rx_tx_cmd_single','iwx_clear_oactive','iwx_rx_tx_ba_notif','iwx_ampdu_txq_advance','iwx_rx_tx_cmd','iwx_tx','iwx_flush_sta_tids','_iwx_start_task','iwx_start','iwx_watchdog','iwx_intr','dexterOriginal_iwx_tx','dexterOriginal_iwx_rx_tx_ba_notif','dexterOriginal_iwx_rx_tx_cmd','dexterOriginal_iwx_ampdu_txq_advance','dexterOriginal__iwx_start_task','dexterOriginal_iwx_start','dexterOriginal_iwx_intr'}
changed={k for k in a if a[k][0]!=b[k][0]};assert changed==allowed,changed^allowed
assert set(b)-set(a)=={'perf1Publish'}
for k in ('dma5Select','iwx_alloc_rx_ring'):
 assert a[k][0].replace('AirportItlwm-Tahoe-B1-ZC5-BOOTFIX1','AirportItlwm-Tahoe-B1-ZC6-PERF1')==b[k][0]
for k in a:
 if k.startswith(('pc1','tx2I','tx2J','cmd2K','rxZc','boot5')) or k in ('dma5Terminal','tx5Release','tx5RingReleasable','iwx_stop','iwx_stop_device'):
  assert a[k][0]==b[k][0],k
start='// B1-ZC5 BEGIN:';end='// B1-ZC5 END'
assert old[old.index(start):old.index(end)]==new[new.index(start):new.index(end)]
assert a['iwx_txd_done'][0]==b['iwx_txd_done'][0]
assert a['iwx_tx_ring_init'][0]==b['iwx_tx_ring_init'][0]
print('PASS PC1/STOP_MASTER, DMA routing (marker excepted), TX allocation/ownership/coherency, RX pool/cache/lifetime and descriptor layouts preserved')
for k in sorted(changed|{'perf1Publish'}):print('CHANGED',C+':'+str(b[k][1]),k)
# Every original tracked byte except explicitly reviewed files remains fixed.
changedfiles={'AirportItlwm/AirportItlwm.cpp','include/HAL/ItlHalService.hpp','itlwm/hal_iwx/ItlIwx.cpp','itlwm/hal_iwx/ItlIwx.hpp'}
for path in filter(None,git('ls-tree','-r','--name-only',BASE).splitlines()):
 if path not in changedfiles:assert (W/path).read_bytes()==subprocess.check_output(['git','-C',str(W),'show',BASE+':'+path]),path
print('PASS firmware, AWDL/P2P implementation, unrelated HALs, dependencies, version/project metadata and prior host tests unchanged')
for k in changed-{'dma5Select','iwx_alloc_rx_ring','iwx_watchdog'}:
 before=a[k][0];after=b[k][0]
 for op in ('withSpecification','prepare(','synchronize(','setProperty','thread_call_enter','IOSleep','commandSleep'):
  assert after.count(op)==before.count(op),(k,op)
assert sum(new.count('perf1Publish();') for _ in [0])==1
assert 'that->perf1Publish();' in b['iwx_watchdog'][0]
assert 'setProperty("KGP_ZC6_PERF1"' in b['perf1Publish'][0]
h=(W/'include/HAL/Perf1.hpp').read_text();assert 'alignas(64)' not in h and '__atomic_always_lock_free' in h
assert 'fPerf1' not in (W/'itlwm/hal_iwx/if_iwxvar.h').read_text()
# Instrumented originals retain all mbuf frees, DMA cursor calls, MMIOs and return values.
for k in changed:
 if k.startswith('dexterOriginal_'):
  for token in ('mbuf_freem(', 'iwx_txd_done(', 'getPhysicalSegmentsWithCoalesce(', 'IWX_WRITE(', 'signalOutputThread(', 'ieee80211_release_node('):
   assert a[k][0].count(token)==b[k][0].count(token),(k,token)
print('PASS no new packet-path mapping, allocation, waits, publication, ownership frees, or descriptor ABI changes')
subprocess.run(['git','-C',str(W),'diff','--check'],check=True)
print('PASS source diff whitespace checks; exact-body preservation no longer claimed for instrumented originals')
