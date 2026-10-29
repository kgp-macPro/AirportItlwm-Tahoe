#!/usr/bin/env python3
"""Decode saved BZ watchdog snapshots; never accesses the live registry."""
import plistlib,struct,json,sys
from pathlib import Path
NAMES=['schema','version','bytes','publication','capture_ns','actual_mode','generation','reset_epoch','fault','error','hardware_owned','net80211_state','msix','refill_producer','published_producer','completion_consumer']
STATS=['irq_filter','irq_action','positive_master_stop','master_stop_failure','rx_credit_publications','nested_service_refused','bad_completion_or_fragment','claimed_rx','parsed_alive','rx_auth_frames','tx_without_queue','packet_publications','pnvm_complete','reserved13','reserved14','last_stopped_fault','last_stopped_error','tx_response_reclaims','ba_response_reclaims','rx_mpdu','reserved20','reserved21','reserved22','reserved23','reserved24','reserved25','reserved26','reserved27','reserved28','reserved29','reserved30','reserved31']
def decode(data):
    found=[];providers=[]
    def walk(n):
        if isinstance(n,dict):
            candidate=n.get('KGP_BE200_CANDIDATE')
            if candidate or (n.get('vendor-id')==bytes.fromhex('86800000') and n.get('device-id')==bytes.fromhex('2b270000')):
                pci={}
                for k in ['vendor-id','device-id','subsystem-vendor-id','subsystem-id','revision-id']:
                    value=n.get(k)
                    if isinstance(value,bytes) and len(value)<=8:pci[k]='0x'+format(int.from_bytes(value,'little'),'04x')
                properties={k:v for k,v in n.items() if (k.startswith('KGP_BE200_') or k.startswith('ItlwmBoot') or k.startswith('ItlwmMld')) and isinstance(v,(str,int,bool))}
                providers.append({'node':n.get('IORegistryEntryName'),'location':n.get('IORegistryEntryLocation'),'pci_identity':pci,'properties':properties,'note':'Boot-boundary metadata, not proof of successful attach or working IRQs. Injected model strings are not hardware identity.'})
            raw=n.get('KGP_BE200_TRANSPORT')
            if isinstance(raw,bytes):
                if len(raw)!=384:raise ValueError('Wrong BE200 snapshot length')
                w=struct.unpack('<48Q',raw)
                if w[:3]!=(200,1,384):raise ValueError('Unknown BE200 schema')
                found.append({'node':n.get('IORegistryEntryName'),'record':dict(zip(NAMES,w[:16])),'counters':dict(zip(STATS,w[16:])),'raw_hex':raw.hex(),'raw_qwords':list(w),'limits':'Saved boundary capture only; source counters, not over-air or end-to-end completion proof.'})
            for v in n.values():walk(v)
        elif isinstance(n,list):
            for v in n:walk(v)
    walk(data)
    return {'snapshots':found,'providers':providers,'status':'FOUND'if found else 'NO_BZ_WATCHDOG_RECORD','note':'Absence can reflect attach failure or capture before watchdog. Inspect stage/error/loaded identity and original IORegistry, not zeros invented by decoder.'}
if __name__=='__main__':
    print(json.dumps(decode(plistlib.loads(Path(sys.argv[1]).read_bytes())),indent=2))
