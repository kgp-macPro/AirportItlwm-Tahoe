#!/usr/bin/env python3
"""Real legacy getSCAN_RESULT and actual per-target result structure under sanitizers.
Mocks only node-tree/KPI surroundings; checks exported rates, whole-IE bounds,
SSID/BSSID/channel/age, iterator and failures. Not a Location Services model.
"""
from pathlib import Path
import re,subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2];G=subprocess.check_output(['/usr/bin/xcrun','--find','git'],text=True).strip();C=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip();SDK=subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
P='AirportItlwm/AirportSTAIOCTL.cpp';src=(R/P).read_text();oldsrc=subprocess.check_output([G,'show','028b4626d113efa283ed44f9c4feb92ba6735aab:'+P],cwd=R,text=True)
def mask(s):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
def fn(s):
 m=mask(s);x=re.search(r'IOReturn\s+AirportItlwm::\s*getSCAN_RESULT\([^;{}]*\)\s*\{',m);assert x;a=m.index('{',x.start());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
def body(s,n):
 m=mask(s);x=re.search(r'\bstruct\s+'+n+r'\s*\{',m);assert x;a=m.index('{',x.start());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():s.index(';',b)+1]
wire=(R/'include/Airport/apple80211_var.h').read_text();constants=''
for n in ['APPLE80211_MAX_RATES','APPLE80211_ADDR_LEN','APPLE80211_MAX_SSID_LEN']:
 x=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',wire,re.M);assert x,n;constants+='#define '+n+' '+x[1]+'\n'
layout=body(wire,'apple80211_channel')+'\n'+body(wire,'apple80211_scan_result')
new=fn(src);old=fn(oldsrc).replace('getSCAN_RESULT(', 'old_getSCAN_RESULT(',1)
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <strings.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include "include/Airport/ScanResultBounds.hpp"
#define __MAC_12_0 120000
#define __MAC_13_0 130000
#define APPLE80211_VERSION 1
#define IEEE80211_ADDR_LEN 6
#define IEEE80211_S_INIT 0
#define IWM_MIN_DBM -100
#define MIN(a,b) ((a)<(b)?(a):(b))
using IOReturn=uint32_t;constexpr auto kIOReturnSuccess=0U,kIOReturnBadArgument=0xe00002c2U,kIOReturnNoMemory=0xe00002bdU;
struct OSObject{};
'''
mock=r'''
struct Channel{uint32_t ic_flags=0x100;};
struct ieee80211_node {ieee80211_node*next=nullptr;void*verb=nullptr;uint8_t*ni_rsnie_tlv=nullptr;unsigned ni_rsnie_tlv_len=0;unsigned ni_intval=100,ni_age_ts=90000,ni_capinfo=0x411,ni_rssi=45,ni_esslen=3;Channel channel;Channel*ni_chan=&channel;
 uint8_t ni_bssid[6]={2,3,4,5,6,7},ni_essid[32]={'b','b','c'};struct{uint8_t rs_nrates=12;uint8_t rs_rates[32]={2,4,11,22,12,18,24,36,48,72,96,108};}ni_rates;};
struct ieee80211com{ieee80211_node*ic_tree=nullptr;};
#define RB_MIN(type,p) (*(p))
#define RB_NEXT(type,p,n) ((n)->next)
struct Hal {ieee80211com ic;ieee80211com*get80211Controller(){return &ic;}Hal*getDriverInfo(){return this;}int getBSSNoise(){return 95;}};
static unsigned airport_up_time(){return 100000;}
static unsigned ieee80211_chan2ieee(ieee80211com*,Channel*){return 100;}
static unsigned ieeeChanFlag2appleScanFlagVentura(unsigned){return 0x1234;}
static unsigned ieeeChanFlag2apple(unsigned,int){return 0x5678;}
struct AirportItlwm{Hal hal;Hal*fHalService=&hal;ieee80211_node*fNextNodeToSend=nullptr;bool fScanResultWrapping=false;
 IOReturn getSCAN_RESULT(OSObject*,apple80211_scan_result**);IOReturn old_getSCAN_RESULT(OSObject*,apple80211_scan_result**);};
