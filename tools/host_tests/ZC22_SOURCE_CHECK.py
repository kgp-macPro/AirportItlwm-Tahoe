#!/usr/bin/env python3
"""Verify changed packet construction/publication against the exact ZC21 source."""
from pathlib import Path
import subprocess,re,tempfile,sys,importlib.util,plistlib,struct,hashlib,os
R=Path(__file__).resolve().parents[2];BASE='bb9dbebbe09b01fec1f918f1fdd04ab6e9a49d14'
def mask(s):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s,flags=re.S)
def compact(s):return re.sub(r'\s+','',mask(s))
def span(s,n):
 m=mask(s);q=re.search(r'(?:static\s+)?(?:void|bool|int|IOReturn)\s+(?:ItlIwx::\s*)?'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return q.start(),b

def fn(s,n):a,b=span(s,n);return s[a:b]
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();old=subprocess.check_output(['git','-C',str(R),'show',BASE+':itlwm/hal_iwx/ItlIwx.cpp'],text=True)
changed=['allocatePacketBacking2H','iwx_tx_control_alloc','iwx_tx','dexterOriginal_iwx_tx','iwx_clear_tx_desc','iwx_tvqm_enable_txq']
# Reverse only the documented code edits, proving the old packet encodings,
# ownership inputs, queue selection, payload mapping and doorbell progression.
for n in ['iwx_tx','dexterOriginal_iwx_tx']:
 f=fn(s,n);o=fn(old,n)
 f=f.replace('    struct iwx_tfh_tfd prepared_desc;\n    struct iwx_device_cmd prepared_command = {};\n','')
 f=f.replace('desc = &prepared_desc;','desc = &ring->desc[idx];').replace('cmd = &prepared_command;','cmd = &ring->cmd[idx];')
 a=f.index('    // Validate/copy before transferring');b=f.index('    iwx_tx_update_byte_tbl',a)
 transfer='    data->m = m;\n    data->in = in;\n    data->type = type;\n'
 assert f.count(transfer)==1
 f=f[:a]+f[b:]
 f=f.replace('    DPRINTFN(3, ("sending data:',transfer+'    DPRINTFN(3, ("sending data:',1)
 assert compact(f)==compact(o),n+' changed packet semantics beyond construction/publication'
 f=fn(s,n);assert f.index('iwx_tx_publish_control(')<f.index('data->m = m;')<f.index('iwx_tx_update_byte_tbl(')<f.index('iwx_dma_publish_barrier();')<f.index('IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR')
 assert f.count('iwx_dma_publish_barrier();')==1
f=fn(s,'allocatePacketBacking2H').replace(', bool inhibitCache = false','').replace('((!coherentPayload || inhibitCache) ? kIOMapInhibitCache : 0)','(coherentPayload ? 0 : kIOMapInhibitCache)')
assert compact(f)==compact(fn(old,'allocatePacketBacking2H'))
f=fn(s,'iwx_tx_control_alloc').replace('fDma5Mapper, true, true)','fDma5Mapper, true)');assert compact(f)==compact(fn(old,'iwx_tx_control_alloc'))
f=fn(s,'iwx_clear_tx_desc');a=f.index('    if (ring->qid != IWX_DQA_CMD_QUEUE)');b=f.index('    uint8_t num_tbs',a);f=f[:a]+f[b:];assert compact(f)==compact(fn(old,'iwx_clear_tx_desc'))
f=fn(s,'iwx_tvqm_enable_txq').replace('ring->cur = wr_idx;\n    ring->tail = wr_idx & (ring->ring_count - 1);','ring->cur = ring->tail = wr_idx;');assert compact(f)==compact(fn(old,'iwx_tvqm_enable_txq'))
# Everything else in the production translation unit remains exact code.
now=s.replace('AirportItlwm-Tahoe-B1-ZC22-PERF','AirportItlwm-Tahoe-B1-ZC21-PERF')
for n in changed:
 a,b=span(now,n);now=now[:a]+fn(old,n)+now[b:]
a,b=span(now,'iwx_tx_publish_control');now=now[:a]+now[b:]
assert compact(now)==compact(old),'Unexpected production changes'
assert (R/'itlwm/hal_iwx/ItlIwx.hpp').read_bytes()==subprocess.check_output(['git','-C',str(R),'show',BASE+':itlwm/hal_iwx/ItlIwx.hpp'])
# No unrelated pre-existing tracked input changed (SDK/firmware/Apple-facing
# queues/RX/power/TX completion tokens included).
for entry in subprocess.check_output(['git','-C',str(R),'ls-tree','-rz',BASE]).split(b'\0'):
 if not entry:continue
 meta,n=entry.split(b'\t',1);n=n.decode()
 if n=='itlwm/hal_iwx/ItlIwx.cpp':continue
 mode,kind,blob=meta.decode().split();data=os.readlink(R/n).encode() if mode=='120000' else (R/n).read_bytes()
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob,n
# Exact fence primitive still emits a hardware memory-ordering instruction.
with tempfile.TemporaryDirectory(prefix='zc22-fence-') as td:
 p=Path(td);(p/'f.cpp').write_text(fn(s,'iwx_dma_publish_barrier')+'\nvoid publish(volatile unsigned*p,unsigned*d){*d=42;iwx_dma_publish_barrier();*p=1;}\n')
 subprocess.run(['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-target','x86_64-apple-macos','-O2','-S',str(p/'f.cpp'),'-o',str(p/'f.s')],check=True)
 assert 'mfence' in (p/'f.s').read_text() or 'lock' in (p/'f.s').read_text()
runner=(R/'tools/ZC22_3x14_PORTABLE_IP_PROMPT.sh').read_text();assert not re.search(r'^\s*(?:/usr/sbin/)?system_profiler\s',runner,re.M)
subprocess.run(['/bin/bash','-n',str(R/'tools/ZC22_3x14_PORTABLE_IP_PROMPT.sh')],check=True)
subprocess.run([sys.executable,'-B',str(R/'tools/host_tests/ZC22_BENCHMARK_GUARD_TEST.py')],check=True)
spec=importlib.util.spec_from_file_location('guard',R/'tools/host_tests/ZC22_BENCHMARK_GUARD_TEST.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
with tempfile.TemporaryDirectory(prefix='zc22-marker-') as td:
 p=Path(td);(p/'guard.py').write_text(mod.python_guard(runner))
 for mode in [1,2]:
  w=[0]*32;w[:8]=[19,1,256,10,123,mode,1,2];w[11:13]=[1,2]
  (p/'i.plist').write_bytes(plistlib.dumps([{'KGP_ZC19':struct.pack('<32Q',*w),'KGP_RX_EXPERIMENT':'AirportItlwm-Tahoe-B1-ZC22-PERF'}]))
  subprocess.run([sys.executable,str(p/'guard.py'),str(p/'i.plist'),'NONE','NONE','mapped' if mode==2 else 'conventional','ZC22','boundary'],check=True)
print('PASS exact ZC21 preservation outside documented construction/publication/tail changes; packet field/mapping/queue/ownership/fence equivalence; SDK/firmware/RX/power unchanged; ZC22 identity and corrected guards in both modes')
