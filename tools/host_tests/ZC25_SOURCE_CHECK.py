#!/usr/bin/env python3
"""Verify bounded ZC25 edits against frozen ZC22, actual packet/fence/tool paths."""
from pathlib import Path
import subprocess,re,tempfile,sys,importlib.util,plistlib,struct,hashlib,os
R=Path(__file__).resolve().parents[2];BASE='7dd260fbfb9d612574a6b18fbb042d4963402eda'
def mask(s):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s,flags=re.S)
def compact(s):return re.sub(r'\s+','',mask(s))
def span(s,n):
 m=mask(s);q=re.search(r'(?:static\s+)?(?:void|bool|int|IOReturn)\s+(?:ItlIwx::\s*)?'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return q.start(),b
def fn(s,n):a,b=span(s,n);return s[a:b]
def oldfile(n):return subprocess.check_output(['git','-C',str(R),'show',BASE+':'+n],text=True)
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();old=oldfile('itlwm/hal_iwx/ItlIwx.cpp')
changed=['iwx_tx_publish_control','iwx_tx_control_alloc','iwx_alloc_tx_ring','iwx_reset_tx_ring','iwx_free_tx_ring','iwx_tvqm_alloc_txq','iwx_tvqm_enable_txq','iwx_tx','dexterOriginal_iwx_tx','rxZcSetup','rxZcReport','zc19Snapshot','zc19Publish']
added=['rxZcCapturePeriodic','rxZcPublishPeriodic']
for n in ['iwx_tx','dexterOriginal_iwx_tx']:
 f=fn(s,n);o=fn(old,n)
 f=f.replace('paddr = htole64(ring->first_tb_dma.paddr + size_t(idx) * IWX_FIRST_TB_SIZE_ALIGN);','paddr = htole64(data->cmd_paddr);')
 f=f.replace('(ring->first_tb_dma.paddr + size_t(idx) * IWX_FIRST_TB_SIZE_ALIGN) >> 32 !=\n        (ring->first_tb_dma.paddr + size_t(idx) * IWX_FIRST_TB_SIZE_ALIGN + IWX_FIRST_TB_SIZE) >> 32','data->cmd_paddr >> 32 != (data->cmd_paddr + le32toh(desc->tbs[0].tb_len)) >> 32')
 assert compact(f)==compact(o),n+' packet semantics beyond TB0 address changed'
 f=fn(s,n)
 assert f.index('iwx_tx_publish_control(')<f.index('data->m = m;')<f.index('iwx_tx_update_byte_tbl(')<f.index('iwx_dma_publish_barrier();')<f.index('IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR')
 assert f.count('iwx_dma_publish_barrier();')==1
# Replacing reviewed functions recovers every other production code token.
now=s.replace('AirportItlwm-Tahoe-B1-ZC25-PERF','AirportItlwm-Tahoe-B1-ZC22-PERF')
for n in changed:
 a,b=span(now,n);now=now[:a]+fn(old,n)+now[b:]
for n in added:a,b=span(now,n);now=now[:a]+now[b:]
assert compact(now)==compact(old),'Unexpected translation-unit behavior'
# Header difference is only allocator parameter plus fixed watchdog-owned storage.
h=(R/'itlwm/hal_iwx/ItlIwx.hpp').read_text()
h=h.replace(', bool inhibitCache = false','')
a=h.index('    // ZC25 watchdog-owned');b=h.index('\n    // PC1 control',a);h=h[:a]+h[b:]
assert compact(h)==compact(oldfile('itlwm/hal_iwx/ItlIwx.hpp'))
v=(R/'itlwm/hal_iwx/if_iwxvar.h').read_text();v=v.replace('    struct iwx_dma_info\tfirst_tb_dma; /* ZC25: isolated 64-byte-stride FH writeback slots */\n','')
# Do not depend on whitespace; locate exact single new declaration.
if compact(v)!=compact(oldfile('itlwm/hal_iwx/if_iwxvar.h')):
 v=re.sub(r'^.*\bfirst_tb_dma;[^\n]*\n','',v,flags=re.M)
assert compact(v)==compact(oldfile('itlwm/hal_iwx/if_iwxvar.h'))
# Entire unchanged pre-existing source/build/SDK/firmware/test inputs remain byte exact.
exceptions={'itlwm/hal_iwx/ItlIwx.cpp','itlwm/hal_iwx/ItlIwx.hpp','itlwm/hal_iwx/if_iwxvar.h'}
count=0
for entry in subprocess.check_output(['git','-C',str(R),'ls-tree','-rz',BASE]).split(b'\0'):
 if not entry:continue
 meta,n=entry.split(b'\t',1);n=n.decode()
 if n in exceptions:continue
 mode,kind,blob=meta.decode().split();data=os.readlink(R/n).encode() if mode=='120000' else (R/n).read_bytes()
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob,n
 count+=1
# CPU constructed command prefix is split; only hardware-written 20B first TB UC.
f=fn(s,'iwx_tx_publish_control')
assert f.count('memcpy(')==5 and 'command, IWX_FIRST_TB_SIZE' in f
assert f.index('UINT64_MAX - command_offset')<f.index('command_address =')
assert f.index('UINT64_MAX - first_offset')<f.index('scratch_address =')
assert 'IWX_FIRST_TB_SIZE_ALIGN > ring->first_tb_dma.size' in f
assert 'ring->first_tb_dma.vaddr' in f and 'IWX_DQA_CMD_QUEUE' in f
assert 'true, inhibitCache)' in fn(s,'iwx_tx_control_alloc')
for n in ['iwx_alloc_tx_ring','iwx_tvqm_enable_txq']:
 f=fn(s,n);assert 'IWX_FIRST_TB_SIZE_ALIGN, true)' in f and 'iwx_tx_control_alloc(&ring->bc_tbl' in f
assert 'first_tb_dma.cmd' in fn(s,'iwx_tvqm_enable_txq') and 'first_tb_dma.cmd' in fn(s,'iwx_tvqm_alloc_txq')
assert fn(s,'iwx_free_tx_ring').index('tx5RingReleasable')<fn(s,'iwx_free_tx_ring').index('iwx_dma_contig_free(&ring->first_tb_dma)')
# Publication fence remains real x86 ordering, not empty SDK OSSynchronizeIO.
with tempfile.TemporaryDirectory(prefix='zc25-fence-') as td:
 p=Path(td);(p/'f.cpp').write_text(fn(s,'iwx_dma_publish_barrier')+'\nvoid publish(volatile unsigned*p,unsigned*d){*d=42;iwx_dma_publish_barrier();*p=1;}\n')
 subprocess.run(['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-target','x86_64-apple-macos','-O2','-S',str(p/'f.cpp'),'-o',str(p/'f.s')],check=True)
 assert 'mfence' in (p/'f.s').read_text() or 'lock' in (p/'f.s').read_text()
# Both firmware command and key lifetime functions retain exact ZC22 implementations.
for n in ['iwx_send_cmd','iwx_send_cmd_pdu','rxZcArm','rxZcSync','rxZcFree','rxZcReplace','rxZcTerminal','iwx_ampdu_txq_advance']:
 assert compact(fn(s,n))==compact(fn(old,n)),n
runner=(R/'tools/ZC25_3x14_PORTABLE_IP_PROMPT.sh').read_text()
assert "('ZC19','ZC21','ZC22','ZC25')" in runner
assert not re.search(r'^\s*(?:/usr/sbin/)?system_profiler\s',runner,re.M)
subprocess.run(['/bin/bash','-n',str(R/'tools/ZC25_3x14_PORTABLE_IP_PROMPT.sh')],check=True)
subprocess.run([sys.executable,'-B',str(R/'tools/host_tests/ZC25_BENCHMARK_GUARD_TEST.py')],check=True)
spec=importlib.util.spec_from_file_location('guard',R/'tools/host_tests/ZC25_BENCHMARK_GUARD_TEST.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
with tempfile.TemporaryDirectory(prefix='zc25-marker-') as td:
 p=Path(td);(p/'guard.py').write_text(mod.python_guard(runner))
 for mode in [1,2]:
  w=[0]*32;w[:8]=[19,1,256,10,123,mode,1,2];w[11:13]=[1,2]
  (p/'i.plist').write_bytes(plistlib.dumps([{'KGP_ZC19':struct.pack('<32Q',*w),'KGP_RX_EXPERIMENT':'AirportItlwm-Tahoe-B1-ZC25-PERF'}]))
  subprocess.run([sys.executable,str(p/'guard.py'),str(p/'i.plist'),'NONE','NONE','mapped' if mode==2 else 'conventional','ZC25','boundary'],check=True)
subprocess.run([sys.executable,'-B',str(R/'tools/host_tests/ZC25_RX_DECODER_TEST.py')],check=True)
print('PASS ZC25 exact ZC22 preservation:',count,'old files plus all production outside reviewed functions; TB0-only packet change; qualified alloc/retain/reset/free; FW commands unchanged; real fence; identity/health/online guards/decoder')
