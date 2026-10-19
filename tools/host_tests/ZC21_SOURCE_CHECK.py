#!/usr/bin/env python3
"""Exact-source retention, publication, qualification and benchmark guard checks."""
from pathlib import Path
import subprocess,re,tempfile,sys
R=Path(__file__).resolve().parents[2];BASE='db22bb90b72b440b657246940fd8da17dbee23a4'
def old(n):return subprocess.check_output(['git','-C',str(R),'show',BASE+':'+n]).decode()
def mask(s):return re.sub(r'//[^\n]*|/\*.*?\*/', '',s,flags=re.S)
def fn(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 q=re.search(r'(?:static\s+)?(?:void|bool|int|IOReturn|uint32_t)\s+(?:ItlIwx::\s*|AirportItlwm::\s*)?'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[q.start():b]
s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();b=old('itlwm/hal_iwx/ItlIwx.cpp')
names=['iwx_rx_mpdu_mq','iwx_rx_frame','iwx_rx_pkt','dexterOriginal_iwx_rx_pkt','iwx_ccmp_decap','iwx_rxmq_get_signal_strength','iwx_notif_intr','dexterOriginal_iwx_notif_intr','iwx_clear_oactive','iwx_ampdu_txq_advance','iwx_rx_tx_ba_notif','_iwx_start_task','iwx_reset_tx_ring','iwx_stop_device','iwx_send_cmd','iwx_alloc_rx_ring','dexterOriginal_iwx_alloc_rx_ring']
for n in names:
 a=fn(s,n).replace('ZC19-PERF','ZC8-PERF').replace('ZC21-PERF','ZC8-PERF');assert mask(a)==mask(fn(b,n)),n
for path in ['itl80211/openbsd/sys/_ifq.cpp','itl80211/openbsd/sys/_ifq.h','itl80211/openbsd/net80211/ieee80211_input.cpp']:
 p=R/path
 if p.exists():assert p.read_text()==old(path),path
for n in ['dma5Select','pc1StopAction','pc1RingAction','pc1AdvanceAction','tx2IRetire','rxZcCommit']:
 if re.search(r'\b'+n+r'\(',s):assert mask(fn(s,n).replace('ZC19-PERF','ZC8-PERF').replace('ZC21-PERF','ZC8-PERF'))==mask(fn(b,n)),n
for n in ['iwx_enable_txq','iwx_tvqm_enable_txq','iwx_tx','dexterOriginal_iwx_tx']:
 f=fn(s,n);assert f.count('iwx_dma_publish_barrier();')==1,n
 if 'iwx_tx' in n and n not in ['iwx_enable_txq','iwx_tvqm_enable_txq']:
  assert f.index('iwx_dma_publish_barrier();')<f.index('IWX_WRITE(sc, IWX_HBUS_TARG_WRPTR'),n
assert 'iwx_rx_free_desc_alloc' not in s and 'zc17' not in s and 'zc10' not in s
assert 'ifq_service_handoff' not in s and 'Zc10Observers' not in s
assert fn(s,'iwx_tx_control_alloc').count('fDma5Selected')==1
assert 'return kIOReturnNotReady' in fn(s,'zc16PublishRun')
assert fn(s,'iwx_sta_rx_agg').count('(timeout_val + 999) / 1000')==1
print('PASS ZC8 actual RX/notification/credit/completion/service bodies and ownership/qualification retention; selected TX fences, restart, BA corrections')
# Compile the exact fence function under production optimization and inspect emitted asm.
with tempfile.TemporaryDirectory(prefix='zc19-source-') as t:
 p=Path(t);(p/'f.cpp').write_text(fn(s,'iwx_dma_publish_barrier')+'\nvoid publish(volatile unsigned*p,unsigned*d){*d=42;iwx_dma_publish_barrier();*p=1;}\n')
 subprocess.run(['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-target','x86_64-apple-macos','-O2','-S',str(p/'f.cpp'),'-o',str(p/'f.s')],check=True)
 a=(p/'f.s').read_text();assert 'mfence' in a or 'lock' in a;print('PASS actual fence emits x86_64 hardware ordering')
 runner=(R/'tools/ZC21_3x14_PORTABLE_IP_PROMPT.sh').read_text();assert not re.search(r'^\s*(?:/usr/sbin/)?system_profiler\s',runner,re.M)
 subprocess.run(['/bin/bash','-n',str(R/'tools/ZC21_3x14_PORTABLE_IP_PROMPT.sh')],check=True)

# Run the actual embedded benchmark guard, including sequential shell handoff.
subprocess.run([sys.executable,'-B',str(R/'tools/host_tests/ZC21_BENCHMARK_GUARD_TEST.py')],check=True)

# ZC21 only changes three queue transaction functions and two marker strings.
parent=subprocess.check_output(['git','-C',str(R),'show','251fb7b09bd17f7506f08029982545ce4d43e4f4:itlwm/hal_iwx/ItlIwx.cpp']).decode()
now=s.replace('AirportItlwm-Tahoe-B1-ZC21-PERF','AirportItlwm-Tahoe-B1-ZC19-PERF')
for n in ['iwx_tvqm_alloc_txq','iwx_tvqm_enable_txq','iwx_ba_task']:
 now=now.replace(fn(now,n),fn(parent,n))
assert now==parent,'Unexpected production changes outside transaction/identity scope'
# New marker uses the retained 19/1 schema, with unchanged strict health fields.
import importlib.util,plistlib,struct,json
spec=importlib.util.spec_from_file_location('guard',R/'tools/host_tests/ZC21_BENCHMARK_GUARD_TEST.py')
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
with tempfile.TemporaryDirectory(prefix='zc21-marker-') as td:
 p=Path(td);(p/'guard.py').write_text(mod.python_guard(runner))
 words=[0]*32;words[:8]=[19,1,256,10,123,2,1,2];words[11:13]=[1,2]
 (p/'i.plist').write_bytes(plistlib.dumps([{'KGP_ZC19':struct.pack('<32Q',*words),'KGP_RX_EXPERIMENT':'AirportItlwm-Tahoe-B1-ZC21-PERF'}]))
 subprocess.run([sys.executable,str(p/'guard.py'),str(p/'i.plist'),'NONE','NONE','mapped','ZC21','boundary'],check=True)
print('PASS exact ZC19 retention outside documented queue correction; ZC21 identity/schema guard')
