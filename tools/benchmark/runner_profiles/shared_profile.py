#!/usr/bin/env python3
"""Offline parsers for the shared runner. No system calls, settings or traffic."""
from pathlib import Path
import hashlib,json,plistlib,re,sys,importlib.util
ROOT=Path(__file__).resolve().parent

def load_profiles():return json.loads((ROOT/'profiles.json').read_text())
def profile(label):
    data=load_profiles()
    if label not in data:raise ValueError('No supported frozen candidate profile: '+label)
    return data[label]
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def integer(v):
    if isinstance(v,bytes) and 0<len(v)<=8:return int.from_bytes(v,'little')
    if isinstance(v,int) and not isinstance(v,bool):return v
    return None
def segment(n):
    name=n.get('IORegistryEntryName','?');loc=n.get('IORegistryEntryLocation')
    return name+('@'+str(loc) if loc and '@' not in name else '')
def save(p,data):Path(p).write_text(json.dumps(data,indent=2)+'\n')
def resolve(tree,info,kind,label,iface='',provider='',prior=None):
    bundle=info.get('CFBundleIdentifier');classes={p.get('IOClass') for p in info.get('IOKitPersonalities',{}).values() if isinstance(p,dict)}
    if not isinstance(bundle,str) or not classes:raise ValueError('Candidate has no bundle identifier/IOKit controller classes')
    candidates=[]
    def walk(n,chain,path):
        if not isinstance(n,dict):return
        chain=chain+[n];path=path+[segment(n)]
        bsd=n.get('BSD Name');cls=n.get('IOObjectClass','')
        if isinstance(bsd,str) and re.fullmatch(r'en[0-9]+',bsd) and 'Interface' in cls and 'P2P' not in cls and 'AWDL' not in cls:
            drivers=[(i,x) for i,x in enumerate(chain[:-1]) if x.get('IOObjectClass') in classes or x.get('IOObjectClass')=='AirportItlwm']
            if drivers:
                di,driver=drivers[-1]
                pci=[(i,x) for i,x in enumerate(chain[:di]) if x.get('IOObjectClass')=='IOPCIDevice' and integer(x.get('vendor-id')) is not None and integer(x.get('device-id')) is not None]
                if pci:
                    pi,p=pci[-1]
                    # Binding is independent of DEVICE_LABEL; metadata identifies only the candidate controller.
                    owner=driver.get('CFBundleIdentifier') or driver.get('IOPersonalityPublisher')
                    if integer(p['vendor-id'])!=0x8086:return
                    if owner is not None and owner!=bundle:return
                    if kind=='external' and label.startswith('LAOBAMAC') and integer(p['device-id'])==0x272b:
                        raise ValueError('The retained laobamac comparison builds do not support BE200; use an own BE200 Alpha profile')
                    ids=[p.get('IORegistryEntryID'),driver.get('IORegistryEntryID'),n.get('IORegistryEntryID')]
                    if any(not isinstance(x,int) or isinstance(x,bool) or x<=0 for x in ids):raise ValueError('Binding has missing/invalid registry entry IDs')
                    if kind=='kgp' and label in load_profiles():
                        spec=profile(label)
                        if integer(p['device-id']) not in spec['pci_devices']:return
                    if kind=='be200':
                        spec=profile(label)
                        if integer(p['device-id'])!=spec['pci_device'] or p.get('KGP_BE200_CANDIDATE')!=spec['registry_candidate']:return
                        if integer(p.get('KGP_BE200_PCI_ID'))!=((spec['pci_device']<<16)|spec['pci_vendor']):raise ValueError('BE200 config-space PCI diagnostic missing or disagrees with provider identity')
                    provider_path='IOService:/'+('/'.join(path[:pi+1]));driver_suffix='/'.join(path[pi+1:di+1]);iface_suffix='/'.join(path[pi+1:])
                    # Subsequent selected-provider snapshots omit outer ancestors. Preserve the
                    # discovered path ONLY after checking the exact provider/controller/interface IDs.
                    if prior and pi==0 and ids[0]==prior.get('provider_entry_id'):provider_path=prior['provider_path']
                    record={'status':'VERIFIED_BOUND_INFRASTRUCTURE_INTERFACE','validation_profile':kind,'candidate_label':label,'bundle_identifier':bundle,'provider_name':p.get('IORegistryEntryName'),'provider_path':provider_path,'provider_entry_id':ids[0],'pci':{k:('0x'+format(integer(p.get(k)),'04x') if integer(p.get(k)) is not None else None) for k in ['vendor-id','device-id','subsystem-vendor-id','subsystem-id','revision-id']},'config_space_pci_id':integer(p.get('KGP_BE200_PCI_ID')) if kind=='be200' else None,'config_space_pci_status':'DRIVER_CAPTURED_CONFIG_READ' if kind=='be200' else 'UNAVAILABLE_IOREGISTRY_PCI_PROPERTIES_ONLY','driver_name':driver.get('IORegistryEntryName'),'driver_class':driver.get('IOObjectClass'),'driver_path':provider_path+'/'+driver_suffix,'driver_entry_id':ids[1],'interface_name':bsd,'interface_class':cls,'interface_path':provider_path+'/'+iface_suffix,'interface_entry_id':ids[2],'candidate_marker':p.get('KGP_BE200_CANDIDATE') if kind=='be200' else None,'device_label_is_identification':False,'limitations':'IORegistry PCI properties/controller binding plus loaded candidate identity are checked separately. Retail labels, ACPI names and AppleVTD activity alone are not hardware/DMA qualification.'}
                    candidates.append((record,p))
        for c in n.get('IORegistryEntryChildren',[]):walk(c,chain,path)
    for r in tree if isinstance(tree,list) else [tree]:walk(r,[],[])
    if iface:candidates=[x for x in candidates if x[0]['interface_name']==iface]
    if provider:candidates=[x for x in candidates if provider in (x[0]['provider_name'],x[0]['provider_path'])]
    if len(candidates)!=1:raise ValueError('Expected exactly one matching PCI -> candidate controller -> infrastructure interface binding; found '+str(len(candidates))+'. Check WIFI_IFACE/IOREG_WIFI_SERVICE; no fallback selected.')
    r,node=candidates[0]
    if prior:
        for k in ['provider_entry_id','driver_entry_id','interface_entry_id','provider_name','provider_path','interface_name','driver_class','pci','bundle_identifier']:
            if r[k]!=prior.get(k):raise ValueError('Device/interface binding changed during campaign: '+k)
    return r,node

