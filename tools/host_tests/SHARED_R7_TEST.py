#!/usr/bin/env python3
"""Offline execution of real r4 Python guards and Bash boundary/round flow.
No driver, network, profiler, EFI or real measurement commands are used.
"""
import base64,hashlib,json,os,plistlib,re,shlex,struct,subprocess,sys,tempfile,unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]/'runner_reference'
SOURCE=(ROOT.parent/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh').read_text()
OLD=(ROOT/'ORIGINAL_SHARED_r3.sh').read_text()
UUID='01234567-89AB-CDEF-0123-456789ABCDEF'
BUNDLE='com.zxystd.AirportItlwm'
COMMIT='738d4c61b5a64433b27c1ab09dba94916ab77d8d'
FOREIGN=[{'IORegistryEntryName':'ARPT','IORegistryEntryID':123}]
def pyblock(s,tag):
    m=re.search(r"<<'"+tag+r"'\n(.*?)\n"+tag+r'(?=\n)',s,re.S)
    assert m,tag
    return m[1]
def guard(s):return pyblock(s[s.index('check_snapshot() {'):], 'PY')
def words(pub=100,mode=2):
    w=[0]*32;w[:8]=[19,1,256,pub,pub*1000000000,mode,1,2];w[11:13]=[1,2];return w
def props(w=None,label='ZC26'):
    return [{'KGP_ZC19':struct.pack('<32Q',*(w or words())),'KGP_RX_EXPERIMENT':('AirportItlwm-Tahoe-B1-ZC28-SERVICE' if label=='ZC28' else f'AirportItlwm-Tahoe-B1-{label}-PERF')}]
def runpy(code,args,env=None):
    return subprocess.run([sys.executable,'-B','-c',code,*map(str,args)],capture_output=True,text=True,env=env)

class R4(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.p=Path(self.t.name);self.seq=0
        self.env={**os.environ,'BENCHMARK_PROFILE':'kgp'}
    def check(self,properties,mode='mapped',label='ZC26',initial='NONE',previous='NONE',phase='boundary',external=False,identity=None,old=False):
        if external and properties==[{}]:properties=FOREIGN
        self.seq+=1;p=self.p/f's{self.seq}.plist';p.write_bytes(plistlib.dumps(properties))
        if identity is not None:p.with_suffix('.identity.json').write_text(json.dumps(identity))
        env=dict(self.env)
        if external:env.update(BENCHMARK_PROFILE='external',BENCHMARK_EXPECTED_UUID=UUID,BENCHMARK_BUNDLE_ID=BUNDLE,BENCHMARK_EXPECTED_SERVICE='ARPT')
        r=runpy(guard(OLD if old else SOURCE),[p,initial,previous,mode,label,phase],env)
        h=p.with_suffix('.health.json')
        return r,h,json.loads(h.read_text()) if h.exists() else None
    def accept(self,*a,**k):
        r,h,d=self.check(*a,**k);self.assertEqual(r.returncode,0,r.stderr);self.assertIsNotNone(d);return h,d
    def reject(self,*a,**k):
        r,h,d=self.check(*a,**k);self.assertNotEqual(r.returncode,0,r.stdout);self.assertFalse(h.exists());return r
    def identity(self):return dict(loaded_uuid=UUID,bundle_id=BUNDLE,candidate_hashes_unchanged=True,declared_source_commit=COMMIT)
    def files(self):
        e=self.p/'AirportItlwm';e.write_bytes(b'offline mock executable')
        i=self.p/'Info.plist';i.write_bytes(plistlib.dumps({'CFBundleIdentifier':BUNDLE,'CFBundleExecutable':'AirportItlwm'}))
        l=self.p/'loaded.txt';l.write_text(f'111 0 0x123 0x456 {BUNDLE} (1.0.0) {UUID} <1 2>\n')
        self.e,self.i,self.l=e,i,l
        self.es=hashlib.sha256(e.read_bytes()).hexdigest();self.is_=hashlib.sha256(i.read_bytes()).hexdigest()
    def idcheck(self,uuid=UUID,bundle=BUNDLE,commit=COMMIT):
        o=self.p/'identity.json';o.unlink(missing_ok=True)
        r=runpy(pyblock(SOURCE,'PY_EXTERNAL_IDENTITY'),[self.l,self.e,self.i,uuid,bundle,self.es,self.is_,commit,o])
        return r,json.loads(o.read_text()) if o.exists() else None

    def test_01_bash_and_all_embedded_python_compile(self):
        subprocess.run(['/bin/bash','-n',str(ROOT.parent/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh')],check=True)
        blocks=re.findall(r"<<'([A-Z_]+)'\n(.*?)\n\1(?=\n)",SOURCE,re.S)
        self.assertGreaterEqual(len(blocks),9)
        for name,code in blocks:
            if name!='HELP':compile(code,name,'exec')
    def test_02_traffic_and_online_flow_byte_identical(self):
        def part(s,a,b=None):return s.split(a,1)[1].split(b,1)[0] if b else s.split(a,1)[1]
        self.assertEqual(part(SOURCE,'run_test() {','stage="PREFLIGHT"'),part(OLD,'run_test() {','stage="PREFLIGHT"'))
        self.assertEqual(part(SOURCE,'stage="INITIAL_SNAPSHOT"'),part(OLD,'stage="INITIAL_SNAPSHOT"'))
        for prefix in ['ioreg ','sleep ','run_test ','networkQuality ']:
            self.assertEqual([x.strip() for x in SOURCE.splitlines() if x.strip().startswith(prefix)],[x.strip() for x in OLD.splitlines() if x.strip().startswith(prefix)])
    def test_03_strict_kgp_guard_body_byte_identical(self):
        start="if label in ('ZC19'"
        current=guard(SOURCE)[guard(SOURCE).index(start):]
        # Exactly the independently tested new identity is removed for the
        # conservation comparison; EVERY health/freshness check remains exact.
        current=current.replace(",'ZC28'", "")
        current=current.replace("'AirportItlwm-Tahoe-B1-ZC28-SERVICE' if label=='ZC28' else\n            ", "")
        self.assertEqual(current,guard(OLD)[guard(OLD).index(start):])
    def test_04_no_profile_does_not_accept_foreign(self):self.reject([{}],label='LAOBAMAC_738D4C6')
    def test_05_kgp_adjacent_repeats_final_progress_both_modes(self):
        for mode,n in [('mapped',2),('conventional',1)]:
            first,_=self.accept(props(words(100,n)),mode=mode);prev=first
            for v in [100,200,200,300,300,400]:prev,_=self.accept(props(words(v,n)),mode=mode,initial=first,previous=prev)
            self.accept(props(words(400,n)),mode=mode,initial=first,previous=prev,phase='final')
    def test_06_kgp_counter_rollback(self):
        first,_=self.accept(props());prev,_=self.accept(props(words(200)),initial=first,previous=first)
        self.reject(props(words(199)),initial=first,previous=prev)
    def test_07_kgp_final_must_exceed_initial(self):
        f,_=self.accept(props());self.reject(props(),initial=f,previous=f,phase='final')
    def test_08_kgp_faults_and_epoch_changes(self):
        f,_=self.accept(props())
        for field in [8,9,10,13,14,6,11,12]:
            w=words();w[field]+=1;self.reject(props(w),initial=f,previous=f)
    def test_09_kgp_wrong_mode_marker_malformed_ambiguous(self):
        self.reject(props(words(mode=1)));self.reject(props(label='ZC22'))
        for p in [[],props()*2,[{'KGP_ZC19':bytes(255)}],props()+[{'KGP_ZC10':bytes(10912)}]]:self.reject(p)
    def test_10_actual_zc8_archive_guards_both_modes(self):
        for rec in json.loads((ROOT/'FIXTURES_ZC8.json').read_text()):
            initial=previous='NONE'
            for snap in rec['snapshots']:
                p=plistlib.loads(base64.b64decode(snap['plist_base64']))
                h,d=self.accept(p,mode=rec['mode'],label='ZC8',initial=initial,previous=previous,phase='final' if snap['source_snapshot']=='FINAL' else 'boundary')
                self.assertFalse(d['NumericFaultAndResetCountersVerified']);previous=h
                if initial=='NONE':initial=h
    def test_11_external_health_honestly_unavailable(self):
        _,d=self.accept([{}],mode='unverified',label='LAOBAMAC_738D4C6',external=True,identity=self.identity())
        self.assertIsNone(d['Mode']);self.assertFalse(d['ActualDMAModeVerified']);self.assertFalse(d['NumericFaultAndResetCountersVerified']);self.assertEqual(d['KGPHealthStatus'],'UNAVAILABLE_NOT_ZERO')
        self.assertFalse(d['GenerationEpochAndPublicationProgressVerified']);self.assertFalse(d['SourceBuildCorrespondenceVerified'])
    def test_12_external_cannot_bypass_any_supported_kgp_markers(self):
        for p in [props(),[{'KGP_PC1':bytes(80)}],[{'KGP_RX_EXPERIMENT':'AirportItlwm-Tahoe-B1-ZC8-PERF'}],[{'KGP_ZC10':bytes(10912)}]]:
            p=[{**FOREIGN[0],**p[0]}]
            r=self.reject(p,mode='unverified',label='LAOBAMAC',external=True,identity=self.identity());self.assertIn('cannot bypass KGP',r.stderr)
    def test_13_external_cannot_claim_kgp_label(self):self.reject([{}],mode='unverified',label='ZC26',external=True,identity=self.identity())
    def test_14_external_cannot_claim_mapped_or_conventional(self):
        for m in ['mapped','conventional']:self.reject([{}],mode=m,label='LAOBAMAC',external=True,identity=self.identity())
    def test_15_external_requires_identity_at_boundary(self):
        self.reject([{}],mode='unverified',label='LAOBAMAC',external=True)
        for key,v in [('loaded_uuid','WRONG'),('bundle_id','wrong.id'),('candidate_hashes_unchanged',False)]:
            d=self.identity();d[key]=v;self.reject([{}],mode='unverified',label='LAOBAMAC',external=True,identity=d)
    def test_16_external_identity_change_rejected(self):
        f,_=self.accept([{}],mode='unverified',label='LAOBAMAC',external=True,identity=self.identity())
        d=self.identity();d['declared_source_commit']='0'*40
        self.reject([{}],mode='unverified',label='LAOBAMAC',external=True,identity=d,initial=f,previous=f)
    def test_17_boundary_identity_accepts_exact(self):
        self.files();r,d=self.idcheck();self.assertEqual(r.returncode,0,r.stderr);self.assertTrue(d['candidate_hashes_unchanged'])
    def test_18_boundary_identity_wrong_uuid_rejected(self):
        self.files();r,d=self.idcheck(uuid='FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF');self.assertNotEqual(r.returncode,0);self.assertIsNone(d)
    def test_19_boundary_identity_exact_bundle_and_uniqueness(self):
        self.files()
        for text in ['',self.l.read_text().replace(BUNDLE,BUNDLE+'.other'),self.l.read_text()*2]:
            self.l.write_text(text);r,d=self.idcheck();self.assertNotEqual(r.returncode,0);self.assertIsNone(d)
    def test_20_boundary_identity_executable_changed(self):
        self.files();self.e.write_bytes(b'changed');r,d=self.idcheck();self.assertNotEqual(r.returncode,0);self.assertIsNone(d)
    def test_21_boundary_identity_info_changed(self):
        self.files();self.i.write_bytes(plistlib.dumps({'CFBundleIdentifier':'another.bundle'}));r,d=self.idcheck();self.assertNotEqual(r.returncode,0);self.assertIsNone(d)
    def test_22_source_declaration_syntax(self):
        self.files();r,d=self.idcheck(commit='main');self.assertNotEqual(r.returncode,0);self.assertIsNone(d)
    def test_23_shell_front_end_fails_closed(self):
        for env,args in [({'BENCHMARK_PROFILE':'oops'},['ZC26']),({'BENCHMARK_PROFILE':'external','EXPECTED_MODE':'mapped'},['/tmp/missing.kext']),({'BENCHMARK_PROFILE':'external','EXPECTED_MODE':'unverified','EXTERNAL_SOURCE_REF':COMMIT},['ZC26']),({'BENCHMARK_PROFILE':'external','EXPECTED_MODE':'unverified','EXTERNAL_SOURCE_REF':'main'},['/tmp/missing.kext']),({'BENCHMARK_PROFILE':'kgp','EXPECTED_MODE':'unverified'},['ZC26'])]:
            r=subprocess.run(['/bin/bash',str(ROOT.parent/'AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh'),*args],env={**self.env,**env},capture_output=True,text=True)
            self.assertEqual(r.returncode,2,r.stdout+r.stderr);self.assertIn('STOP:',r.stdout+r.stderr)
    def campaign(self,external):
        self.files();out=self.p/'out';out.mkdir();p=self.p/'fixture.plist';p.write_bytes(plistlib.dumps(FOREIGN))
        functions='check_snapshot() {\n'+SOURCE.split('check_snapshot() {\n',1)[1].split('\ncheck_network() {',1)[0]
        rounds=SOURCE.split('stage="INITIAL_SNAPSHOT"\n',1)[1].split('# Mandatory FINAL ONLINE',1)[0]
        final=SOURCE.split('stage="FINAL_SNAPSHOT"\n',1)[1]
        initial='''set -Eeuo pipefail
OUT={out}
PROFILE={profile}
MODE={mode}
LABEL={label}
UUID={uuid}
BUNDLE_ID={bundle}
EXECUTABLE={exe}
KEXT={kext}
EXTERNAL_EXE_SHA={esha}
EXTERNAL_INFO_SHA={isha}
EXTERNAL_SOURCE_REF={commit}
DEVICE=AX210
IOREG_SERVICE=ARPT
INITIAL_HEALTH=""
PREVIOUS_HEALTH=""
completed=0
cursor=0
identity_calls=0
finished_ok=0
'''.format(out=shlex.quote(str(out)),profile='external' if external else 'kgp',mode='unverified' if external else 'mapped',label='LAOBAMAC' if external else 'ZC26',uuid=UUID,bundle=BUNDLE,exe=shlex.quote(str(self.e)),kext=shlex.quote(str(self.p/'Candidate.kext')),esha=self.es,isha=self.is_,commit=COMMIT)
        k=self.p/'Candidate.kext/Contents';k.mkdir(parents=True);(k/'Info.plist').write_bytes(self.i.read_bytes())
        if external:
            stubs=f'''ioreg() {{ cat {shlex.quote(str(p))}; cursor=$((cursor+1)); }}
kmutil() {{ cat {shlex.quote(str(self.l))}; identity_calls=$((identity_calls+1)); }}
'''
        else:
            for i,v in enumerate([100,100,200,200,300,300,400,400]):(self.p/f'{i}.plist').write_bytes(plistlib.dumps(props(words(v))))
            stubs=f'''ioreg() {{ cat {shlex.quote(str(self.p))}/"$cursor.plist"; cursor=$((cursor+1)); }}
kmutil() {{ echo UNEXPECTED_KMUTIL >&2; return 99; }}
'''
        script=initial+stubs+'check_network() { :; }\nrun_test() { completed=$((completed+1)); }\n'+functions+'\nstage="INITIAL_SNAPSHOT"\n'+rounds+'\nstage="FINAL_SNAPSHOT"\n'+final+f'\n[[ "$completed" == 42 && "$cursor" == 8 && "$identity_calls" == {8 if external else 0} && "$finished_ok" == 1 ]]\n'
        return subprocess.run(['/bin/bash','-c',script],capture_output=True,text=True)
    def test_24_real_external_shell_flow_42_cases_8_boundaries(self):
        r=self.campaign(True);self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_25_real_kgp_shell_flow_42_cases_no_external_queries(self):
        r=self.campaign(False);self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_26_guard_differential_against_r3(self):
        for m,n in [('mapped',2),('conventional',1)]:
            for w in [words(100,n),words(0,n),words(101,3-n)]:
                old=self.check(props(w),mode=m,old=True);new=self.check(props(w),mode=m)
                self.assertEqual((old[0].returncode,old[2]),(new[0].returncode,new[2]))
    def test_27_external_bundle_metadata_validation(self):
        self.files();r=runpy(pyblock(SOURCE,'PY_EXTERNAL_BUNDLE'),[self.i]);self.assertEqual(r.returncode,0,r.stderr)
        for d in [{'CFBundleIdentifier':'bad value','CFBundleExecutable':'AirportItlwm'},{'CFBundleIdentifier':BUNDLE,'CFBundleExecutable':'other'}]:
            self.i.write_bytes(plistlib.dumps(d));self.assertNotEqual(runpy(pyblock(SOURCE,'PY_EXTERNAL_BUNDLE'),[self.i]).returncode,0)
    def test_28_external_snapshot_must_select_one_real_device(self):
        for p in [[],FOREIGN*2,[{'IORegistryEntryName':'WRONG','IORegistryEntryID':123}],[{'IORegistryEntryName':'ARPT'}]]:
            self.reject(p,mode='unverified',label='LAOBAMAC',external=True,identity=self.identity())
    def test_29_external_device_instance_change_rejected(self):
        f,_=self.accept(FOREIGN,mode='unverified',label='LAOBAMAC',external=True,identity=self.identity())
        self.reject([{'IORegistryEntryName':'ARPT','IORegistryEntryID':124}],mode='unverified',label='LAOBAMAC',external=True,identity=self.identity(),initial=f,previous=f)
    def test_30_zc13_zc18_numeric_guard_both_modes(self):
        for label in ['ZC13','ZC18']:
            for m,n in [('mapped',2),('conventional',1)]:
                w=[0]*1364;w[:3]=[10,2,10912];w[5]=n;w[6]=1;w[23]=1;w[63]=1
                p=[{'KGP_ZC10':struct.pack('<1364Q',*w),'KGP_RX_EXPERIMENT':('AirportItlwm-Tahoe-B1-ZC28-SERVICE' if label=='ZC28' else f'AirportItlwm-Tahoe-B1-{label}-PERF')}]
                f,_=self.accept(p,mode=m,label=label)
                w[63]+=1;p[0]['KGP_ZC10']=struct.pack('<1364Q',*w)
                r,h,d=self.check(p,mode=m,label=label,initial=f,previous=f)
                self.assertNotEqual(r.returncode,0);self.assertIn('generation/reset changed',r.stderr)
                # The inherited 10/2 decoder writes a diagnostic before rejecting;
                # capture never promotes it to PREVIOUS_HEALTH on that failure.
                self.assertEqual(json.loads(f.read_text())['ResetSerial'],1)
    def test_31_protocol_provenance_and_change_detection(self):
        source=self.p/'runner.sh';source.write_text(SOURCE);dest=self.p/'archive';dest.mkdir()
        code=pyblock(SOURCE,'PY_PROTOCOL')
        args=[source,dest,'AIRPORTITLWM_3X14_NO_PROFILER_V1','shared-2026-10-20-r6-numbered','initial','LAOBAMAC','/tmp/AirportItlwm.kext',UUID,'AX210','en2','192.168.178.135','192.168.178.33','5201','unverified','2','ARPT','/tmp','/tmp','3.2','external',COMMIT,BUNDLE,'mapped']
        r=runpy(code,args);self.assertEqual(r.returncode,0,r.stderr)
        o=json.loads((dest/'EFFECTIVE_SETTINGS.json').read_text());self.assertEqual(o['declared_external_dma_configuration'],'mapped');self.assertFalse(o['external_actual_dma_mode_verified']);self.assertIsNone(o['expected_actual_mode']);self.assertFalse(o['numeric_kgp_health_available']);self.assertEqual(len(o['tests_per_round']),14)
        args[5]='ZC8';args[19]='kgp';args[13]='mapped';args[4]='prepared'
        r=runpy(code,args);self.assertEqual(r.returncode,0,r.stderr)
        o=json.loads((dest/'EFFECTIVE_SETTINGS.json').read_text());self.assertTrue(o['kgp_guard_profile_enabled']);self.assertFalse(o['numeric_kgp_health_available'])
        source.write_text(SOURCE+'\n# source changed during fixture\n');r=runpy(code,args);self.assertNotEqual(r.returncode,0)

    def shortcut(self,label,mode,extra=None):
        block=SOURCE.split('# r6 numbered shortcut expansion BEGIN\n',1)[1].split('# r5 shortcut expansion END',1)[0]
        code='set -Eeuo pipefail\nset -- '+shlex.quote(label)+' '+shlex.quote(mode)+'\n'+block+'\nprintf "%s\n" "$1" "${EXPECTED_MODE:-}" "${BENCHMARK_PROFILE:-kgp}" "$EXTERNAL_DECLARED_MODE" "${EXTERNAL_SOURCE_REF:-}" "${CANDIDATE_LABEL:-}"\n'
        env={**os.environ,'HOME':'/Users/kgp'}
        for k in ['AIRPORTITLWM_ROOT','EXPECTED_MODE','BENCHMARK_PROFILE','LAOBAMAC_SOURCE_REF','LAOBAMAC_KEXT','CANDIDATE_LABEL','EXTERNAL_SOURCE_REF','EXTERNAL_DECLARED_MODE','LAOBAMAC_BUILD_NUMBER']:env.pop(k,None)
        env.update(extra or {})
        return subprocess.run(['/bin/bash','-c',code],env=env,capture_output=True,text=True)
    def test_32_simple_laobamac_both_modes(self):
        for mode in ['mapped','conventional']:
            r=self.shortcut('LAOBAMAC',mode);self.assertEqual(r.returncode,0,r.stderr);out=r.stdout.splitlines()
            self.assertEqual(out[0],'/Users/kgp/Desktop/AirportItlwm/LAOBAMAC-738d4c6/AirportItlwm.kext')
            self.assertEqual(out[1:5],['unverified','external',mode,COMMIT]);self.assertEqual(out[5],'LAOBAMAC_738d4c6_'+mode)
    def test_33_simple_kgp_both_modes(self):
        for mode in ['mapped','conventional']:
            r=self.shortcut('ZC26',mode);self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout.splitlines()[:4],['ZC26',mode,'kgp','unspecified'])
    def test_34_simple_build_override_and_bad_mode(self):
        ref='1'*40;r=self.shortcut('LAOBAMAC','mapped',{'LAOBAMAC_KEXT':'/tmp/test build/AirportItlwm.kext','LAOBAMAC_SOURCE_REF':ref})
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout.splitlines()[0],'/tmp/test build/AirportItlwm.kext');self.assertEqual(r.stdout.splitlines()[4],ref)
        self.assertNotEqual(self.shortcut('LAOBAMAC','oops').returncode,0)

    def numbered(self,number,mode,extra=None):
        block=SOURCE.split('# r6 numbered shortcut expansion BEGIN\n',1)[1].split('# r5 shortcut expansion END',1)[0]
        code='set -Eeuo pipefail\nset -- LAOBAMAC '+shlex.quote(number)+' '+shlex.quote(mode)+'\n'+block+'\nprintf "%s\n" "$1" "$EXPECTED_MODE" "$BENCHMARK_PROFILE" "$EXTERNAL_DECLARED_MODE" "$EXTERNAL_SOURCE_REF" "$CANDIDATE_LABEL" "${LAOBAMAC_BUILD_NUMBER:-}"\n'
        env={**os.environ,'HOME':'/Users/kgp'}
        for k in ['AIRPORTITLWM_ROOT','EXPECTED_MODE','BENCHMARK_PROFILE','LAOBAMAC_SOURCE_REF','LAOBAMAC_KEXT','CANDIDATE_LABEL','EXTERNAL_SOURCE_REF','EXTERNAL_DECLARED_MODE','LAOBAMAC_BUILD_NUMBER']:env.pop(k,None)
        env.update(extra or {})
        return subprocess.run(['/bin/bash','-c',code],env=env,capture_output=True,text=True)
    def test_35_numbered_builds_both_modes(self):
        refs={'44':'d695c971420cc8af0daa286dcd3297e8236c0a63','47':'bbe368ec7d55a5395d08c729e88eeaabebae8255','53':COMMIT}
        for n,ref in refs.items():
            self.assertEqual(len(ref),40)
            for mode in ['mapped','conventional']:
                r=self.numbered(n,mode);self.assertEqual(r.returncode,0,r.stderr);out=r.stdout.splitlines()
                self.assertEqual(out[0],'/Users/kgp/Desktop/Laobamac/'+n+'/AirportItlwm.kext')
                self.assertEqual(out[1:5],['unverified','external',mode,ref])
                self.assertEqual(out[5],'LAOBAMAC_'+n+'_'+ref[:7]+'_'+mode);self.assertEqual(out[6],n)
    def test_36_numbered_invalid_selection_and_source_conflict(self):
        for number,mode,extra in [('46','mapped',{}),('53','auto',{}),('44','mapped',{'LAOBAMAC_SOURCE_REF':COMMIT})]:
            self.assertNotEqual(self.numbered(number,mode,extra).returncode,0)
    def test_37_numbered_path_override(self):
        for n in ['44','47','53']:
            r=self.numbered(n,'conventional',{'LAOBAMAC_KEXT':'/tmp/other build/AirportItlwm.kext'})
            self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout.splitlines()[0],'/tmp/other build/AirportItlwm.kext')

    def test_38_zc28_service_identity_actual_both_modes(self):
        for mode,code in [('mapped',2),('conventional',1)]:
            self.accept(props(words(100,code),label='ZC28'),mode=mode,label='ZC28')
            self.reject(props(words(100,code),label='ZC26'),mode=mode,label='ZC28')
            self.reject(props(words(100,3-code),label='ZC28'),mode=mode,label='ZC28')
    def test_39_zc28_service_path_and_old_numbered_external_preserved(self):
        self.assertIn('B1-ZC28-SERVICE',SOURCE)
        self.assertIn('ZC28|ZC27|ZC26|',SOURCE)
        self.assertIn('shared-2026-10-20-r7-service',SOURCE)
        self.assertIn('ACTUAL_DMA_MODE_VERIFIED=false',SOURCE)

if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(R4)
    r=unittest.TextTestRunner(verbosity=2).run(suite)
    (Path(os.environ.get('RUNNER_TEST_OUTPUT', tempfile.gettempdir()))/'ZC28_RUNNER_VALIDATION.json').write_text(json.dumps({'tests_run':r.testsRun,'failures':len(r.failures),'errors':len(r.errors),'success':r.wasSuccessful(),'runner_sha256':hashlib.sha256(SOURCE.encode()).hexdigest(),'scope':'Offline real embedded guard and shell round/capture flow; no Mac hardware or physical benchmark executed'},indent=2)+'\n')
    sys.exit(0 if r.wasSuccessful() else 1)
