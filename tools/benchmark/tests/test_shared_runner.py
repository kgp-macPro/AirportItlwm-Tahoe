#!/usr/bin/env python3
"""Execute actual runner/profile code OFFLINE; OS/network tools are stubbed."""
from pathlib import Path
import unittest,tempfile,subprocess,json,plistlib,struct,hashlib,copy,os,sys,re,importlib.util,zipfile,shlex,pty,select,time,errno
B=Path(__file__).resolve().parents[1]
NEW=B/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh';OLD=B/'reference/AirportItlwm_3x14_PORTABLE_IP_PROMPT.r7-service.sh'
spec=importlib.util.spec_from_file_location('profile',B/'runner_profiles/shared_profile.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
FIX=plistlib.loads((B/'tests/fixtures/P7_BOUNDARY.plist').read_bytes())
P8=Path(os.environ.get('BE200_P8_TEST_KEXT','/nonexistent/private-P8'))
P7=Path(os.environ.get('BE200_TEST_KEXT',str(Path.home()/'Desktop/AirportItlwm/BE200-EXPERIMENTAL/AirportItlwm.kext')))
INFO=plistlib.loads((P7/'Contents/Info.plist').read_bytes()) if P7.exists() else {'CFBundleIdentifier':'com.zxystd.AirportItlwm','CFBundleExecutable':'AirportItlwm','IOKitPersonalities':{'itlwm':{'IOClass':'AirportItlwm'}}}
UUID='11111111-2222-3333-4444-555555555555';COUNTS={'full_mock_campaigns':0,'legacy_frontends':0,'guard_comparisons':0}
def modified(iface='en2',parent='ARPT'):
 d=copy.deepcopy(FIX);d[0]['IORegistryEntryName']=parent;driver=d[0]['IORegistryEntryChildren'][0];wi=driver['IORegistryEntryChildren'][0];wi['IORegistryEntryName']=wi['BSD Name']=iface;return d

def terminal_run(command,env,answer):
    pid,fd=pty.fork()
    if pid==0:os.execvpe(command[0],command,env)
    chunks=[];deadline=time.monotonic()+90
    try:
        os.write(fd,answer.encode())
        while time.monotonic()<deadline:
            ready,_,_=select.select([fd],[],[],0.1)
            if ready:
                try:
                    chunk=os.read(fd,65536)
                    if chunk:chunks.append(chunk)
                    else:break
                except OSError as e:
                    if e.errno==errno.EIO:break
                    raise
            ended,status=os.waitpid(pid,os.WNOHANG)
            if ended:
                # Drain pending terminal output after process exit.
                try:
                    while True:
                        chunk=os.read(fd,65536)
                        if not chunk:break
                        chunks.append(chunk)
                except OSError:pass
                return subprocess.CompletedProcess(command,os.waitstatus_to_exitcode(status),b''.join(chunks).decode(errors='replace'),'')
        ended,status=os.waitpid(pid,os.WNOHANG)
        if not ended:
            os.kill(pid,9);os.waitpid(pid,0);raise TimeoutError('Offline interactive runner timeout')
        return subprocess.CompletedProcess(command,os.waitstatus_to_exitcode(status),b''.join(chunks).decode(errors='replace'),'')
    finally:os.close(fd)

def guard_code(script):
 s=script.read_text().split('check_snapshot() {',1)[1].split('\nexternal_identity_snapshot()',1)[0]
 return s.split("<<'PY'\n",1)[1].split('\nPY\n',1)[0]

def node_for(label,mode,iface='en9',parent='CNVW'):
 d=modified(iface,parent);p=d[0]
 for k in list(p):
  if k.startswith('KGP_'):p.pop(k)
 if label=='EXTERNAL':
  p['device-id']=(0x2725).to_bytes(4,'little');return d
 p['KGP_RX_EXPERIMENT']=('AirportItlwm-Tahoe-B1-ZC28-SERVICE' if label=='ZC28' else 'AirportItlwm-Tahoe-B1-ZC27-STABILITY' if label=='ZC27' else 'AirportItlwm-Tahoe-B1-ZC20-TX-CMD-UC' if label=='ZC20' else 'AirportItlwm-Tahoe-B1-'+label+'-PERF')
 p['device-id']=(0x2725).to_bytes(4,'little')
 if label=='ZC8':
  p.update(KGP_ZC5_DMA_MODE='mapped RX/TX' if mode=='mapped' else 'original Dexter RX/TX',KGP_PC1=bytes(80))
  if mode=='mapped':p['KGP_TX_2I']=bytes(128)
 elif label in ['ZC13','ZC18']:
  w=[0]*72;w[:3]=[10,2,10912];w[5]=2 if mode=='mapped' else 1;w[6]=19;w[23]=10;w[63]=10;p['KGP_ZC10']=struct.pack('<72Q',*w)+bytes(10912-576)
 else:
  w=list(struct.unpack('<32Q',FIX[0]['KGP_ZC19']));w[5]=2 if mode=='mapped' else 1;p['KGP_ZC19']=struct.pack('<32Q',*w)
 return d

MOCK=r'''#!/usr/bin/env python3
from pathlib import Path
import os,sys,json,plistlib,struct,zipfile,copy
name=Path(sys.argv[0]).name;args=sys.argv[1:];state=Path(os.environ['MOCK_STATE']);iface=os.environ['MOCK_IFACE'];parent=os.environ['MOCK_PARENT'];scene=os.environ.get('MOCK_SCENE','normal')
if name=='iperf3':
 if '--version' in args:print('iperf3 SIMULATED offline');sys.exit(0)
 with (state/'iperf-calls.jsonl').open('a')as f:f.write(json.dumps(args)+'\n')
 if scene=='iperf-error':print('{"error":"SIMULATED failure"}');print('SIMULATED error',file=sys.stderr);sys.exit(1)
 udp='-u'in args;rev='-R'in args;secs=int(args[args.index('-t')+1]);rate=int(args[args.index('-b')+1][:-1]) if udp else 480
 sent=min(rate,450)*1e6 if udp else 550e6;recv=sent*0.99;end={'sum_sent':{'bits_per_second':sent,'seconds':secs,'retransmits':7},'sum_received':{'bits_per_second':recv,'seconds':secs}}
 if udp:end['sum_received'].update(lost_percent=1.25,lost_packets=5,packets=400)
 intervals=[{'sum':{'start':i,'end':i+1,'seconds':1,'bytes':12345+i,'bits_per_second':recv+i}}for i in range(secs)]
 server={'SIMULATED':True,'end':end,'intervals':intervals}
 print(json.dumps({'SIMULATED':True,'start':{'test_start':{'protocol':'UDP'if udp else'TCP'}},'end':end,'intervals':intervals,'server_output_json':server}));sys.exit(0)
if name=='networkQuality':
 with (state/'nq-calls.jsonl').open('a')as f:f.write(json.dumps(args)+'\n')
 if scene=='online-error':print('SIMULATED online failure',file=sys.stderr);sys.exit(2)
 print(json.dumps({'SIMULATED':True,'interface_name':iface,'dl_throughput':79000000,'ul_throughput':28000000,'dl_responsiveness':210.74,'ul_responsiveness':67.91,'base_rtt':8,'raw_latency':[8,1600]}));sys.exit(0)
if name=='ioreg':
 if 'AppleVTD'in args:print('AppleVTD registered, matched, active');sys.exit(0)
 data=plistlib.loads((state/'fixture.plist').read_bytes())
 if '-p'in args:sys.stdout.buffer.write(plistlib.dumps([{'IORegistryEntryName':'SIMULATED_ROOT','IORegistryEntryChildren':data}]));sys.exit(0)
 q=state/'snapshot-count';i=int(q.read_text())if q.exists()else 0;q.write_text(str(i+1));p=data[0]
 for key,sz in [('KGP_ZC19',32),('KGP_BE200_TRANSPORT',48)]:
  if key not in p:continue
  w=list(struct.unpack('<'+str(sz)+'Q',p[key]));delta=(i//2)if scene=='adjacent-equal'else i
  if scene=='stale-final':delta=0
  w[3]+=delta;w[4]+=delta*1000000000
  if scene=='fault-after-round1'and i>=3:w[8 if key=='KGP_ZC19'else 8]=1
  p[key]=struct.pack('<'+str(sz)+'Q',*w)
 if scene=='missing-bz':p.pop('KGP_BE200_TRANSPORT',None)
 sys.stdout.buffer.write(plistlib.dumps(data));sys.exit(0)
if name=='kmutil':
 uuid=os.environ['MOCK_UUID'];uuid='FFFFFFFF-2222-3333-4444-555555555555'if scene=='wrong-uuid'else uuid
 print('111 0 0xffffff8000000000 0x1000 com.zxystd.AirportItlwm (1.0.0) '+uuid+' <1>');sys.exit(0)
if name=='xcrun':print('UUID: '+os.environ['MOCK_UUID']+' (x86_64) '+args[-1]);sys.exit(0)
if name=='ipconfig':print('192.0.2.135');sys.exit(0)
if name=='route':print('interface: '+('en0'if scene=='wrong-route'else iface));sys.exit(0)
if name=='ifconfig':print(iface+': SIMULATED status active');sys.exit(0)
if name=='networksetup':print('Hardware Port: Wi-Fi\nDevice: '+iface);sys.exit(0)
if name=='sysctl':print('SIMULATED boot');sys.exit(0)
if name=='sw_vers':print('SIMULATED macOS fixture');sys.exit(0)
if name=='ps':print('SIMULATED no compiler');sys.exit(0)
if name=='sleep':
 with (state/'sleep-calls.jsonl').open('a')as f:f.write(json.dumps(args)+'\n')
 sys.exit(0)
if name=='ditto':
 src=Path(args[-2]);dest=Path(args[-1])
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED)as z:
  for p in src.rglob('*'):
   if p.is_file():z.write(p,str(p.relative_to(src.parent)))
 sys.exit(0)
raise SystemExit('Unstubbed command '+name)
'''

class Tests(unittest.TestCase):
 def test_01_baseline_and_protocol(self):
  self.assertEqual(hashlib.sha256(OLD.read_bytes()).hexdigest(),'be9c07168daf7b8ffd6f6c71ba4f62117d1e28334c4a64a9a063b2a115cc19ed')
  old=OLD.read_text();new=NEW.read_text()
  for a,b in [('run_test() {','\nstage="PREFLIGHT"'),('for round in 1 2 3; do','\n# Mandatory FINAL ONLINE')]:
   actual=new.split(a,1)[1].split('\nstage="DISCOVER_BOUND_INTERFACE"'if a=='run_test() {'else b,1)[0];self.assertEqual(actual,old.split(a,1)[1].split(b,1)[0])
  self.assertEqual(new.split('# Mandatory FINAL ONLINE',1)[1],old.split('# Mandatory FINAL ONLINE',1)[1])
  subprocess.run(['/bin/bash','-n',str(NEW)],check=True)
 def test_02_all_legacy_frontends(self):
  calls=[[l,mode] for l in ['ZC8','ZC13','ZC18','ZC19','ZC20','ZC21','ZC22','ZC23','ZC24','ZC25','ZC26','ZC27','ZC28'] for mode in ['mapped','conventional']]
  calls +=[['LAOBAMAC',b,mode]for b in ['44','47','53']for mode in ['mapped','conventional']]+[['LAOBAMAC','mapped'],['LAOBAMAC','conventional'],['/custom/AirportItlwm.kext']]
  env={k:v for k,v in os.environ.items()if k not in ['BENCHMARK_PROFILE','CANDIDATE_LABEL','LAOBAMAC_SOURCE_REF','EXPECTED_MODE','LAOBAMAC_KEXT']};env.update(IPERF_SERVER='192.0.2.33',WIFI_IFACE='en9',IOREG_WIFI_SERVICE='CNVW',DEVICE_LABEL='AX211',AIRPORTITLWM_ROOT='/custom/root')
  for args in calls:
   values=[]
   for script in [OLD,NEW]:
    code=script.read_text().split('[[ -f "$EXECUTABLE" ]]',1)[0]+"\nprintf 'RES=%s|%s|%s|%s|%s|%s\\n' \"$PROFILE\" \"$LABEL\" \"$MODE\" \"$KEXT\" \"$IFACE\" \"$DEVICE\"\n"
    r=subprocess.run(['/bin/bash','-c',code,'mock-front',*args],capture_output=True,text=True,env=env);self.assertEqual(r.returncode,0,r.stderr+r.stdout);values.append(next(x for x in r.stdout.splitlines()if x.startswith('RES=')))
   self.assertEqual(*values,args);COUNTS['legacy_frontends']+=1
 def test_03_resolver_actual_and_alternate_names(self):
  for iface,node in [('en0','CNVW'),('en1','WIFI'),('en2','ARPT'),('en17','NETW')]:
   data=modified(iface,node);r,p=m.resolve(data,INFO,'be200','BE200-ALPHA1',iface,node);self.assertEqual(r['interface_name'],iface);self.assertEqual(r['pci']['device-id'],'0x272b')
  data=modified('en1','WIFI')
  for iface,node in [('en2','WIFI'),('en1','ARPT')]:
   with self.assertRaises(ValueError):m.resolve(data,INFO,'be200','BE200-ALPHA1',iface,node)
  with self.assertRaises(ValueError):m.resolve(data+copy.deepcopy(data),INFO,'be200','BE200-ALPHA1')
  changed=copy.deepcopy(data);changed[0]['KGP_BE200_PCI_ID']=0x27258086
  with self.assertRaises(ValueError):m.resolve(changed,INFO,'be200','BE200-ALPHA1')
  prior=m.resolve(data,INFO,'be200','BE200-ALPHA1')[0];changed=copy.deepcopy(data);changed[0]['IORegistryEntryID']+=1
  with self.assertRaises(ValueError):m.resolve(changed,INFO,'be200','BE200-ALPHA1',prior=prior)
  external=plistlib.loads((B/'tests/fixtures/EXTERNAL53_BINDING.plist').read_bytes());r,_=m.resolve(external,INFO,'external','LAOBAMAC_53');self.assertEqual(r['interface_class'],'AirportItlwmEthernetInterface');self.assertEqual(r['pci']['device-id'],'0x2725')
 def guard(self,script,source,mode,label,phase='boundary',ref='NONE',prev='NONE',kind='kgp'):
  env=dict(os.environ,BENCHMARK_PROFILE=kind,BENCHMARK_CANDIDATE_PROFILE_JSON=str(B/'runner_profiles/profiles.json'))
  return subprocess.run([sys.executable,'-c',guard_code(script),str(source),str(ref),str(prev),mode,label,phase],env=env,capture_output=True,text=True)
 def test_04_legacy_guards_unchanged(self):
  with tempfile.TemporaryDirectory()as t:
   t=Path(t)
   for label in ['ZC8','ZC13','ZC18','ZC19','ZC20','ZC21','ZC22','ZC23','ZC24','ZC25','ZC26','ZC27','ZC28']:
    for mode in ['mapped','conventional']:
     data=node_for(label,mode);results=[]
     for script in [OLD,NEW]:
      p=t/(script.parent.name+'.plist');p.write_bytes(plistlib.dumps(data));r=self.guard(script,p,mode,label);self.assertEqual(r.returncode,0,r.stderr);results.append(json.loads(p.with_suffix('.health.json').read_text()))
     self.assertEqual(*results);COUNTS['guard_comparisons']+=1
   for kind in ['marker','mode','fault','malformed','rollback','epoch','final-stale']:
    codes=[]
    for script in [OLD,NEW]:
     d=node_for('ZC28','mapped');p=t/(script.parent.name+'.plist');p.write_bytes(plistlib.dumps(d));self.assertEqual(self.guard(script,p,'mapped','ZC28').returncode,0);h=p.with_suffix('.health.json');reference=t/(script.parent.name+'-ref.json');reference.write_bytes(h.read_bytes());w=list(struct.unpack('<32Q',d[0]['KGP_ZC19']))
     if kind=='marker':d[0]['KGP_RX_EXPERIMENT']='wrong'
     if kind=='mode':w[5]=1
     if kind=='fault':w[8]=1
     if kind=='epoch':w[12]+=1
     if kind=='rollback':w[3]-=1
     d[0]['KGP_ZC19']=struct.pack('<32Q',*w)
     if kind=='malformed':d[0]['KGP_ZC19']=b'bad'
     p.write_bytes(plistlib.dumps(d));r=self.guard(script,p,'mapped','ZC28','final'if kind=='final-stale'else'boundary',reference,reference);codes.append(r.returncode);self.assertNotEqual(r.returncode,0,kind)
    self.assertEqual(*codes);COUNTS['guard_comparisons']+=1
   # Adjacent equal records are accepted, FINAL must advance from INITIAL.
   for script in [OLD,NEW]:
    p=t/(script.parent.name+'.plist');d=node_for('ZC28','mapped');p.write_bytes(plistlib.dumps(d));self.assertEqual(self.guard(script,p,'mapped','ZC28').returncode,0);ref=t/(script.parent.name+'-base.json');ref.write_bytes(p.with_suffix('.health.json').read_bytes());self.assertEqual(self.guard(script,p,'mapped','ZC28',ref=ref,prev=ref).returncode,0)
 def campaign(self,script,label='ZC28',mode='mapped',scene='normal',iface='en17',parent='WIFI',keyword=False):
  with tempfile.TemporaryDirectory(prefix='SIMULATED-shared-r8-')as tmp:
   t=Path(tmp);bin=t/'bin';bin.mkdir();state=t/'state';state.mkdir();out=t/'results';root=t/'runner';root.mkdir();shutil=__import__('shutil');shutil.copytree(B/'runner_profiles',root/'runner_profiles');(bin/'mock.py').write_text(MOCK);(bin/'mock.py').chmod(0o755)
   for cmd in ['iperf3','networkQuality','ioreg','kmutil','xcrun','ipconfig','route','ifconfig','networksetup','sysctl','sw_vers','ps','sleep','ditto']:(bin/cmd).symlink_to('mock.py')
   text=script.read_text().replace('/usr/bin/ditto',str(bin/'ditto'));run=root/'runner.sh';run.write_text(text)
   exe=t/'AirportItlwm.kext/Contents/MacOS/AirportItlwm';exe.parent.mkdir(parents=True);info=copy.deepcopy(INFO);candidate_uuid=UUID
   if label in ('BE200-ALPHA1','BE200-P8'):
    bundle=P8 if label=='BE200-P8' else P7
    if not bundle.exists():self.skipTest('Actual frozen P7 bundle not locally available for full profile test')
    kext=bundle;candidate_uuid=m.profile(label)['uuid'];data=modified(iface,parent)
    if label=='BE200-P8':
     data[0]['KGP_BE200_CANDIDATE']=m.profile(label)['registry_candidate'];data[0]['KGP_RX_EXPERIMENT']=m.profile(label)['marker']
    w=list(struct.unpack('<32Q',data[0]['KGP_ZC19']));w[5]=2 if mode=='mapped'else 1;data[0]['KGP_ZC19']=struct.pack('<32Q',*w);w=list(struct.unpack('<48Q',data[0]['KGP_BE200_TRANSPORT']));w[5]=2 if mode=='mapped'else 1;data[0]['KGP_BE200_TRANSPORT']=struct.pack('<48Q',*w)
   else:
    exe.write_bytes(b'SIMULATED executable '+label.encode());exe.parent.parent.joinpath('Info.plist').write_bytes(plistlib.dumps(info));kext=exe.parent.parent.parent;data=node_for(label,mode,iface,parent)
   (state/'fixture.plist').write_bytes(plistlib.dumps(data))
   env={k:v for k,v in os.environ.items() if k not in ['BENCHMARK_PROFILE','CANDIDATE_LABEL','EXPECTED_MODE','CANDIDATE_KEXT','BE200_KEXT','LAOBAMAC_KEXT','EXTERNAL_SOURCE_REF','LAOBAMAC_SOURCE_REF','WIFI_IFACE','IOREG_WIFI_SERVICE']};env.update(PATH=str(bin)+':'+os.environ['PATH'],MOCK_STATE=str(state),MOCK_IFACE=iface,MOCK_PARENT=parent,MOCK_UUID=candidate_uuid,MOCK_SCENE=scene,IPERF_SERVER='192.0.2.33',AIRPORTITLWM_TEST_DIR=str(out),DEVICE_LABEL='SIMULATED_OFFLINE',EXPECTED_MODE=mode,CANDIDATE_LABEL=label,PYTHONDONTWRITEBYTECODE='1')
   if script==OLD or label not in ('BE200-ALPHA1','BE200-P8'):env.update(WIFI_IFACE=iface,IOREG_WIFI_SERVICE=parent)
   if label=='EXTERNAL':env.update(BENCHMARK_PROFILE='external',EXPECTED_MODE='unverified',EXTERNAL_SOURCE_REF='738d4c61b5a64433b27c1ab09dba94916ab77d8d',CANDIDATE_LABEL='LAOBAMAC_53',EXTERNAL_DECLARED_MODE=mode)
   elif label in ('BE200-ALPHA1','BE200-P8'):env.update(BENCHMARK_PROFILE='be200')
   if keyword:env.update(IPERF_SERVER='',CANDIDATE_KEXT=str(kext),TEST_GAP_SECONDS='2',WIFI_IFACE='',IOREG_WIFI_SERVICE='')
   r=terminal_run(['/bin/bash',str(run),label],env,'192.0.2.33\n') if keyword else subprocess.run(['/bin/bash',str(run),str(kext)],env=env,capture_output=True,text=True,timeout=90)
   zips=list(out.glob('*.zip'));self.assertEqual(len(zips),1,r.stdout+r.stderr)
   with zipfile.ZipFile(zips[0])as z:
    self.assertIsNone(z.testzip());prefix=next(n for n in z.namelist()if n.endswith('/RUN_STATUS.txt')).rsplit('/',1)[0]+'/'
    status=z.read(prefix+'RUN_STATUS.txt').decode();names=z.namelist()
    for line in z.read(prefix+'SHA256SUMS.txt').decode().splitlines():
     h,n=line.split(None,1);self.assertEqual(hashlib.sha256(z.read(prefix+n.removeprefix('./'))).hexdigest(),h)
    if scene in ['normal','adjacent-equal']:
     self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertIn('STATUS=COMPLETE_42_AND_ONLINE',status)
     self.assertEqual(sum(n.endswith('.iperf3.json')for n in names),42);self.assertEqual(sum(n.endswith('.server_output.json')for n in names),42)
     for n in names:
      if n.endswith('.iperf3.json'):
       j=json.loads(z.read(n));self.assertTrue(j['SIMULATED']);self.assertEqual(json.loads(z.read(n.replace('.iperf3.json','.server_output.json'))),j['server_output_json'])
     self.assertEqual(z.read(prefix+'EXECUTED_RUNNER.sh'),run.read_bytes())
     self.assertIn(b'Download responsiveness (RPM): 210.74',z.read(prefix+'ONLINE_NETWORKQUALITY.txt'))
     if script==NEW:
      prov=json.loads(z.read(prefix+'RUNNER_PROVENANCE.json'));self.assertTrue(prov['copy_matches_executed_source_at_checks']);self.assertEqual(len(prov['resource_sha256']),3)
      binding=json.loads(z.read(prefix+'DEVICE_BINDING.json'));self.assertEqual(binding['interface_name'],iface);self.assertEqual(binding['provider_name'],parent)
     if label in ('BE200-ALPHA1','BE200-P8'):
      h=json.loads(z.read(prefix+'FINAL.health.json'));self.assertEqual(h['Candidate'],label);self.assertIn('BE200',h);self.assertEqual(h['BE200']['record']['actual_mode'],2 if mode=='mapped'else 1)
    else:
     self.assertNotEqual(r.returncode,0,r.stdout+r.stderr);self.assertIn('STATUS=INCOMPLETE_OR_ONLINE_FAILURE',status)
     if scene=='stale-final':self.assertIn('COMPLETED_IPERF_TESTS=42',status)
     if scene in ['wrong-uuid','missing-bz','wrong-route']:self.assertIn('COMPLETED_IPERF_TESTS=0',status)
   calls=[json.loads(x)for x in(state/'iperf-calls.jsonl').read_text().splitlines()]if(state/'iperf-calls.jsonl').exists()else[]
   sleeps=[json.loads(x)for x in(state/'sleep-calls.jsonl').read_text().splitlines()]if(state/'sleep-calls.jsonl').exists()else[]
   for c in calls:self.assertEqual(c[c.index('-B')+1],'192.0.2.135%'+iface)
   COUNTS['full_mock_campaigns']+=1
   print('PASS MOCK campaign',script.parent.name,label,mode,scene,'calls',len(calls),flush=True)
   return calls,sleeps
 def test_05_full_legacy_protocol_equivalence(self):
  for label,mode in [('ZC28','mapped'),('ZC28','conventional'),('ZC8','conventional'),('ZC13','mapped'),('EXTERNAL','mapped'),('EXTERNAL','conventional')]:
   old=self.campaign(OLD,label,mode);new=self.campaign(NEW,label,mode);self.assertEqual(old,new)
   self.assertEqual(len(new[0]),42);self.assertEqual(new[1],[['2']]*42+[['3']])
 def test_06_be200_modes_adjacent_and_failures(self):
  for mode in ['mapped','conventional']:self.assertEqual(len(self.campaign(NEW,'BE200-ALPHA1',mode,'adjacent-equal')[0]),42)
  for bad in ['wrong-uuid','missing-bz','wrong-route','fault-after-round1','stale-final','iperf-error','online-error']:self.campaign(NEW,'BE200-ALPHA1','mapped',bad)

 def test_07_be200_transport_contracts(self):
  with tempfile.TemporaryDirectory()as t:
   t=Path(t);base=modified();p=t/'initial.plist';p.write_bytes(plistlib.dumps(base));self.assertEqual(self.guard(NEW,p,'mapped','BE200-ALPHA1',kind='be200').returncode,0);m.be200_health(str(p),str(p.with_suffix('.health.json')),'NONE','NONE','boundary','BE200-ALPHA1');ref=t/'ref.json';ref.write_bytes(p.with_suffix('.health.json').read_bytes())
   for kind,index,value in [('mode',5,1),('generation',6,20),('reset',7,11),('fault',8,30),('error',9,35),('ownership',10,0),('run',11,1),('msix',12,0),('schema',0,199),('rollback',3,839),('safety-counter',19,1),('final-stale',3,840),('missing',None,None),('truncated',None,None),('bad-history',None,None),('marker',None,None)]:
    d=copy.deepcopy(base);w=list(struct.unpack('<48Q',d[0]['KGP_BE200_TRANSPORT']));w[3]=841;w[4]+=1000000000
    if index is not None:w[index]=value
    d[0]['KGP_BE200_TRANSPORT']=struct.pack('<48Q',*w)
    if kind=='missing':d[0].pop('KGP_BE200_TRANSPORT')
    if kind=='truncated':d[0]['KGP_BE200_TRANSPORT']=b'bad'
    if kind=='bad-history':d[0]['KGP_BE200_CONNECTION']=b'bad'
    if kind=='marker':d[0]['KGP_RX_EXPERIMENT']='wrong'
    q=t/(kind+'.plist');q.write_bytes(plistlib.dumps(d));h=q.with_suffix('.health.json');h.write_bytes(ref.read_bytes())
    with self.assertRaises((ValueError,KeyError)):
     m.be200_health(str(q),str(h),str(ref),str(ref),'final'if kind=='final-stale'else'boundary','BE200-ALPHA1')
   # Rollback against LAST accepted snapshot, even still greater than INITIAL.
   last=json.loads(ref.read_text());last['BE200']['record']['publication']+=2;last_path=t/'last.json';last_path.write_text(json.dumps(last));d=copy.deepcopy(base);w=list(struct.unpack('<48Q',d[0]['KGP_BE200_TRANSPORT']));w[3]+=1;w[4]+=1000000000;d[0]['KGP_BE200_TRANSPORT']=struct.pack('<48Q',*w);q=t/'rollback-above-initial.plist';q.write_bytes(plistlib.dumps(d));h=q.with_suffix('.health.json');h.write_bytes(ref.read_bytes())
   with self.assertRaises(ValueError):m.be200_health(str(q),str(h),str(ref),str(last_path),'boundary','BE200-ALPHA1')
   # Optional history is unavailable, not a fabricated empty/zero trace.
   d=copy.deepcopy(base);d[0].pop('KGP_BE200_CONNECTION');q=t/'without-history.plist';q.write_bytes(plistlib.dumps(d));h=q.with_suffix('.health.json');h.write_bytes(ref.read_bytes());m.be200_health(str(q),str(h),str(ref),str(ref),'boundary','BE200-ALPHA1');self.assertEqual(json.loads(h.read_text())['BE200']['connection_history_status'],'UNAVAILABLE_NOT_ZERO')
   # Actual profile pins are exercised against the real saved P7 bundle read-only.
   if P7.exists():
    loaded=t/'loaded.txt';spec=m.profile('BE200-ALPHA1');loaded.write_text('com.zxystd.AirportItlwm (1.0.0) '+spec['uuid']);exe=P7/'Contents/MacOS/AirportItlwm';info=P7/'Contents/Info.plist';hinfo=hashlib.sha256(info.read_bytes()).hexdigest();out=t/'identity.json'
    m.identity(str(loaded),str(exe),str(info),spec['uuid'],'com.zxystd.AirportItlwm',spec['executable_sha256'],hinfo,'be200','BE200-ALPHA1','',str(out))
    wrong=t/'wrong-exe';wrong.write_bytes(b'not the frozen P7 executable')
    with self.assertRaises(ValueError):m.identity(str(loaded),str(wrong),str(info),spec['uuid'],'com.zxystd.AirportItlwm',hashlib.sha256(wrong.read_bytes()).hexdigest(),hinfo,'be200','BE200-ALPHA1','',str(out))

 def test_08_generic_devices_and_cli_separation(self):
  for device,iface,parent in [(0x43f0,'en0','CNVW'),(0x51f0,'en1','WIFI'),(0x2725,'en2','ARPT'),(0x24fd,'en17','NETW')]:
   data=node_for('ZC28','mapped',iface,parent);data[0]['device-id']=device.to_bytes(4,'little');r,_=m.resolve(data,INFO,'kgp','ZC28');self.assertEqual(r['pci']['device-id'],'0x'+format(device,'04x'));self.assertFalse(r['device_label_is_identification'])
  data=modified('en2','ARPT');wi=data[0]['IORegistryEntryChildren'][0]['IORegistryEntryChildren'][0];wi['BSD Name']='awdl0';wi['IOObjectClass']='IO80211P2PInterface'
  with self.assertRaises(ValueError):m.resolve(data,INFO,'kgp','ZC28')
  data=modified('en2','ARPT');data[0]['IORegistryEntryChildren'][0]['CFBundleIdentifier']='other.driver'
  with self.assertRaises(ValueError):m.resolve(data,INFO,'kgp','ZC28')
  code=NEW.read_text().split('[[ -f "$EXECUTABLE" ]]',1)[0]
  for args,extra in [(['ZC28'],{'BENCHMARK_PROFILE':'be200'}),(['BE200-ALPHA1'],{'BENCHMARK_PROFILE':'external'}),(['/absolute/AirportItlwm.kext'],{'BENCHMARK_PROFILE':'be200','CANDIDATE_LABEL':'ZC28'})]:
   env={k:v for k,v in os.environ.items()if k not in ['CANDIDATE_LABEL','BENCHMARK_PROFILE','EXPECTED_MODE']};env.update(IPERF_SERVER='192.0.2.33',**extra);r=subprocess.run(['/bin/bash','-c',code,'mock-front',*args],env=env,capture_output=True,text=True);self.assertNotEqual(r.returncode,0,r.stdout+r.stderr)

 def test_09_ready_start_command(self):
  calls,sleeps=self.campaign(NEW,'BE200-ALPHA1','mapped','adjacent-equal',keyword=True)
  self.assertEqual(len(calls),42);self.assertEqual(sleeps,[['2']]*42+[['3']])

 def test_09b_private_p8_candidate_and_ax29(self):
  self.assertTrue(P8.exists(),'Built private P8 bundle required; no skipped candidate validation')
  self.assertEqual(len(self.campaign(NEW,'BE200-P8','mapped','adjacent-equal',keyword=True)[0]),42)
  self.assertEqual(len(self.campaign(NEW,'BE200-P8','conventional','normal')[0]),42)
  # Independent fixture directories/OS stubs; no shared system/network state.
  from concurrent.futures import ThreadPoolExecutor
  jobs=[('BE200-P8','mapped',bad)for bad in ['wrong-uuid','missing-bz','wrong-route','fault-after-round1','stale-final']]+[('ZC30',mode,'normal')for mode in ['mapped','conventional']]
  with ThreadPoolExecutor(max_workers=2)as pool:
   for args,calls in zip(jobs,pool.map(lambda args:self.campaign(NEW,*args)[0],jobs)):
    if args[0]=='ZC30':self.assertEqual(len(calls),42)

 def test_09bb_candidate_default_directories(self):
  code=NEW.read_text().split('[[ -f "$EXECUTABLE" ]]',1)[0]+"\nprintf 'KEXT=%s\\n' \"$KEXT\"\n"
  for label,folder in [('BE200-ALPHA1','BE200-EXPERIMENTAL'),('BE200-P8','BE200-EXPERIMENTAL-P8')]:
   env={k:v for k,v in os.environ.items()if k not in ['CANDIDATE_LABEL','BENCHMARK_PROFILE','EXPECTED_MODE','CANDIDATE_KEXT','BE200_KEXT']}
   env.update(IPERF_SERVER='192.0.2.33',AIRPORTITLWM_ROOT='/test/root')
   r=subprocess.run(['/bin/bash','-c',code,'mock-front',label,'mapped'],env=env,capture_output=True,text=True)
   self.assertEqual(r.returncode,0,r.stderr+r.stdout);self.assertIn('KEXT=/test/root/'+folder+'/AirportItlwm.kext',r.stdout)

 def test_09c_frozen_r8_resources_and_p7_profile(self):
  ref=B/'reference/r8'
  self.assertEqual(hashlib.sha256((ref/NEW.name).read_bytes()).hexdigest(),'ad488030e076fc825538bc310bc3b62374ee08340659b34cf150d43acd26d398')
  old=json.loads((ref/'runner_profiles/profiles.json').read_text());new=json.loads((B/'runner_profiles/profiles.json').read_text())
  self.assertEqual(old['BE200-ALPHA1'],new['BE200-ALPHA1'])
  for a,b in [('run_test() {','\nstage="DISCOVER_BOUND_INTERFACE"'),('for round in 1 2 3; do','\n# Mandatory FINAL ONLINE')]:
   self.assertEqual(NEW.read_text().split(a,1)[1].split(b,1)[0],(ref/NEW.name).read_text().split(a,1)[1].split(b,1)[0])
  self.assertEqual(NEW.read_text().split('# Mandatory FINAL ONLINE',1)[1],(ref/NEW.name).read_text().split('# Mandatory FINAL ONLINE',1)[1])

 def test_09d_old_p8_rejected_and_r9_frozen(self):
  ref=B/'reference/r9';self.assertEqual(hashlib.sha256((ref/NEW.name).read_bytes()).hexdigest(),'ba4b08fdf8516140cdbfe8de43d6560945319e873f47725acd1e24f76962d182')
  old=json.loads((ref/'runner_profiles/profiles.json').read_text())['BE200-P8'];current=m.profile('BE200-P8');self.assertEqual(old['uuid'],'53925674-06E8-3F06-AAD6-DF6A9A58B2BD');self.assertNotEqual(old['uuid'],current['uuid']);self.assertEqual(current['registry_candidate'],'BE200-GL92-PRIVATE-P8R1')
  with tempfile.TemporaryDirectory()as t:
   t=Path(t);loaded=t/'loaded.txt';loaded.write_text('com.zxystd.AirportItlwm (1.0.0) '+old['uuid']);bundle=Path(os.environ['OLD_P8_TEST_KEXT']);exe=bundle/'Contents/MacOS/AirportItlwm';info=bundle/'Contents/Info.plist'
   with self.assertRaises(ValueError):m.identity(str(loaded),str(exe),str(info),old['uuid'],'com.zxystd.AirportItlwm',old['executable_sha256'],old['info_plist_sha256'],'be200','BE200-P8','',str(t/'out.json'))

 def test_10_identity_scope_is_not_source_verification(self):
  with tempfile.TemporaryDirectory()as t:
   t=Path(t);e=t/'executable';e.write_bytes(b'SIMULATED executable');i=t/'Info.plist';i.write_bytes(plistlib.dumps(INFO));loaded=t/'loaded.txt';loaded.write_text('com.zxystd.AirportItlwm (1.0.0) '+UUID);out=t/'identity.json'
   m.identity(str(loaded),str(e),str(i),UUID,'com.zxystd.AirportItlwm',hashlib.sha256(e.read_bytes()).hexdigest(),hashlib.sha256(i.read_bytes()).hexdigest(),'kgp','ZC28','',str(out));r=json.loads(out.read_text());self.assertIs(r['source_build_correspondence_verified'],False);self.assertEqual(r['binary_identity_scope'],'CANDIDATE_DISK_UUID_AND_MARKER_GUARD');self.assertIsNone(r['referenced_binary_build_commit'])

if __name__=='__main__':
 r=unittest.main(verbosity=2,exit=False);print('OFFLINE COVERAGE',json.dumps(COUNTS),flush=True);sys.exit(not r.result.wasSuccessful())