def identity(loaded,exe,info_path,uuid,bundle,exe_sha,info_sha,kind,label,commit,out):
    info=plistlib.loads(Path(info_path).read_bytes())
    records=[s for s in Path(loaded).read_text(errors='replace').splitlines() if bundle in s.split()]
    if len(records)!=1:raise ValueError('Candidate bundle requires exactly one loaded record')
    uuids=re.findall(r'(?<![A-Fa-f0-9])[A-Fa-f0-9]{8}(?:-[A-Fa-f0-9]{4}){3}-[A-Fa-f0-9]{12}(?![A-Fa-f0-9])',records[0])
    if len(uuids)!=1 or uuids[0].upper()!=uuid.upper():raise ValueError('Loaded UUID does not match selected candidate')
    if info.get('CFBundleIdentifier')!=bundle or info.get('CFBundleExecutable')!='AirportItlwm':raise ValueError('Candidate bundle metadata mismatch')
    actual=digest(exe);actual_info=digest(info_path)
    if actual!=exe_sha or actual_info!=info_sha:raise ValueError('Candidate executable/Info.plist changed during campaign')
    pinned = kind=='be200' or (kind=='kgp' and label in load_profiles())
    if pinned:
        spec=profile(label)
        if uuid.upper()!=spec['uuid'] or actual!=spec['executable_sha256'] or actual_info!=spec['info_plist_sha256']:raise ValueError('Wrong frozen '+label+' candidate UUID/SHA256/Info.plist')
    r={'bundle_id':bundle,'loaded_uuid':uuid.upper(),'loaded_record':records[0],'candidate_hashes_unchanged':True,'executable_sha256':actual,'info_plist_sha256':actual_info,'validation_profile':kind,'candidate_label':label,'declared_source_commit':commit.lower() if kind=='external' else None,'source_build_correspondence_verified':False,'binary_identity_scope':('FROZEN_UUID_AND_HASH_PROFILE' if pinned else 'CANDIDATE_DISK_UUID_AND_MARKER_GUARD'),'referenced_binary_build_commit':profile(label)['binary_build_commit'] if pinned else None}
    save(out,r)

