#!/usr/bin/env python3
"""Exact bounded ZC26 production diff, preserved ownership/reporting, shared protocol."""
from pathlib import Path
import subprocess,re,hashlib,os,sys
R=Path(__file__).resolve().parents[2]
BASE='513e3708800bdd20ee15debe17dda0cec9e90b4f'
Z22='7dd260fbfb9d612574a6b18fbb042d4963402eda'
def mask(s):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s,flags=re.S)
def compact(s):return re.sub(r'\s+','',mask(s))
def span(s,n):
 m=mask(s);q=re.search(r'(?:static\s+)?(?:inline\s+)?(?:void|bool|int|IOReturn)\s+(?:ItlIwx::\s*)?'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return q.start(),b
def fn(s,n):a,b=span(s,n);return s[a:b]
def old(n,rev=BASE):return subprocess.check_output(['git','-C',str(R),'show',rev+':'+n],text=True)
cpp='itlwm/hal_iwx/ItlIwx.cpp';s=(R/cpp).read_text();prior=old(cpp);z22=old(cpp,Z22)
restored=['iwx_tx_control_alloc','iwx_alloc_tx_ring','iwx_reset_tx_ring','iwx_free_tx_ring','iwx_tvqm_alloc_txq','iwx_tx','dexterOriginal_iwx_tx']
for n in restored:assert compact(fn(s,n))==compact(fn(z22,n)),n+' not exact ZC22'
f=fn(s,'iwx_tvqm_enable_txq').replace('iwx_dma_queue_publish_barrier','iwx_dma_publish_barrier')
assert compact(f)==compact(fn(z22,'iwx_tvqm_enable_txq'))
f=fn(s,'iwx_enable_txq').replace('iwx_dma_queue_publish_barrier','iwx_dma_publish_barrier')
assert compact(f)==compact(fn(prior,'iwx_enable_txq'))
for n in ['iwx_tx','dexterOriginal_iwx_tx']:
 f=fn(s,n)
 assert f.index('iwx_tx_publish_control(')<f.index('data->m = m;')<f.index('iwx_tx_update_byte_tbl(')<f.index('iwx_dma_publish_barrier();')<f.index('IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR')
assert s.count('iwx_dma_publish_barrier();')==2
assert '__ATOMIC_SEQ_CST' in fn(s,'iwx_dma_publish_barrier')
assert 'sfence' not in mask(fn(s,'iwx_dma_publish_barrier')).lower()
assert s.count('iwx_dma_queue_publish_barrier();')==2
p=fn(s,'iwx_tx_publish_control')
assert p.count('memcpy(')==4 and 'first_tb_dma' not in s
assert p.index('UINT64_MAX - command_offset')<p.index('address =')
assert 'address + IWX_FIRST_TB_SIZE' in p
assert 'true, true)' in fn(s,'iwx_tx_control_alloc')
# Replace ONLY reviewed function bodies: all other production tokens must recover25.
changed=restored+['iwx_tvqm_enable_txq','iwx_enable_txq','iwx_tx_publish_control','iwx_dma_publish_barrier','iwx_send_cmd_status','iwx_add_sta_cmd','iwx_add_aux_sta']
now=s.replace('AirportItlwm-Tahoe-B1-ZC26-PERF','AirportItlwm-Tahoe-B1-ZC25-PERF')
for n in changed:
 a,b=span(now,n);now=now[:a]+fn(prior,n)+now[b:]
a,b=span(now,'iwx_dma_queue_publish_barrier');now=now[:a]+now[b:]
assert compact(now)==compact(prior),'unexpected production behavior outside reviewed methods'
# Header and ring layout revert ONLY the scratch allocation/allocator bool.
h='itlwm/hal_iwx/ItlIwx.hpp';v='itlwm/hal_iwx/if_iwxvar.h'
assert compact((R/h).read_text())==compact(old(h).replace(', bool inhibitCache = false',''))
assert compact((R/v).read_text())==compact(re.sub(r'^.*\bfirst_tb_dma;[^\n]*\n','',old(v),flags=re.M))
# All RX, completion, command transport, PC1 and restart/power code byte token-exact25.
for n in ['iwx_send_cmd','iwx_send_cmd_pdu','rxZcArm','rxZcSync','rxZcFree','rxZcReplace','rxZcTerminal','rxZcReport','rxZcCapturePeriodic','rxZcPublishPeriodic','iwx_ampdu_txq_advance']:
 assert compact(fn(s,n))==compact(fn(prior,n)),n
exceptions={cpp,h,v,'tools/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh','tools/host_tests/SHARED_BENCHMARK_TEST.py'}
count=0
for entry in subprocess.check_output(['git','-C',str(R),'ls-tree','-rz',BASE]).split(b'\0'):
 if not entry:continue
 meta,n=entry.split(b'\t',1);n=n.decode()
 if n in exceptions:continue
 mode,kind,blob=meta.decode().split();data=os.readlink(R/n).encode() if mode=='120000' else (R/n).read_bytes()
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob,n
 count+=1
# Sharedr2 differs only in identity support and display revision, not traffic.
r='tools/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh';runner=(R/r).read_text()
normalized=runner.replace('ZC26|ZC25','ZC25').replace("'ZC25','ZC26'","'ZC25'").replace('shared-2026-10-19-r2','shared-2026-10-19-r1')
assert normalized==old(r),'measurement/guard changes beyond identity support'
assert "PROTOCOL_VERSION='AIRPORTITLWM_3X14_NO_PROFILER_V1'" in runner
assert not re.search(r'^\s*(?:/usr/sbin/)?system_profiler\s',runner,re.M)
subprocess.run(['/bin/bash','-n',str(R/r)],check=True)
subprocess.run([sys.executable,'-B',str(R/'tools/host_tests/SHARED_BENCHMARK_TEST.py')],check=True)
subprocess.run([sys.executable,'-B',str(R/'tools/host_tests/ZC26_RX_DECODER_TEST.py')],check=True)
print('PASS ZC26:',count,'unchanged existing files; exact bounded production changes; ZC22 publication + ZC25 RX reporting/safety; 2packet/2queue fence roles; shared traffic protocol unchanged')
