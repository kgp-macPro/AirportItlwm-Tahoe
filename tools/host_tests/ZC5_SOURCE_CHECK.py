#!/usr/bin/env python3
"""Read-only source conservation and routing checks for independent ZC5 review."""
from pathlib import Path
import hashlib, re, subprocess, os
W=Path(__file__).resolve().parents[2]
W4=W.parent/'TRACK-B1-ZC4-SUPPLY'
E=Path('/Users/kgp/Developer/OpenIntelWireless/AirportItlwm-Tahoe-1.0.0-Implementation-2026-10-06')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(w,*args):return subprocess.check_output(['git','-C',str(w),*args],env={**os.environ,'GIT_OPTIONAL_LOCKS':'0'})
assert sha(E/'TRACK_B1_ZC4.diff')=='1de4a7cb8f360cf43c17047d5e958dd932e552d8fc87b02896057cabaf41f250'
assert sha(E/'TRACK_B1_ZC4_SOURCE_SHA256SUMS.txt')=='dc6c34fdef191e33665232c51620a037e19217aa6aa46148f4419e81f486f358'
lines=(E/'TRACK_B1_ZC4_SOURCE_SHA256SUMS.txt').read_text().splitlines()
for line in lines:
 digest,name=line.split('  ',1);assert sha(W4/name)==digest,name
assert len(lines)==266
assert git(W4,'rev-parse','HEAD').decode().strip()=='20daea29bc8474e77f42fc7a1b3155f0d62a90fa'
assert git(W4,'branch','--show-current').decode().strip()=='experiment/track-b1-zc4-supply-20261007'
assert git(W4,'status','--porcelain').decode()==' M itlwm/hal_iwx/ItlIwx.cpp\n M itlwm/hal_iwx/ItlIwx.hpp\n?? tools/host_tests/RX_ZC4_HOST_TEST.py\n'
for directory,head in [('/Users/kgp/Developer/OpenIntelWireless/itlwm-dexter-vtd-clean','51543d6cfecf8aae3f66792b0699f5a1bb16a368'),('/Users/kgp/OCLP-Github-KGP/kgp/AirportItlwm-Tahoe','6d14093de576c0d278f9bdc9f82b100393143fd3')]:
 assert git(directory,'rev-parse','HEAD').decode().strip()==head,directory
 assert not git(directory,'status','--porcelain'),directory
print('PASS original ZC4 266/266 + branch/status; public repository and canonical Dexter unchanged')
def methods(s):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda x:' '*len(x[0]),s,flags=re.S);out={}
 for x in re.finditer(r'ItlIwx::\s*(\w+)\s*\(',m):
  a=m.find('{',x.end());semi=m.find(';',x.end())
  if a<0 or 0<=semi<a:continue
  b=a+1;n=1
  while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
  out[x[1]]=(s[x.start():b],s.count('\n',0,x.start())+1,s.count('\n',0,b)+1)
 return out
p='itlwm/hal_iwx/ItlIwx.cpp';old=(W4/p).read_text();new=(W/p).read_text();a=methods(old);b=methods(new)
allowed={'tx2IAllocate','tx2IPrepare','tx2IPublished','tx2IRetire','tx2IFree','path2L','cmd2KPrepare','rxZcSetup','detach','free','iwx_ctxt_info_gen3_init','iwx_dma_contig_alloc','iwx_alloc_rx_ring','iwx_reset_rx_ring','iwx_free_rx_ring','iwx_reset_tx_ring','iwx_free_tx_ring','iwx_tvqm_enable_txq','iwx_update_rx_desc','iwx_rx_addbuf','iwx_ampdu_txq_advance','iwx_tx','iwx_rx_pkt','iwx_notif_intr','iwx_attach'}
changed={k for k in a if a[k][0]!=b.get(k,('ABSENT',))[0]}
assert changed==allowed,(changed^allowed)
added={'dma5Select','dma5Terminal','dma5Report','tx5RingReleasable','tx5Return','tx5Release','rxZcTelemetry'}
assert set(b)-set(a)==added
for k in sorted(changed|added):print(f'REVIEW {p}:{b[k][1]}-{b[k][2]} {k}')
for k in a:
 if k not in allowed:assert a[k][0]==b[k][0],k