struct Guard{uint64_t pre=0x1122334455667788ULL;apple80211_scan_result result{};uint64_t post=0x8877665544332211ULL;};
'''
test=r'''
int main(){unsigned cases=0;
 {AirportItlwm x;ieee80211_node n;Guard g;n.verb=&g.result;x.hal.ic.ic_tree=&n;apple80211_scan_result*r=nullptr;
  assert(!x.old_getSCAN_RESULT(nullptr,&r)&&r->asr_nrates==12);for(auto rate:r->asr_rates)assert(rate==0); // Reproduce old bug.
  x.fNextNodeToSend=nullptr;x.fScanResultWrapping=false;assert(!x.getSCAN_RESULT(nullptr,&r)&&r==&g.result&&r->asr_nrates==12);
  for(unsigned i=0;i<12;++i)assert(r->asr_rates[i]==n.ni_rates.rs_rates[i]);for(unsigned i=12;i<15;++i)assert(!r->asr_rates[i]);++cases;}
 for(unsigned nr:{0U,1U,12U,15U,16U,32U,255U})for(unsigned ie:{0U,1U,2U,255U,257U,1024U,1285U,2000U}){
  AirportItlwm x;ieee80211_node n;Guard g;n.verb=&g.result;x.hal.ic.ic_tree=&n;n.ni_rates.rs_nrates=nr;std::vector<uint8_t> data(ie);unsigned at=0;
  while(ie-at>=2){unsigned len=std::min(255U,ie-at-2);data[at]=48;data[at+1]=len;for(unsigned i=0;i<len;++i)data[at+2+i]=uint8_t(i);at+=len+2;}if(at<ie)data[at]=0xff;
  n.ni_rsnie_tlv=ie?data.data():nullptr;n.ni_rsnie_tlv_len=ie;apple80211_scan_result*r=nullptr;assert(!x.getSCAN_RESULT(nullptr,&r));
  assert(r==&g.result&&g.pre==0x1122334455667788ULL&&g.post==0x8877665544332211ULL);
  assert(r->asr_nrates==std::min(nr,15U));for(unsigned i=0;i<r->asr_nrates;++i)assert(r->asr_rates[i]==n.ni_rates.rs_rates[i]);
#if __IO80211_TARGET >= __MAC_12_0
  unsigned expected=ie==1?0:ie>1024?771:ie;assert(r->asr_ie_len==expected&&expected<=sizeof(r->asr_ie_data));assert(!memcmp(r->asr_ie_data,data.data(),expected));
#else
  assert(r->asr_ie_len==ie&&r->asr_ie_data==n.ni_rsnie_tlv);
#endif
  assert(r->asr_ssid_len==3&&!memcmp(r->asr_ssid,"bbc",3));assert(!memcmp(r->asr_bssid,n.ni_bssid,6));
  assert(r->asr_channel.channel==100&&r->asr_age==10000&&r->asr_rssi==-55&&r->asr_noise==-95&&r->asr_beacon_int==100);
  assert(x.fScanResultWrapping);assert(x.getSCAN_RESULT(nullptr,&r)==5&&!x.fScanResultWrapping);++cases;
 }
 {AirportItlwm x;ieee80211_node n;Guard g;n.verb=&g.result;n.ni_esslen=255;x.hal.ic.ic_tree=&n;apple80211_scan_result*r=nullptr;assert(!x.getSCAN_RESULT(nullptr,&r)&&r->asr_ssid_len==32&&g.post==0x8877665544332211ULL);++cases;}
 {AirportItlwm x;ieee80211_node n;Guard g;n.verb=&g.result;uint8_t partial[]={48,8,1,2};n.ni_rsnie_tlv=partial;n.ni_rsnie_tlv_len=4;x.hal.ic.ic_tree=&n;apple80211_scan_result*r=nullptr;assert(!x.getSCAN_RESULT(nullptr,&r));
#if __IO80211_TARGET >= __MAC_12_0
  assert(r->asr_ie_len==0);
#endif
 ++cases;}
 {AirportItlwm x;assert(x.getSCAN_RESULT(nullptr,nullptr)==kIOReturnBadArgument);apple80211_scan_result*r=nullptr;assert(x.getSCAN_RESULT(nullptr,&r)==12);++cases;}
 {AirportItlwm x;ieee80211_node n;x.hal.ic.ic_tree=&n;apple80211_scan_result*r=nullptr;assert(x.getSCAN_RESULT(nullptr,&r)==kIOReturnNoMemory);++cases;}
 {AirportItlwm x;ieee80211_node a,b;Guard ga,gb;a.verb=&ga.result;b.verb=&gb.result;a.next=&b;b.ni_bssid[5]=99;x.hal.ic.ic_tree=&a;apple80211_scan_result*r=nullptr;assert(!x.getSCAN_RESULT(nullptr,&r)&&r->asr_bssid[5]==7);assert(!x.getSCAN_RESULT(nullptr,&r)&&r->asr_bssid[5]==99);assert(x.getSCAN_RESULT(nullptr,&r)==5);++cases;}
 std::cout<<"PASS "<<cases<<" real legacy scan-export cases target="<<__IO80211_TARGET<<" result_bytes="<<sizeof(apple80211_scan_result)<<"; not location/iCloud/Find My proof.\n";
}
'''
san=sys.argv[1]if len(sys.argv)>1 else'address,undefined'
with tempfile.TemporaryDirectory(prefix='scan-export-')as td:
 p=Path(td);(p/'test.cpp').write_text(pre+constants+layout+mock+old+new+test)
 for target in [110000,120000,130000]:
  cmd=[C,'-isysroot',SDK,'-std=gnu++17','-O1','-g','-fsanitize='+san,'-D__IO80211_TARGET='+str(target),'-I'+str(R),str(p/'test.cpp'),'-o',str(p/'test')]
  print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
