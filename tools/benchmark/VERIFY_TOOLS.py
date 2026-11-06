#!/usr/bin/env python3
"""Offline package/matrix/identity validation. Never invokes network tests."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,re,subprocess,tempfile,shlex,plistlib,struct,os
root=Path(__file__).resolve().parent
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--kext',type=Path,required=True);ap.add_argument('--p10-kext',type=Path);args=ap.parse_args();bundle=args.kext
(root/'evidence').mkdir(exist_ok=True)
ident=json.loads((root/'BUILD_IDENTITY.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
exe=bundle/'Contents/MacOS/AirportItlwm';info=bundle/'Contents/Info.plist'
assert sha(exe)==ident['executable_sha256'] and sha(info)==ident['info_plist_sha256']
assert subprocess.check_output(['dwarfdump','--uuid',str(exe)],text=True).split()[1]==ident['uuid']
profiledir=root/'runner_profiles'
spec=importlib.util.spec_from_file_location('profiles',profiledir/'shared_profile.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
profiles=json.loads((profiledir/'profiles.json').read_text())
rates={'':(100,250,400,500,600,700,800,900),'UDP1200':(300,500,700,900,1000,1100,1200,1200),'UDP1300':(300,500,700,900,1000,1100,1200,1300),'UDP1600':(300,500,700,900,1100,1300,1500,1600),'UDP1700':(300,500,700,900,1100,1300,1500,1700)}
protocols=set();folders=set();results={}
with tempfile.TemporaryDirectory(prefix='r13-package-') as tmp:
 t=Path(tmp)
 loaded=t/'loaded.txt';loaded.write_text('AirportItlwm com.zxystd.AirportItlwm '+ident['uuid']+'\n')
 for label in ident['benchmark_profile_labels']:
  p=profiles[label];assert p['uuid']==ident['uuid'] and p['executable_sha256']==sha(exe) and p['binary_build_commit']==ident['source_commit']
  helper.identity(str(loaded),str(exe),str(info),ident['uuid'],'com.zxystd.AirportItlwm',sha(exe),sha(info),('be200' if label.startswith('BE200') else 'kgp'),label,ident['source_commit'],str(t/'identity.json'))
  for bad_uuid,bad_sha in [('00000000-0000-0000-0000-000000000000',sha(exe)),(ident['uuid'],'0'*64)]:
   try:helper.identity(str(loaded),str(exe),str(info),bad_uuid,'com.zxystd.AirportItlwm',bad_sha,sha(info),('be200' if label.startswith('BE200') else 'kgp'),label,ident['source_commit'],str(t/'bad.json'))
   except ValueError:pass
   else:raise AssertionError('identity guard accepted mismatch')
 # Both BE200 binary generations have their own exact pins. Never relabel P10.
 old=args.p10_kext
 for label,selected_bundle in [('BE200-P10',old),('BE200-2.0.0',bundle)]:
  if selected_bundle is None:continue
  bundle_check=selected_bundle
  p=profiles[label];x=bundle_check/'Contents/MacOS/AirportItlwm';ip=bundle_check/'Contents/Info.plist'
  assert sha(x)==p['executable_sha256'] and subprocess.check_output(['dwarfdump','--uuid',str(x)],text=True).split()[1]==p['uuid']
  loaded.write_text('AirportItlwm com.zxystd.AirportItlwm '+p['uuid']+'\n')
  helper.identity(str(loaded),str(x),str(ip),p['uuid'],'com.zxystd.AirportItlwm',sha(x),sha(ip),'be200',label,p['binary_build_commit'],str(t/'be-identity.json'))
  opposite='BE200-2.0.0' if label=='BE200-P10' else 'BE200-P10'
  try:helper.identity(str(loaded),str(x),str(ip),p['uuid'],'com.zxystd.AirportItlwm',sha(x),sha(ip),'be200',opposite,p['binary_build_commit'],str(t/'bad-be-identity.json'))
  except ValueError:pass
  else:raise AssertionError('accepted incorrect original P10 / release BE200 pin')
 def device_fixture(label):
  spec=profiles[label];be=label.startswith('BE200')
  w=[19,1,256,10,100,2,9,0,0,0,0,5,7]+[0]*19
  node={'IORegistryEntryName':'ARPT','IOObjectClass':'IOPCIDevice','IORegistryEntryID':10,
   'vendor-id':bytes.fromhex('86800000'),'device-id':(0x272b if be else 0x2725).to_bytes(4,'little'),
   'KGP_RX_EXPERIMENT':spec['marker'],'KGP_ZC19':struct.pack('<32Q',*w),
   'IORegistryEntryChildren':[{'IORegistryEntryName':'AirportItlwm','IOObjectClass':'AirportItlwm','IORegistryEntryID':11,'CFBundleIdentifier':'com.zxystd.AirportItlwm','IORegistryEntryChildren':[{'IORegistryEntryName':'interface','IOObjectClass':'IO80211Interface','IORegistryEntryID':12,'BSD Name':'en2'}]}]}
  if be:
   node.update(KGP_BE200_CANDIDATE=spec['registry_candidate'],KGP_BE200_PCI_ID=(0x272b<<16)|0x8086,
    KGP_BE200_TRANSPORT=struct.pack('<48Q',200,1,384,10,100,2,9,5,0,0,1,4,1,*([0]*35)))
  return node
 for label,kind in [('AX210-2.0.0','kgp'),('BE200-P10','be200'),('BE200-2.0.0','be200')]:
  tree=device_fixture(label);meta=plistlib.loads(info.read_bytes());binding,_=helper.resolve([tree],meta,kind,label,'en2')
  assert binding['interface_name']=='en2'
  tree['device-id']=(0x2725 if kind=='be200' else 0x272b).to_bytes(4,'little')
  try:helper.resolve([tree],meta,kind,label,'en2')
  except ValueError:pass
  else:raise AssertionError('wrong AX/BE200 provider accepted')
 for variant,rx in rates.items():
  name='AirportItlwm_3x20'+('_'+variant if variant else '')+'_PORTABLE_IP_PROMPT.sh';runner=root/name;s=runner.read_text()
  subprocess.run(['/bin/bash','-n',str(runner)],check=True)
  # Exercise the actual label condition. A negative label still fails.
  condition=next(l.strip().split(' || {')[0] for l in s.splitlines() if 'Invalid CANDIDATE_LABEL' in l)
  for label in ['AX201-2.0.0','AX210-2.0.0','AX211-2.0.0','AX210-P10R10']:
   rc=subprocess.run(['/bin/bash','-c','LABEL='+shlex.quote(label)+'\n'+condition]).returncode
   assert (rc==0)==(label!='AX210-P10R10'),label
  becondition=next(l.strip().split(' || {')[0] for l in s.splitlines() if 'unsupported BE200 candidate profile' in l)
  for label in ['BE200-P10','BE200-2.0.0','BE200-P10R10']:
   assert (subprocess.run(['/bin/bash','-c','LABEL='+label+'\n'+becondition]).returncode==0)==(label!='BE200-P10R10')
  # Execute each runner's real embedded health guard with no AX-only fields.
  guard=s.split('check_snapshot() {',1)[1].split("<<'PY'\n",1)[1].split('\nPY\n',1)[0]
  gp=t/'guard.py';gp.write_text(guard)
  for label,kind in [('AX210-2.0.0','kgp'),('BE200-P10','be200'),('BE200-2.0.0','be200')]:
   src=t/(variant+'-'+label+'.plist');tree=device_fixture(label);src.write_bytes(plistlib.dumps([tree]))
   env=os.environ.copy();env.update(BENCHMARK_PROFILE=kind,BENCHMARK_CANDIDATE_PROFILE_JSON=str(profiledir/'profiles.json'))
   command=['python3',str(gp),str(src),'NONE','NONE','mapped',label,'boundary']
   cp=subprocess.run(command,env=env,capture_output=True,text=True);assert cp.returncode==0,cp.stdout+cp.stderr
   if kind=='be200':
    helper.be200_health(str(src),str(src.with_suffix('.health.json')),'NONE','NONE','boundary',label)
   # Wrong marker, DMA mode and fault remain fail-closed in each profile.
   for mutation in ['marker','mode','fault']:
    bad=device_fixture(label)
    if mutation=='marker':bad['KGP_RX_EXPERIMENT']='wrong'
    else:
     words=list(struct.unpack('<32Q',bad['KGP_ZC19']));words[5 if mutation=='mode' else 9]=1;bad['KGP_ZC19']=struct.pack('<32Q',*words)
    src.write_bytes(plistlib.dumps([bad]));assert subprocess.run(command,env=env,capture_output=True).returncode!=0
  m=s[s.index('matrix=['):s.index('settings={',s.index('matrix=['))];scope={};exec(m,scope);matrix=scope['matrix']
  tx=tuple(min(x,600) for x in rx) if variant else rx
  assert len(matrix)==20
  assert [e['offered_mbps'] for e in matrix if e['name'].startswith('UDP_TX')]==list(tx)
  assert [e['offered_mbps'] for e in matrix if e['name'].startswith('UDP_RX')]==list(rx)
  block=s[s.index('for round in 1 2 3; do'):s.index('# Mandatory FINAL ONLINE')]
  out=t/variant if variant else t/'standard';out.mkdir()
  script='''set -eu
check_network(){ :; }
capture(){ :; }
run_test(){ printf 'CASE'; printf '\t%s' "$@"; printf '\n'; }
'''+f'OUT={shlex.quote(str(out))}\nLABEL=AX210-2.0.0\nDEVICE=AX210\n'+block
  stdout=subprocess.check_output(['/bin/bash','-c',script],text=True)
  actual=[line.split('\t')[1:] for line in stdout.splitlines() if line.startswith('CASE\t')]
  assert len(actual)==60
  for i,args in enumerate(actual):
   entry=matrix[i%20];assert args[0]=='T%02d_'%(i%20+1)+entry['name']
   assert ('-R' in args)==entry['reverse']
   assert args[args.index('-t')+1]==str(entry['seconds'])
   if i%20<4:assert args[args.index('-P')+1]==str(entry['streams'])
   else:assert args[args.index('-b')+1]==str(entry['offered_mbps'])+'M' and args[args.index('-l')+1]=='1200'
  protocol=re.search(r"PROTOCOL_VERSION='([^']+)'",s)[1];assert protocol not in protocols;protocols.add(protocol)
  folder=re.search(r'OUT="\$\(mktemp -d "([^\n]+)',s)[1];assert folder not in folders;folders.add(folder)
  assert 'FAILURE_CAPTURE' in s and 'CAPTURE_EXIT_CODE' in s
  # Execute the original failure trap with fake capture/ifconfig; preserve
  # failure exit, partial data and archive, even when capture itself fails.
  failure=t/(variant+'-failure');failure.mkdir()
  finish=s[s.index('finish() {'):s.index('trap finish EXIT')]
  fixture='set -Eeuo pipefail\nrecord_protocol(){ :; }\ncapture(){ echo raw-fixture > "$1"; return 9; }\nifconfig(){ echo fixture-interface; }\n'+finish+'\n'+f'OUT={shlex.quote(str(failure))}\nARCHIVE={shlex.quote(str(failure)+".zip")}\nPROFILE=kgp\nIFACE=en2\ncompleted=19\nfinished_ok=0\nonline_status=NOT_RUN\nstage=ROUND_01/T20\ntrap finish EXIT\nexit 23\n'
  fp=subprocess.run(['/bin/bash','-c',fixture],capture_output=True,text=True)
  assert fp.returncode==23,fp.stderr
  assert 'CAPTURE_EXIT_CODE=9' in (failure/'FAILURE_CAPTURE.log').read_text()
  assert 'COMPLETED_IPERF_TESTS=19' in (failure/'RUN_STATUS.txt').read_text() and Path(str(failure)+'.zip').is_file()
  results[name]={'tests':len(actual),'per_round':matrix,'protocol':protocol,'sha256':sha(runner)}
 # Synthetic new-schema records: execute the real passive sampler against a
 # fake ioreg executable (no real hardware query required for decoder test).
 w=[42,1,1216,1,9,5,1,100]+[0xff00]*16+[0]*128;w[24:32]=[1,90,(9<<32)|5,4,0,0x101,0,1]
 q=[43,1,128,100,1,2,0x11dc1b,0,2401960,11,2,3,1,9,5,1]
 fake=t/'ioreg';fake.write_text('#!/bin/sh\n'+''.join("printf '%s\\n' "+shlex.quote('"'+name+'" = <'+struct.pack('<'+str(len(data))+'Q',*data).hex()+'>')+'\n' for name,data in [('KGP_AX_BA_LIFECYCLE',w),('KGP_AX_TX_RATE',q)]));fake.chmod(0o755)
 env=os.environ.copy();env['PATH']=str(t)+':'+env['PATH']
 subprocess.run(['python3',str(root/'AX_HE_DIAG_CAPTURE.py'),'--output',str(t/'capture.jsonl'),'--seconds','1','--interval','1'],env=env,check=True,stdout=subprocess.DEVNULL)
 rows=[json.loads(x) for x in (t/'capture.jsonl').read_text().splitlines()];assert rows[0]['KGP_AX_TX_RATE']['decoded']['mcs']==11 and rows[0]['KGP_AX_BA_LIFECYCLE']['events'][0]['firmware_status']==0x101
 # The new last-known schema keeps valid data with fresh0; old R6 remains decodable.
 q2=[43,2,136,100,1,2,0x11dc1b,10000,2401960,11,2,3,1,9,5,1,0]
 line='"KGP_AX_TX_RATE" = <'+struct.pack('<17Q',*q2).hex()+'>'
 fake.write_text('#!/bin/sh\nprintf \'%s\\n\' '+shlex.quote(line)+'\n')
 subprocess.run(['python3',str(root/'AX_HE_DIAG_CAPTURE.py'),'--output',str(t/'v2-capture.jsonl'),'--seconds','1','--interval','1'],env=env,check=True,stdout=subprocess.DEVNULL)
 v2=json.loads((t/'v2-capture.jsonl').read_text().splitlines()[0]);assert v2['KGP_AX_TX_RATE']['decoded']['valid']==1 and v2['KGP_AX_TX_RATE']['decoded']['fresh']==0
 # New AX-only handoff schema: exercise the real sampler and clock anchor.
 h=[44,1,5248,1,1000,1,1,64,10,0,5201,256,0,0,0,0]+[0]*640
 h[16:26]=[1,100,200,(1<<32)|1,(123<<32)|456,0xc0a8b2c5c0a8b287,5201|(50099<<16)|(0x18<<32)|(1<<48),53|(1<<16)|(2<<32),0x0d000000,0]
 line2='"KGP_AX_INPUT_HANDOFF" = <'+struct.pack('<656Q',*h).hex()+'>'
 fake.write_text('#!/bin/sh\nprintf \'%s\\n\' '+shlex.quote(line)+'\nprintf \'%s\\n\' '+shlex.quote(line2)+'\n')
 subprocess.run(['python3',str(root/'AX_HE_DIAG_CAPTURE.py'),'--output',str(t/'handoff-capture.jsonl'),'--seconds','1','--interval','1'],env=env,check=True,stdout=subprocess.DEVNULL)
 hr=json.loads((t/'handoff-capture.jsonl').read_text().splitlines()[0]);e=hr['KGP_AX_INPUT_HANDOFF']['events'][0]
 assert e['duration_ns']==100 and e['tcp_seq']==123 and e['source_port']==5201 and e['payload_prefix_hex']=='0d000000'
 assert hr['clock_anchor_uptime_ns']>0
 # BE200 mode skips AX decoding; missing AX properties remain optional in auto.
 subprocess.run(['python3',str(root/'AX_HE_DIAG_CAPTURE.py'),'--device','be200','--output',str(t/'be-capture.jsonl'),'--seconds','1','--interval','1'],env=env,check=True,stdout=subprocess.DEVNULL)
 be=json.loads((t/'be-capture.jsonl').read_text().splitlines()[0]);assert be['ax_diagnostics']=='not_applicable_be200' and 'KGP_AX_TX_RATE' not in be and 'KGP_AX_INPUT_HANDOFF' not in be
 line='"KGP_ZC19" = <'+struct.pack('<32Q',19,1,256,*([0]*29)).hex()+'>'
 fake.write_text('#!/bin/sh\nprintf \'%s\\n\' '+shlex.quote(line)+'\n')
 subprocess.run(['python3',str(root/'AX_HE_DIAG_CAPTURE.py'),'--output',str(t/'no-ax.jsonl'),'--seconds','1','--interval','1'],env=env,check=True,stdout=subprocess.DEVNULL)
 absent=json.loads((t/'no-ax.jsonl').read_text().splitlines()[0]);assert absent['ax_diagnostics']=='unavailable_optional'
(root/'evidence/PACKAGE_VALIDATION.json').write_text(json.dumps({'status':'PASS offline; no hardware tests','runners':results,'identity':'AX201/AX210/AX211-2.0.0 + original BE200-P10 + BE200-2.0.0 exact UUID/hash positives, wrong binary pin rejection; AX/BE200 PCI binding negatives; actual embedded health guards for both families in all 5 runners with no AX fields; wrong marker/mode/fault negatives','diagnostic_decoder':'actual script with synthetic 42/43 records; BE200 skips AX and missing AX fields remain optional'},indent=2)+'\n')
print('PASS bundle UUID/hashes; 5 actual 60-case matrices and metadata; distinct protocols/folders; labels/identity guards; 42/43 diagnostic decoder')
