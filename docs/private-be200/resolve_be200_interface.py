#!/usr/bin/env python3
"""Resolve a SAVED IOService tree, never touches the system or guesses names."""
import argparse,json,plistlib,re,sys
from pathlib import Path
EXPECTED='BE200-GL92-PRIVATE-P7'
def pci(n,k):
 v=n.get(k)
 if isinstance(v,bytes)and 0<len(v)<=8:return int.from_bytes(v,'little')
 if isinstance(v,int)and not isinstance(v,bool):return v
 return None
def segment(n):
 name=n.get('IORegistryEntryName','?');loc=n.get('IORegistryEntryLocation')
 return name+('@'+str(loc)if loc and '@'not in name else'')
def resolve(data,iface='',provider='',expected=EXPECTED,previous=None):
 matches=[];seen_candidates=[]
 def walk(n,path,ancestors):
  if not isinstance(n,dict):return
  path=path+[segment(n)];ancestors=ancestors+[n]
  if n.get('IOObjectClass')=='IOPCIDevice'and n.get('KGP_BE200_CANDIDATE')==expected:
   seen_candidates.append({'path':'IOService:/'+('/'.join(path)),'vendor':pci(n,'vendor-id'),'device':pci(n,'device-id')})
   if pci(n,'vendor-id')==0x8086 and pci(n,'device-id')==0x272b:
    for driver in n.get('IORegistryEntryChildren',[]):
     if not isinstance(driver,dict)or driver.get('IOObjectClass')!='AirportItlwm':continue
     for wi in driver.get('IORegistryEntryChildren',[]):
      if not isinstance(wi,dict)or wi.get('IOObjectClass')!='AirportItlwmInterface':continue
      name=wi.get('BSD Name')
      if not isinstance(name,str)or not re.fullmatch(r'en[0-9]+',name):continue
      # A primary AirportItlwmInterface is distinct from AWDL/P2P virtual classes.
      # A BSD name alone, MAC/model label, or network-service list is insufficient.
      ids=[x.get('IORegistryEntryID')for x in [n,driver,wi]]
      if any(not isinstance(i,int)or isinstance(i,bool)for i in ids):continue
      matches.append((n,{'status':'UNIQUE_BOUND_INTERFACE','expected_candidate':expected,
       'candidate_property':n['KGP_BE200_CANDIDATE'],'rx_experiment_marker':n.get('KGP_RX_EXPERIMENT'),
       'provider_name':n.get('IORegistryEntryName'),'provider_path':'IOService:/'+('/'.join(path)),
       'provider_class':'IOPCIDevice','provider_entry_id':ids[0],
       'pci':{k:('0x'+format(pci(n,k),'04x')if pci(n,k)is not None else None)for k in ['vendor-id','device-id','subsystem-vendor-id','subsystem-id','revision-id']},
       'driver_name':driver.get('IORegistryEntryName'),'driver_class':driver.get('IOObjectClass'),
       'driver_entry_id':ids[1],'driver_path':'IOService:/'+('/'.join(path+[segment(driver)])),
       'interface_class':wi.get('IOObjectClass'),'interface_entry_id':ids[2],
       'interface_path':'IOService:/'+('/'.join(path+[segment(driver),segment(wi)])),
       'wifi_iface':name,'requested_WIFI_IFACE':iface or None,'requested_IOREG_WIFI_SERVICE':provider or None,
       'identity_limit':'Registry binding+authentic PCI ID+driver marker, not independent executable verification. Inspect saved LOADED.txt UUID against BUILD_IDENTITY.'}))
  for c in n.get('IORegistryEntryChildren',[]):walk(c,path,ancestors)
 roots=data if isinstance(data,list)else[data]
 for n in roots:walk(n,[],[])
 if len(matches)!=1:raise ValueError(f'Expected ONE {expected} 8086:272b -> AirportItlwm -> primary AirportItlwmInterface binding; found {len(matches)}. Candidate providers: {seen_candidates}. No fallback interface selected.')
 subtree,record=matches[0]
 if iface and iface!=record['wifi_iface']:raise ValueError(f'WIFI_IFACE={iface} does not match bound infrastructure interface {record["wifi_iface"]}. No capture started.')
 if provider and provider not in [record['provider_name'],record['provider_path']]:raise ValueError(f'IOREG_WIFI_SERVICE={provider} does not match bound PCI provider {record["provider_name"]} ({record["provider_path"]}). No capture started.')
 if previous:
  for k in ['provider_entry_id','driver_entry_id','interface_entry_id','provider_path','wifi_iface','expected_candidate']:
   if previous.get(k)!=record[k]:raise ValueError(f'Binding/lifetime changed during collection: {k}. Preserve both snapshots; do not combine packet evidence.')
 return record,subtree
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('input');p.add_argument('record');p.add_argument('selected_plist');p.add_argument('--iface',default='');p.add_argument('--provider',default='');p.add_argument('--previous');a=p.parse_args()
 try:
  data=plistlib.loads(Path(a.input).read_bytes());prev=json.loads(Path(a.previous).read_text())if a.previous else None
  record,subtree=resolve(data,a.iface,a.provider,previous=prev)
  Path(a.record).write_text(json.dumps(record,indent=2)+'\n');Path(a.selected_plist).write_bytes(plistlib.dumps([subtree]));print(record['wifi_iface']);return 0
 except (ValueError,TypeError,KeyError,OSError,plistlib.InvalidFileException)as e:
  Path(a.record).write_text(json.dumps({'status':'ABORT_NO_UNIQUE_BINDING','error':str(e),'requested_WIFI_IFACE':a.iface,'requested_IOREG_WIFI_SERVICE':a.provider},indent=2)+'\n')
  print('STOP: '+str(e),file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
