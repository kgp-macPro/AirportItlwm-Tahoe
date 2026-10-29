#!/usr/bin/env python3
"""Differential test of real frozen-ZC26/current-ZC28 iwx_rs_update bodies.
Mocks only node/IOKit environment, diagnostic sink and removed pretty/log sinks.
This is state-equivalence validation, not a radio/throughput or DMA model.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
BASELINE='48e9a31e68c328b5f19e5bdc81901e900d350738'
REPO=Path('/Users/kgp/Developer/OpenIntelWireless/AirportItlwm-Tahoe-1.0.0-Experiments/TRACK-B1-ZC8-PERF')
CPP='itlwm/hal_iwx/ItlIwx.cpp';REG='itlwm/hal_iwx/if_iwxreg.h';VAR='itlwm/hal_iwx/if_iwxvar.h'
def git(path):return subprocess.check_output(['git','-C',str(REPO),'show',BASELINE+':'+path],text=True)
def masked(t):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),t,flags=re.S)
def extract(t,name):
 m=masked(t);x=re.search(r'(?:static\s+)?(?:uint8_t|uint32_t|int|void)\s+(?:ItlIwx::\s*)?'+name+r'\s*\(',m);assert x,name
 a=m.index('{',x.end());b=a+1;n=1
 while n:n+=(m[b]=='{')-(m[b]=='}');b+=1
 return t[x.start():b]
def block(t,start):
 p=t.index(start);m=masked(t);a=m.index('{',p);b=a+1;depth=1
 while depth:depth+=(m[b]=='{')-(m[b]=='}');b+=1
 end=t.index(';',b)+1;return t[p:end]
def decomment(t):return re.sub(r'\s+',' ',masked(t)).strip()
def digest(t):return hashlib.sha256(t.encode()).hexdigest()
args=argparse.ArgumentParser();args.add_argument('sanitizer',nargs='?',default='address,undefined');args.add_argument('--require-production',action='store_true');args.add_argument('--emit-model',type=Path);a=args.parse_args()
oldsrc=git(CPP);reg=git(REG);var=git(VAR);old=extract(oldsrc,'iwx_rs_update')
prototype=old.replace('    char pretty_rate[100];\n','')
chunk='''    if (notifver < 3 || notifver == IWX_FW_CMD_VER_UNKNOWN) {
        iwx_rs_pretty_print_rate_v1(pretty_rate, sizeof(pretty_rate),
                                    rate_n_flags);
        rate_n_flags = iwx_new_rate_from_v1(rate_n_flags);
    } else {
        iwx_rs_pretty_print_rate(pretty_rate, sizeof(pretty_rate),
                                 rate_n_flags);
    }
    XYLog("%s new rate: %s\\n", __FUNCTION__, pretty_rate);'''
assert chunk in prototype
prototype=prototype.replace(chunk,'''    if (notifver < 3 || notifver == IWX_FW_CMD_VER_UNKNOWN)
        rate_n_flags = iwx_new_rate_from_v1(rate_n_flags);
    zc28Note(5, rate_n_flags, notifver, 0);''')
# Validation defaults to the actual worktree containing this test. A staged
# prototype is an independent expectation only, never the tested candidate.
root=os.environ.get('ZC28_SOURCE_ROOT',str(Path(__file__).resolve().parents[2]))
actualsrc=(Path(root)/CPP).read_text();new=extract(actualsrc,'iwx_rs_update');kind='actual_current_source'
assert all(x not in masked(new) for x in ['pretty_rate','iwx_rs_pretty_print_rate','XYLog'])
assert 'zc28Note(5,' in new
# The rate-update suffix and the conversion function must remain source-identical.
assert decomment(old[old.index('    format ='):])==decomment(new[new.index('    format ='):])
conv=extract(oldsrc,'iwx_new_rate_from_v1')
if actualsrc:assert decomment(conv)==decomment(extract(actualsrc,'iwx_new_rate_from_v1'))
# Preserve actual rate-format constants, enum, firmware notification and rate table.
defs=[]
lines=reg.splitlines();i=0
while i<len(lines):
 line=lines[i]
 if re.match(r'^#define\s+IWX_(RATE_|ANT_|TLC_NOTIF_|TLC_MNG_UPDATE_NOTIF|DATA_PATH_GROUP|FW_CMD_VER_UNKNOWN)',line):
  while line.endswith('\\'):i+=1;line+='\n'+lines[i]
  defs.append(line)
 i+=1
rateenum=block(reg,'enum {\n    IWX_RATE_1M_INDEX')
ratetable=block(oldsrc,'const struct iwx_rate {')
notif=block(reg,'struct iwx_tlc_update_notif {')
lookup=extract(oldsrc,'iwx_lookup_notif_ver').replace('ItlIwx::\n','').replace('iwx_lookup_notif_ver(', 'actual_lookup_notif_ver(',1)
station=re.search(r'^#define\s+IWX_STATION_ID\s+\S+',var,re.M)[0]
head=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
using u8=uint8_t;using u32=uint32_t;
#define BIT(n) (1U<<(n))
#define nitems(a) (sizeof(a)/sizeof((a)[0]))
#define ARRAY_SIZE(a) nitems(a)
#define WARN_ON(x) ((void)(x))
#define le32toh(x) (x)
#define __packed __attribute__((packed))
#define IEEE80211_RATE_VAL 0x7f
'''
model=head+'\n'+'\n'.join(defs)+'\n'+station+'\n'+rateenum+'\n'+ratetable+'\n#define IWX_RATE_INVALID IWX_RATE_COUNT\n#define IWX_LAST_HE_RATE IWX_RATE_MCS_11_INDEX\n'+notif+r'''
struct ieee80211_rateset { uint8_t rs_nrates; uint8_t rs_rates[32]; };
struct ieee80211_node { uint8_t prefix[32]; uint32_t ni_flags, ni_xflags; int ni_txmcs,ni_txrate; ieee80211_rateset ni_rates; uint8_t suffix[128]; };
struct ieee80211com { ieee80211_node *ic_bss; uint32_t unrelated_state; };
struct iwx_fw_cmd_version { uint8_t group,cmd,cmd_ver,notif_ver; };
struct iwx_softc { ieee80211com sc_ic; iwx_fw_cmd_version *cmd_versions; int n_cmd_versions; unsigned harness_mode,harness_family; uint64_t unrelated_cookie; };
static std::atomic<uint64_t> pretty1{0},pretty2{0},cases{0},status_events{0};
int iwx_rs_pretty_print_rate_v1(char *p,int n,uint32_t) { pretty1.fetch_add(1,std::memory_order_relaxed);assert(n==100);p[0]='v';p[1]=0;return 1; }
int iwx_rs_pretty_print_rate(char *p,int n,uint32_t) { pretty2.fetch_add(1,std::memory_order_relaxed);assert(n==100);p[0]='n';p[1]=0;return 1; }
'''+extract(oldsrc,'iwx_rs_extract_rate')+'\n'+extract(oldsrc,'iwx_hwrate_to_plcp_idx')+'\n'+conv+'\n'+lookup+r'''
struct Env {
 unsigned lookups=0,logs=0,status=0;uint32_t observed_rate=0,observed_version=0,observed_arg=0,observed_event=0;
 uint8_t iwx_lookup_notif_ver(iwx_softc *s,uint8_t g,uint8_t c) { ++lookups;return actual_lookup_notif_ver(s,g,c); }
 void zc28Note(unsigned ev,uint32_t rate,uint32_t ver,uint32_t arg) { ++status;observed_event=ev;observed_rate=rate;observed_version=ver;observed_arg=arg;status_events.fetch_add(1,std::memory_order_relaxed); }
};
struct Baseline:Env { void iwx_rs_update(iwx_softc*,iwx_tlc_update_notif*); };
struct Candidate:Env { bool bzDevice(){return false;}void iwx_bz_port_rs_update(iwx_softc*,iwx_tlc_update_notif*){assert(false);}
 void iwx_rs_update(iwx_softc*,iwx_tlc_update_notif*); };
#define XYLog(...) (++logs)
'''+old.replace('ItlIwx::','Baseline::')+'\n'+new.replace('ItlIwx::','Candidate::')+r'''
#undef XYLog
static uint64_t rnd(uint64_t &x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
static void run(uint32_t rate,uint8_t ver,uint32_t flags,uint8_t station,unsigned mode,unsigned family,uint64_t seed,bool matching=true){
 ieee80211_node initial{};uint64_t state=seed;
 for(auto &x:initial.prefix)x=uint8_t(rnd(state));for(auto &x:initial.suffix)x=uint8_t(rnd(state));
 initial.ni_flags=uint32_t(rnd(state));initial.ni_xflags=uint32_t(rnd(state));initial.ni_txrate=173;initial.ni_txmcs=37;
 // Permutations, basic-rate marker, absent rates and empty ratesets.
 unsigned count=unsigned(seed%4)==0?0:unsigned(seed%4)==1?1:unsigned(seed%4)==2?12:32;initial.ni_rates.rs_nrates=uint8_t(count);
 for(unsigned i=0;i<32;i++)initial.ni_rates.rs_rates[i]=uint8_t(i<nitems(iwx_rates)?iwx_rates[(i+seed)%nitems(iwx_rates)].rate:(rnd(state)&127));
 for(unsigned i=0;i<count;i++)initial.ni_rates.rs_rates[i]|=uint8_t((i&1)?0x80:0);
 ieee80211_node x=initial,y=initial;ieee80211com ix{&x,0xfedcba98},iy{&y,0xfedcba98};
 iwx_fw_cmd_version table[3]={{IWX_DATA_PATH_GROUP,uint8_t(IWX_TLC_MNG_UPDATE_NOTIF^1),91,2},{uint8_t(IWX_DATA_PATH_GROUP^1),IWX_TLC_MNG_UPDATE_NOTIF,92,1},{IWX_DATA_PATH_GROUP,IWX_TLC_MNG_UPDATE_NOTIF,93,ver}};
 iwx_softc sx{ix,table,matching?3:2,mode,family,0x9988776655443322ULL},sy{iy,table,matching?3:2,mode,family,0x9988776655443322ULL};
 iwx_tlc_update_notif n{};n.sta_id=station;n.flags=flags;n.rate=rate;n.amsdu_size=0x12345678;n.amsdu_enabled=0xabcdef13;for(auto &v:n.reserved)v=0x5a;
 auto original=n;auto oldsx=sx,oldsy=sy;Baseline b;Candidate c;
 b.iwx_rs_update(&sx,&n);c.iwx_rs_update(&sy,&n);
 assert(!memcmp(&x,&y,sizeof(x)));assert(!memcmp(&n,&original,sizeof(n)));
 assert(!memcmp(&sx,&oldsx,sizeof(sx))&&!memcmp(&sy,&oldsy,sizeof(sy)));
 assert(!memcmp(x.prefix,initial.prefix,sizeof(x.prefix))&&!memcmp(x.suffix,initial.suffix,sizeof(x.suffix)));
 assert(x.ni_flags==initial.ni_flags&&x.ni_xflags==initial.ni_xflags);assert(!memcmp(&x.ni_rates,&initial.ni_rates,sizeof(x.ni_rates)));
 bool eligible=station==IWX_STATION_ID&&(flags&IWX_TLC_NOTIF_FLAG_RATE);
 assert(b.lookups==unsigned(eligible)&&c.lookups==unsigned(eligible));assert(b.logs==unsigned(eligible)&&c.logs==0);assert(c.status==unsigned(eligible));
 if(eligible){uint8_t effective=matching?ver:IWX_FW_CMD_VER_UNKNOWN;
  uint32_t expected=(effective<3||effective==IWX_FW_CMD_VER_UNKNOWN)?iwx_new_rate_from_v1(rate):rate;
  assert(c.observed_event==5&&c.observed_rate==expected&&c.observed_version==effective&&c.observed_arg==0);
 }else assert(!memcmp(&x,&initial,sizeof(x)));
 cases.fetch_add(1,std::memory_order_relaxed);
}
static void known(){
 // Independent explicit MCS expectations for every supported new modulation.
 iwx_fw_cmd_version entry{IWX_DATA_PATH_GROUP,IWX_TLC_MNG_UPDATE_NOTIF,1,3};
 for(auto m:{IWX_RATE_MCS_HT_MSK,IWX_RATE_MCS_VHT_MSK,IWX_RATE_MCS_HE_MSK,IWX_RATE_MCS_EHT_MSK})for(unsigned nss=0;nss<2;nss++)for(unsigned mcs=0;mcs<(m==IWX_RATE_MCS_HT_MSK?8:16);mcs++){
  ieee80211_node n{};n.ni_txmcs=99;n.ni_txrate=77;n.ni_flags=0xfedcba98;iwx_softc s{{&n,0},&entry,1,1,210,0};iwx_tlc_update_notif v{};v.flags=IWX_TLC_NOTIF_FLAG_RATE;v.rate=m|(nss<<IWX_RATE_MCS_NSS_POS)|mcs;Candidate c;c.iwx_rs_update(&s,&v);
  assert(n.ni_txmcs==int(m==IWX_RATE_MCS_HT_MSK?nss*8+mcs:mcs));assert(n.ni_txrate==77&&n.ni_flags==0xfedcba98);
 }
 // Old-format conversion remains mandatory, including unknown version.
 for(uint8_t ver:{uint8_t(0),uint8_t(1),uint8_t(2),uint8_t(IWX_FW_CMD_VER_UNKNOWN)}){
  entry.notif_ver=ver;ieee80211_node n{};n.ni_txmcs=99;n.ni_txrate=77;iwx_softc s{{&n,0},&entry,1,2,210,0};iwx_tlc_update_notif v{};v.flags=IWX_TLC_NOTIF_FLAG_RATE;v.rate=IWX_RATE_MCS_HT_MSK_V1|IWX_RATE_HT_MCS_MIMO2_MSK|7;Candidate c;c.iwx_rs_update(&s,&v);assert(n.ni_txmcs==15&&n.ni_txrate==77);
 }
 // Modern legacy path still finds the same rateset entry/basic-rate masking.
 {entry.notif_ver=3;ieee80211_node n{};n.ni_txrate=77;n.ni_txmcs=99;n.ni_rates.rs_nrates=3;n.ni_rates.rs_rates[0]=12;n.ni_rates.rs_rates[1]=uint8_t(108|0x80);n.ni_rates.rs_rates[2]=24;iwx_softc s{{&n,0},&entry,1,1,201,0};iwx_tlc_update_notif v{};v.flags=1;v.rate=IWX_RATE_MCS_LEGACY_OFDM_MSK|IWX_RATE_54M_PLCP;Candidate c;c.iwx_rs_update(&s,&v);assert(n.ni_txrate==1&&n.ni_txmcs==99);}
}
int main(){known();
 const uint8_t versions[]={0,1,2,3,4,12,IWX_FW_CMD_VER_UNKNOWN,255};
 const std::pair<unsigned,unsigned> modes[]={{1,201},{1,210},{2,210},{1,211},{2,211}};
 uint64_t seed=0x182726;
 for(auto f:modes)for(uint8_t v:versions){
  // Complete low-byte space for every modulation: legacy/HT/VHT/HE/EHT plus unknown encodings; flag bits do not change forwarding behavior.
  for(unsigned fmt=0;fmt<8;fmt++)for(unsigned low=0;low<256;low++)run((fmt<<8)|low|IWX_RATE_MCS_CHAN_WIDTH_80|IWX_RATE_MCS_ANT_AB_MSK,v,1,0,f.first,f.second,rnd(seed));
  for(unsigned kind=0;kind<3;kind++)for(unsigned low=0;low<32;low++){
   uint32_t raw=(kind==0?IWX_RATE_MCS_HT_MSK_V1:kind==1?IWX_RATE_MCS_VHT_MSK_V1:IWX_RATE_MCS_HE_MSK_V1)|low|IWX_RATE_MCS_SGI_MSK_V1|IWX_RATE_MCS_LDPC_MSK_V1;
   run(raw,v,1|IWX_TLC_NOTIF_FLAG_AMSDU,0,f.first,f.second,rnd(seed));
  }
  // Correct station without RATE, wrong station, unrelated flags and unknown-version lookup fallback.
  for(uint32_t flags:{0U,uint32_t(IWX_TLC_NOTIF_FLAG_AMSDU),0x80000000U,1U,0xffffffffU})for(uint8_t sta:{uint8_t(0),uint8_t(1),uint8_t(255)})run(uint32_t(rnd(seed)),v,flags,sta,f.first,f.second,rnd(seed));
  run(uint32_t(rnd(seed)),v,1,0,f.first,f.second,rnd(seed),false);
 }
 // Independent node ownership, shared fixed atomic diagnostic sink. No claim that concurrent writes to the same production node are permitted.
 std::atomic<bool> done{false};std::thread observer([&]{while(!done.load(std::memory_order_acquire)){(void)status_events.load(std::memory_order_relaxed);}});
 std::vector<std::thread> threads;
 for(unsigned t=0;t<4;t++)threads.emplace_back([t]{uint64_t state=0x999965+t;for(unsigned i=0;i<5000;i++)run(uint32_t(rnd(state)),uint8_t(i%5),i%7?1U:0U,uint8_t(i%11?0:1),i&1?1:2,210,rnd(state));});
 for(auto &t:threads)t.join();done.store(true,std::memory_order_release);observer.join();
 std::cout<<"PASS actual iwx_rs_update differential cases="<<cases.load()<<"; frozen pretty-v1/new calls="<<pretty1.load()<<"/"<<pretty2.load()<<"; candidate logging/formatting calls=0; bounded status events="<<status_events.load()<<"; all node bytes/rates/flags preserved except identical legacy txrate or txmcs updates. Qualified mode/family scenarios, all modulation low-byte encodings, notif version and STA/flag guards, unknown fallback, invalid/no-op encodings, and20000 independent-node concurrent cases. No physical throughput/cache/DMA/rate-control proof.\n";
}
'''
if a.emit_model:a.emit_model.write_text(model)
proof={'test_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'baseline':BASELINE,'baseline_cpp_sha256':digest(oldsrc),'baseline_function_sha256':digest(old),'conversion_sha256':digest(conv),'candidate_kind':kind,'candidate_source_root':root,'candidate_cpp_sha256':digest(actualsrc) if actualsrc else None,'candidate_function_sha256':digest(new),'rate_update_suffix_source_equal':True,'conversion_source_equal':True,'sanitizer':a.sanitizer,'mock_scope':'Node and softc backing; diagnostic sink and removed pretty/log sinks. Actual update, actual conversion, actual notification lookup, original constants/rate table compiled. Atomic mock status sink race-tested; production status sink needs its own test.'}
print(json.dumps(proof,sort_keys=True),flush=True)
with tempfile.TemporaryDirectory(prefix='zc27-rate-') as td:
 p=Path(td);(p/'model.cpp').write_text(model)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-std=c++17','-O1','-g','-fsanitize='+a.sanitizer,str(p/'model.cpp'),'-o',str(p/'model')]
 print('COMMAND '+repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'model')],check=True,timeout=120)
print('PASS '+a.sanitizer,flush=True)