def be200_health(src,health_path,initial,previous,phase,label):
    spec=profile(label);p=Path(src);output=Path(str(p).rsplit('.',1)[0]+'.be200.json')
    try:
        modspec=importlib.util.spec_from_file_location('be200_decode',ROOT/'decode_be200.py');mod=importlib.util.module_from_spec(modspec);modspec.loader.exec_module(mod)
        data=plistlib.loads(p.read_bytes());decoded=mod.decode(data);save(output,decoded)
        if len(decoded['snapshots'])!=1 or len(decoded['providers'])!=1:raise ValueError('Required BE200 transport diagnostic UNAVAILABLE or ambiguous')
        props=decoded['providers'][0]['properties']
        if props.get('KGP_BE200_CANDIDATE')!=spec['registry_candidate'] or (data[0] if isinstance(data,list) and len(data)==1 else {}).get('KGP_RX_EXPERIMENT')!=spec['marker']:raise ValueError('Wrong BE200 candidate marker')
        snap=decoded['snapshots'][0];r=snap['record'];c=snap['counters'];h=json.loads(Path(health_path).read_text())
        for k in ['Mode','Generation','Pc1Reset']:
            field={'Mode':'actual_mode','Generation':'generation','Pc1Reset':'reset_epoch'}[k]
            if h[k]!=r[field]:raise ValueError('BE200/ZC19 diagnostics disagree: '+k)
        if not r['publication'] or not r['capture_ns']:raise ValueError('Unpublished BE200 watchdog record')
        if r['fault'] or r['error'] or r['hardware_owned']!=1 or r['net80211_state']!=4 or r['msix']!=1:raise ValueError('BE200 fault/inactive hardware/non-RUN/MSI-X state')
        # An old cumulative counter is not silently reset or called zero. New safety
        # errors within the accepted campaign do reject completion.
        bz={'record':r,'counters':c,'connection_history_status':'AVAILABLE' if decoded['connection_histories'] else 'UNAVAILABLE_NOT_ZERO','connection_histories':decoded['connection_histories'],'numeric_transport_health_available':True,'publication_progress_scope':'Boundary observations only, not continuous watchdog proof'}
        if initial!='NONE':
            old=json.loads(Path(initial).read_text())['BE200']
            if any(old['record'][k]!=r[k] for k in ['actual_mode','generation','reset_epoch']):raise ValueError('BE200 DMA mode/generation/reset epoch changed')
        if previous!='NONE':
            last=json.loads(Path(previous).read_text())['BE200']
            if r['publication']<last['record']['publication'] or r['capture_ns']<last['record']['capture_ns']:raise ValueError('BE200 watchdog counter/time rolled back')
            for k in ['master_stop_failure','bad_completion_or_fragment','tx_without_queue']:
                if c[k]!=last['counters'][k]:raise ValueError('BE200 cumulative safety counter changed: '+k)
        if phase=='final':
            if initial=='NONE' or previous=='NONE':raise ValueError('Missing BE200 campaign references')
            if r['publication']<=old['record']['publication']:raise ValueError('BE200 publication did not advance over complete campaign')
        elif phase!='boundary':raise ValueError('Unknown BE200 health phase')
        h.update(ValidationProfile='be200',Candidate=label,BE200=bz);save(health_path,h)
        print(label+' BE200 transport + inherited ZC19 health verified; history '+bz['connection_history_status'])
    except Exception as e:
        save(Path(str(p).rsplit('.',1)[0]+'.be200-check-error.json'),{'status':'REJECTED','error':str(e),'missing_is_not_zero':True});raise

def summary(folder):
    root=Path(folder);rows=[]
    for p in root.rglob('*.health.json'):
        h=json.loads(p.read_text())
        if 'BE200' in h:rows.append({'file':str(p.relative_to(root)),**h['BE200']})
    rows.sort(key=lambda x:x['record']['capture_ns'])
    out={'status':'SAVED_BOUNDARY_DIAGNOSTICS_NOT_CONTINUOUS' if rows else 'UNAVAILABLE_NO_ACCEPTED_BE200_DIAGNOSTICS','accepted_boundaries':len(rows),'boundaries':rows,'rejections':[{'file':str(p.relative_to(root)),**json.loads(p.read_text())} for p in root.rglob('*.be200-check-error.json')],'limits':'No new sampling or traffic. Missing records are unavailable, not zero. Bounded connection histories may overwrite events. A clean boundary is not proof that every transient was absent.'}
    if rows:out['counter_deltas']={k:rows[-1]['counters'][k]-rows[0]['counters'][k] for k in rows[0]['counters']}
    save(root/'BE200_CAMPAIGN_DIAGNOSTICS.json',out)

def main():
    cmd,*a=sys.argv[1:]
    if cmd=='resolve':
        source,info,out,selected,kind,label,iface,provider,prior=a
        try:
            old=json.loads(Path(prior).read_text()) if prior!='NONE' else None
            r,node=resolve(plistlib.loads(Path(source).read_bytes()),plistlib.loads(Path(info).read_bytes()),kind,label,iface,provider,old)
            save(out,r);Path(selected).write_bytes(plistlib.dumps([node]));print(r['interface_name'])
        except Exception as e:save(out,{'status':'REJECTED_NO_UNIQUE_BINDING','error':str(e),'no_fallback':True});raise
    elif cmd=='identity':identity(*a)
    elif cmd=='be200-health':be200_health(*a)
    elif cmd=='summary':summary(*a)
    else:raise ValueError('Unknown runner-profile command')
if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,TypeError,OSError,plistlib.InvalidFileException) as e:print('STOP: '+str(e),file=sys.stderr);sys.exit(1)
