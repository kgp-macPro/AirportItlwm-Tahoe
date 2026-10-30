#!/usr/bin/env python3
"""Decode saved BZ watchdog snapshots; never accesses the live registry."""
import plistlib,struct,json,sys
from pathlib import Path
NAMES=['schema','version','bytes','publication','capture_ns','actual_mode','generation','reset_epoch','fault','error','hardware_owned','net80211_state','msix','refill_producer','published_producer','completion_consumer']
STATS=['irq_filter','irq_action','positive_master_stop','master_stop_failure','rx_credit_publications','nested_service_refused','bad_completion_or_fragment','claimed_rx','parsed_alive','rx_auth_frames','tx_without_queue','packet_publications','pnvm_complete','reserved13','reserved14','last_stopped_fault','last_stopped_error','tx_response_reclaims','ba_response_reclaims','rx_mpdu','reserved20','reserved21','reserved22','reserved23','reserved24','reserved25','reserved26','reserved27','reserved28','reserved29','reserved30','reserved31']
EVENTS={1:'first_fault',2:'mld_step',3:'scan_start',4:'scan_complete',5:'packet_tx_completion_before_RUN',6:'rx_authentication',7:'rx_association',8:'rx_disconnect',9:'init_complete',10:'rx_header_rejected',11:'lifecycle_stop_begin',12:'stale_state_cancelled',13:'scan_accepted_current_lifetime',14:'command_refused_inactive_firmware'}
def connection(raw):
    if len(raw)!=4160:raise ValueError('Wrong connection history length')
    w=struct.unpack('<520Q',raw)
    if w[:3]!=(201,1,4160) or w[6]!=64 or w[7]!=0:raise ValueError('Unknown connection history schema')
    total,count,overwritten=w[3:6]
    if count!=min(total,64) or overwritten!=max(total-64,0):raise ValueError('Inconsistent connection history counts')
    records=[]
    for i in range(64):
        r=w[8+i*8:16+i*8]
        if not r[0]:continue
        if not total-count<r[0]<=total or (r[0]-1)%64!=i:raise ValueError('Invalid connection event sequence')
        records.append(dict(zip(['serial','uptime_ns','generation','reset_epoch','event','a','b','c'],r)))
        records[-1]['name']=EVENTS.get(r[4],'unknown_event_'+str(r[4]))
    if len(records)!=count:raise ValueError('Missing connection event records')
    records.sort(key=lambda r:r['serial'])
    return {'schema':201,'version':1,'total':total,'retained':count,'overwritten':overwritten,'events':records,'raw_hex':raw.hex(),'limits':'Same-host uptime ns; control-stage observations,not over-air proof. Overwritten history is explicit. No keys,MACs,SSID or data payloads.'}
def decode(data):
    found=[];providers=[];histories=[]
    def walk(n):
        if isinstance(n,dict):
            candidate=n.get('KGP_BE200_CANDIDATE')
            if candidate or (n.get('vendor-id')==bytes.fromhex('86800000') and n.get('device-id')==bytes.fromhex('2b270000')):
                pci={}
                for k in ['vendor-id','device-id','subsystem-vendor-id','subsystem-id','revision-id']:
                    value=n.get(k)
                    if isinstance(value,bytes) and len(value)<=8:pci[k]='0x'+format(int.from_bytes(value,'little'),'04x')
                properties={k:v for k,v in n.items() if (k.startswith('KGP_BE200_') or k.startswith('ItlwmBoot') or k.startswith('ItlwmMld') or k.startswith('ItlwmPnvm') or k.startswith('ItlwmFirmware') or k.startswith('ItlwmScan') or k.startswith('ItlwmNvm')) and isinstance(v,(str,int,bool))}
                providers.append({'node':n.get('IORegistryEntryName'),'location':n.get('IORegistryEntryLocation'),'pci_identity':pci,'properties':properties,'note':'Boot-boundary metadata, not proof of successful attach or working IRQs. Injected model strings are not hardware identity.'})
            event_raw=n.get('KGP_BE200_CONNECTION')
            if isinstance(event_raw,bytes):histories.append(connection(event_raw))
            raw=n.get('KGP_BE200_TRANSPORT')
            if isinstance(raw,bytes):
                if len(raw)!=384:raise ValueError('Wrong BE200 snapshot length')
                w=struct.unpack('<48Q',raw)
                if w[:3]!=(200,1,384):raise ValueError('Unknown BE200 schema')
                found.append({'node':n.get('IORegistryEntryName'),'record':dict(zip(NAMES,w[:16])),'counters':dict(zip(STATS[:20]+['tx_qos_data','tx_nonqos_data','tx_data_on_management','tx_data_on_tid_queue','rx_hwdecrypt_drop','rx_duplicate_drop','rx_reorder_consumed_buffered_or_dropped','rx_direct_before_stack','rx_reorder_drop','rx_reorder_buffered','rx_crc_or_overrun_drop','rx_envelope_drop'] if candidate=='BE200-GL92-PRIVATE-P8' else STATS,w[16:])),'raw_hex':raw.hex(),'raw_qwords':list(w),'limits':'Saved boundary capture only; source counters, not over-air or end-to-end completion proof.'})
            for v in n.values():walk(v)
        elif isinstance(n,list):
            for v in n:walk(v)
    walk(data)
    return {'connection_histories':histories,'snapshots':found,'providers':providers,'status':'FOUND'if found else 'NO_BZ_WATCHDOG_RECORD','note':'Absence can reflect attach failure or capture before watchdog. Inspect stage/error/loaded identity and original IORegistry, not zeros invented by decoder.'}
if __name__=='__main__':
    print(json.dumps(decode(plistlib.loads(Path(sys.argv[1]).read_bytes())),indent=2))
