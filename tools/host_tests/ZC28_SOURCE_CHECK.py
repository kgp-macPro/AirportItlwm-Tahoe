#!/usr/bin/env python3
"""Exact ZC26 preservation outside bounded console/observation changes."""
from pathlib import Path
import subprocess,re,hashlib,os,sys
R=Path(__file__).resolve().parents[2];BASE='48e9a31e68c328b5f19e5bdc81901e900d350738'
def mask(s):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s,flags=re.S)
def compact(s):return re.sub(r'\s+','',mask(s))
def span(s,n):
 m=mask(s);q=re.search(r'(?:static\s+)?(?:inline\s+)?(?:void|bool|int|IOReturn|uint64_t|uint32_t|unsigned)\s+(?:ItlIwx::\s*)?'+n+r'\s*\(',m);assert q,n
 a=m.index('{',q.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return q.start(),b
def fn(s,n):a,b=span(s,n);return s[a:b]
def old(n):return subprocess.check_output(['git','-C',str(R),'show',BASE+':'+n],text=True)
cpp='itlwm/hal_iwx/ItlIwx.cpp';s=(R/cpp).read_text();base=old(cpp)
changed=['iwx_rx_tx_cmd_single','iwx_rx_tx_cmd','dexterOriginal_iwx_rx_tx_cmd','iwx_rs_update','dexterOriginal_iwx_rx_addbuf','iwx_notif_intr','dexterOriginal_iwx_notif_intr','iwx_tx','dexterOriginal_iwx_tx','iwx_txd_done','iwx_rx_tx_ba_notif','zc19Snapshot','zc19Publish','_iwx_start_task']
# Ownership/control flow in all packet/retirement/notification helpers is exact26
# after removing PURE observation statements and replacing the bounded log sink.
for n in ['iwx_tx','dexterOriginal_iwx_tx','iwx_txd_done','iwx_rx_tx_ba_notif']:
 f=fn(s,n);f=re.sub(r'^.*__atomic_fetch_add\(&fZc27Stats\[[^\n]*\n','',f,flags=re.M)
 f=f.replace('    zc27BaStatus(pkt);\n','')
 assert compact(f)==compact(fn(base,n)),n+' packet behavior changed'
for n in ['iwx_notif_intr','dexterOriginal_iwx_notif_intr']:
 f=fn(s,n)
 f=re.sub(r'^.*(?:const uint64_t (?:rx27|failed\w*27)|zc27RxEnd\()[^\n]*\n','',f,flags=re.M)
 assert compact(f)==compact(fn(base,n)),n+' credit/handoff/control behavior changed'
for n in ['iwx_rx_tx_cmd','dexterOriginal_iwx_rx_tx_cmd']:
 f=fn(base,n)
 a=f.index('        for (int i = 0; i < tx_resp->frame_count; i++) {');b=f.index('\n    } else {',a)
 f=f[:a]+'        zc28Aggregate(pkt);'+f[b:]
 assert compact(f)==compact(fn(s,n)),n+' completion side effects changed'
f=fn(base,'iwx_rx_tx_cmd_single');a=f.index('        XYLog(');b=f.index('\n',a)
f=f[:a]+'        zc28Note(3,unsigned(status),unsigned(txd->type),0);'+f[b:]
assert compact(f)==compact(fn(s,'iwx_rx_tx_cmd_single'))
f=fn(s,'dexterOriginal_iwx_rx_addbuf').replace('        zc28Note(7,ENOMEM,unsigned(idx),0);','        XYLog("could not allocate RX mbuf\\n");').replace('        zc28Note(8,ENOMEM,unsigned(idx),0);\n','')
assert compact(f)==compact(fn(base,'dexterOriginal_iwx_rx_addbuf'))
rate=fn(s,'iwx_rs_update');assert 'snprintf' not in rate and 'pretty' not in rate and 'XYLog' not in rate
assert compact(rate[rate.index('    format ='):])==compact(fn(base,'iwx_rs_update')[fn(base,'iwx_rs_update').index('    format ='):])
assert compact(fn(s,'iwx_new_rate_from_v1'))==compact(fn(base,'iwx_new_rate_from_v1'))
# Reinsert only explicitly removed format-only blocks and reviewed method bodies;
# every other production token must reconstruct frozen26.
now=s.replace('AirportItlwm-Tahoe-B1-ZC28-SERVICE','AirportItlwm-Tahoe-B1-ZC26-PERF')
a=now.index('// ZC28: rare former console');b=now.index('// ZC19 independent minimal health publication',a);now=now[:a]+now[b:]
for n in changed:
 a,b=span(now,n);now=now[:a]+fn(base,n)+now[b:]
for start,end,anchor in [('struct iwx_rate_mcs_info {','#define IWX_MCS_INDEX_PER_STREAM','#define IWX_MCS_INDEX_PER_STREAM'),('static const char *iwx_rs_pretty_ant(','static uint8_t iwx_rs_extract_rate(','static uint8_t iwx_rs_extract_rate(')]:
 a=base.index(start);b=base.index(end,a);p=now.index(anchor);now=now[:p]+base[a:b]+now[p:]
a=base.index('static int iwx_rs_pretty_print_rate_v1(');b=base.index('#if NBPFILTER > 0',a)
p=now.index('#if NBPFILTER > 0',now.index('static int iwx_hwrate_to_plcp_idx('));now=now[:p]+base[a:b]+now[p:]
a=base.index('#define IWX_AGG_TX_STATE_(');b=base.index('\nvoid ItlIwx::\niwx_rx_tx_cmd',a);p=now.index('\nvoid ItlIwx::\niwx_rx_tx_cmd(');now=now[:p]+base[a:b]+now[p:]
assert compact(now)==compact(base),'unexpected production changes beyond reviewed spans'
h='itlwm/hal_iwx/ItlIwx.hpp';header=(R/h).read_text();a=header.index('    // ZC28 fixed watchdog-owned wire');b=header.index('    // PC1 control',a)
assert compact(header[:a]+header[b:])==compact(old(h))
f=fn(s,'_iwx_start_task');a=f.index('    auto *serviceIfp=');b=f.index('    // ZC8:',a)
assert compact(f[:a]+f[b:])==compact(fn(base,'_iwx_start_task'))
assert compact(fn(s,'dexterOriginal__iwx_start_task'))==compact(fn(base,'dexterOriginal__iwx_start_task'))
assert compact(fn(s,'iwx_clear_oactive'))==compact(fn(base,'iwx_clear_oactive'))
# No new normal TX/BA/release/notification observer hooks or clocks.
for n in ['iwx_tx','dexterOriginal_iwx_tx','iwx_txd_done','iwx_rx_tx_ba_notif','iwx_notif_intr','dexterOriginal_iwx_notif_intr']:
 assert compact(fn(s,n))==compact(fn(base,n)),n
assert 'fZc27' not in s and 'zc27' not in s
q=(R/'itl80211/openbsd/sys/_ifq.cpp').read_text()
assert 'serviceDepth' in q and '__atomic_' not in q and 'time2GNow' not in q
assert 'ifq_begin_service' not in (R/'AirportItlwm/AirportItlwm.cpp').read_text()
# No barrier/mapper/command/restart/BA-state change is hidden in the observer.
for n in ['iwx_tx_publish_control','iwx_dma_publish_barrier','iwx_dma_queue_publish_barrier','iwx_tx_control_alloc','iwx_send_cmd','iwx_ba_task','iwx_rx_reorder','iwx_sta_rx_agg','iwx_stop','zc16ArmReorder']:
 assert compact(fn(s,n))==compact(fn(base,n)),n
assert s.count('iwx_dma_publish_barrier();')==2 and s.count('iwx_dma_queue_publish_barrier();')==2
assert 'sfence' not in mask(fn(s,'iwx_dma_publish_barrier')).lower()
exceptions={cpp,h,'itl80211/openbsd/sys/_ifq.cpp','itl80211/openbsd/sys/_ifq.h','tools/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh','tools/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh'};count=0
for e in subprocess.check_output(['git','-C',str(R),'ls-tree','-rz',BASE]).split(b'\0'):
 if not e:continue
 meta,n=e.split(b'\t');n=n.decode()
 if n in exceptions:continue
 mode,kind,blob=meta.decode().split();data=os.readlink(R/n).encode() if mode=='120000' else (R/n).read_bytes()
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob,n
 count+=1
for test in ['SHARED_R7_TEST.py','ZC28_RX_DECODER_TEST.py','ZC28_SERVICE_DECODER_TEST.py']:
 subprocess.run([sys.executable,'-B',str(R/'tools/host_tests'/test)],check=True)
print('PASS ZC28: exact26packet/credit/retirement/ownership; finitecapacityhandoff independently modeled; raresinks only; ',count,'existingfilesunchanged; strictsharedprotocol/guards/decoders')