assert a['cmd2KPrepare'][0].replace('kCmd2KSize, PAGE_SIZE, &stage)','kCmd2KSize, PAGE_SIZE, &stage, fDma5Mapper)')==b['cmd2KPrepare'][0]
print('PASS all other ItlIwx methods byte-identical, including PC1/STOP_MASTER, 2J/2M and command submission; 2K only mapper argument')
tracked=git(W,'ls-files','-z').decode().split('\0')
for p in filter(None,tracked):
 if p not in ['itlwm/hal_iwx/ItlIwx.cpp','itlwm/hal_iwx/ItlIwx.hpp']:assert (W/p).read_bytes()==(W4/p).read_bytes(),p
assert (W/'tools/host_tests/RX_ZC4_HOST_TEST.py').read_bytes()==(W4/'tools/host_tests/RX_ZC4_HOST_TEST.py').read_bytes()
print('PASS firmware/AWDL/framework/IWM/IWN/versions/project files and unrelated models unchanged')
block=new[new.index('// B1-ZC5 BEGIN:'):new.index('// B1-ZC5 END')]
assert 'memcpy(' not in block and 'getPhysicalSegments' not in block and 'mbuf_mclhasreference' not in block
for k in ('rxZcReplace','rxZcSync','rxZcRepost'):
 assert not any(x in b[k][0] for x in ['prepare(','setMemoryDescriptor','withAddressRange','mbuf_alloccluster','commandSleep','IOMalloc'])
assert 'synchronize(' not in b['tx2IPrepare'][0] and 'withSpecification' not in b['tx2IPrepare'][0]
assert 'for (unsigned attempt' not in b['tx2IPrepare'][0]
assert 'fTx5Free[--fTx5FreeCount]' in b['tx2IPrepare'][0]
assert 'if (dma5Mapped())\n        nsegs = tx2IPrepare' in b['iwx_tx'][0]
assert 'else\n    nsegs = data->map->cursor->getPhysicalSegmentsWithCoalesce' in b['iwx_tx'][0]
assert 'dma5Mapped() ? fRxZcPool->entry[fRxZcSlot[idx]].address : data->map->dm_segs[0].location' in b['iwx_update_rx_desc'][0]
assert b['iwx_tx'][0].index('OSSynchronizeIO()')<b['iwx_tx'][0].index('IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR')
assert 'cacheLimit = count' in block and 'if (cacheCount==cacheLimit)' in block
assert 'getIOMemoryDescriptor()!=bmd' in new
assert 'kIOMemoryMapperNone' in block
print('PASS explicit mapper split, no active-mapper cursor fallback, bounded cache, no new RX gated preparation or payload memcpy, no TX packet map/sync/scan')
subprocess.run(['git','-C',str(W),'diff','--check'],check=True)
assert git(W,'rev-parse','HEAD').decode().strip()=='20daea29bc8474e77f42fc7a1b3155f0d62a90fa'
assert git(W,'branch','--show-current').decode().strip()=='experiment/track-b1-zc5-20261010'
print('PASS ZC5 base/branch; no commit or kernel build required by these checks')

dexter=git('/Users/kgp/Developer/OpenIntelWireless/itlwm-dexter-vtd-clean','show','51543d6cfecf8aae3f66792b0699f5a1bb16a368:itlwm/hal_iwx/ItlIwx.cpp').decode()
d=methods(dexter)
r=b['iwx_rx_addbuf'][0];q=d['iwx_rx_addbuf'][0]
assert r[r.index('    struct iwx_rx_ring'):]==q[q.index('    struct iwx_rx_ring'):]
r=b['iwx_tx'][0];q=d['iwx_tx'][0]
start='/* Fill TX descriptor. */';end='iwx_tx_update_byte_tbl(sc, ring, idx, totlen, num_tbs);'
assert r[r.index(start):r.index(end)]==q[q.index(start):q.index(end)]
assert 'nsegs = data->map->cursor->getPhysicalSegmentsWithCoalesce(m, &segs[0], IWX_TFH_NUM_TBS - 2);' in q
assert 'setProperty' not in b['dma5Report'][0] and 'setProperty' not in b['rxZcTelemetry'][0]
print('PASS pinned Dexter conventional RX body, TX cursor/descriptor body, new telemetry publication outside RX gate')
