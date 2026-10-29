#!/usr/bin/env python3
"""Pinned GL92 envelope/versions and actual serializers' wire ABI + SDK slot.
Read-only references; output is explicit evidence, never old audit mutation.
"""
from pathlib import Path
import re,json,hashlib,subprocess,tempfile,struct,zlib,sys
R=Path(__file__).resolve().parents[1];A=Path('/Users/kgp/Desktop/AirportItlwm/EVIDENCE/2026-10-29_BE200_SOURCE_BINARY_AUDIT')
O=Path(sys.argv[1]);O.mkdir(parents=True,exist_ok=True)
G='/Applications/Xcode.app/Contents/Developer/usr/bin/git';C='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
SDK='/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk'
reg=(R/'itlwm/hal_iwx/if_iwxreg.h').read_text();cpp=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text();extra=(R/'itlwm/hal_iwx/ItlIwxBzTransport.inc').read_text()
def sha(b):return hashlib.sha256(b).hexdigest()
def extract(s,n):
 m=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda x:' '*len(x[0]),s,flags=re.S)
 x=re.search(r'(?:void|int|uint8_t)\s+ItlIwx::\s*'+n+r'\(',m);assert x,n;a=m.index('{',x.end());b=a+1;d=1
 while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
 return s[x.start():b]
def structbody(s,n):
 x=re.search(r'\bstruct '+n+r'\s*\{',s);assert x,n;a=s.index('{',x.start());b=a+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[x.start():s.index(';',b)+1]
fw=R/'itlwm/firmware/be200-gl92';expected={'iwlwifi-gl-c0-fm-c0-92.ucode':'c7092f1f78676f9e9c2c4d12716401db2116b009c5d7c464e8387c5b1c645d6f','iwlwifi-gl-c0-fm-c0.pnvm':'f481e3b0403ca43a0a444b01c04fa4d1582868afcb026b1e697905f0039ccfa3'}
embedded=(R/'include/FwBinary.cpp').read_text();base=subprocess.check_output([G,'show','483940988c502379fb6d3dcde885dca83f00b7aa:include/FwBinary.cpp'],cwd=R,text=True)
assert embedded.startswith(base[:base.index('const struct FwDesc')]);assert 'fwNumber = 37' in embedded
for n,h in expected.items():
 raw=(fw/n).read_bytes();assert sha(raw)==h;v=n.replace('.','_').replace('-','_');x=re.search(r'const unsigned char '+v+r'\[\] = \{(.*?)\};',embedded,re.S);assert x
 packed=bytes(int(z,16) for z in re.findall(r'0x([0-9A-Fa-f]{2})',x[1]));assert zlib.decompress(packed)==raw
b=(fw/'iwlwifi-gl-c0-fm-c0-92.ucode').read_bytes();at=88;versions=[];tlvs=[]
while at<len(b):
 assert at+8<=len(b);t,n=struct.unpack_from('<II',b,at);at+=8;assert at+n<=len(b);v=b[at:at+n];tlvs.append({'type':t,'length':n})
 if t==48:assert not n%4;versions=[list(v[j:j+4]) for j in range(0,n,4)]
 at+=(n+3)&~3
assert at==len(b) and len(versions)==215
(O/'GL92_TLVS.json').write_text(json.dumps({'sha256':sha(b),'tlvs':tlvs,'versions':versions},indent=2)+'\n')
# Run the exact production version selector, not a separately inferred table.
fns='\n'.join(extract(cpp,n) for n in ['iwx_lookup_cmd_ver','iwx_lookup_notif_ver'])+'\n'+extract(extra,'bzValidateFirmware')
constants=''
for n in sorted(set(re.findall(r'\bIWX_[A-Z0-9_]+',fns))):
 x=re.search(r'^#define\s+'+n+r'\s+([^\n]+)',reg,re.M);assert x,n
 constants+='#define '+n+' '+x[1]+'\n'
pre='#include <cstdint>\n#include <cassert>\n#include <cerrno>\n#include <cstdio>\n#include "itlwm/hal_iwx/ItlIwxBzContracts.hpp"\n#define __packed __attribute__((packed))\n#define isset(a,n) ((a)[(n)/8]&(1U<<((n)%8)))\n'+constants+structbody(reg,'iwx_fw_cmd_version')+';\n'
pre+='struct iwx_softc {unsigned char sc_enabled_capa[16]={};iwx_fw_cmd_version cmd_versions[256];int n_cmd_versions=215;};\nstruct ItlIwx {iwx_softc com;unsigned errors=0;void bzFail(unsigned,int){++errors;}uint8_t iwx_lookup_cmd_ver(iwx_softc*,uint8_t,uint8_t);uint8_t iwx_lookup_notif_ver(iwx_softc*,uint8_t,uint8_t);int bzValidateFirmware();};\n'
arr=','.join('{'+','.join(map(str,v))+'}' for v in versions)
test='int main(){ItlIwx x;x.com.sc_enabled_capa[110/8]|=1U<<(110%8);iwx_fw_cmd_version v[]={'+arr+'};for(unsigned i=0;i<215;i++)x.com.cmd_versions[i]=v[i];assert(!x.bzValidateFirmware());\n'
test+='for(unsigned i=0;i<215;i++)if(x.com.cmd_versions[i].group==3&&x.com.cmd_versions[i].cmd==9)x.com.cmd_versions[i].cmd_ver=3;assert(x.bzValidateFirmware()!=0);for(unsigned i=0;i<215;i++)x.com.cmd_versions[i]=v[i];assert(!x.bzValidateFirmware());for(unsigned i=0;i<215;i++)if(x.com.cmd_versions[i].group==12&&x.com.cmd_versions[i].cmd==254)x.com.cmd_versions[i].notif_ver=2;assert(x.bzValidateFirmware()!=0);puts("PASS actual GL92 selector: implicit STA accepted; LINK and independent PNVM layout mutations rejected");}\n'
(O/'version_fixture.cpp').write_text(pre+structbody(cpp,'iwl_pnvm_init_complete_ntfy')+'\nstatic_assert(sizeof(iwl_pnvm_init_complete_ntfy)==4,"PNVM v1 wire size");\n'+fns+test)
cmd=[C,'-isysroot',SDK,'-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(R),str(O/'version_fixture.cpp'),'-o',str(O/'version_fixture')]
subprocess.run(cmd,check=True);subprocess.run([str(O/'version_fixture')],check=True)
# Original pinned Linux ABI fixture: replace EVERY iwx struct with the current
# production definition. Linux bodies/assertions remain unchanged (29 assertions).
fixture=(A/'analysis/host_wire_abi.c').read_text()
for n in set(re.findall(r'^struct (iwx_\w+)\s*\{',fixture,re.M)):
 fixture=fixture.replace(structbody(fixture,n),structbody(reg,n))
(O/'wire_abi.cpp').write_text(fixture)
subprocess.run([C,'-isysroot',SDK,'-std=c++17',str(O/'wire_abi.cpp'),'-o',str(O/'wire_abi')],check=True);subprocess.run([str(O/'wire_abi')],check=True)
# TX_RSP v7 is explicitly the same pinned Linux layout as v6, not assumed.
linux_tx=(A/'sources/linux-a38297e3fb01/fw/api/tx.h').read_text()
def txshape(s,n):
 text=structbody(s,n)
 text=re.sub(r'__le(16|32|64)',r'uint\1_t',text)
 text=re.sub(r'\bu(8|16|32|64)\b',r'uint\1_t',text)
 text=text.replace('iwl_mvm_tx_resp','iwx_tx_resp')
 text=re.sub(r'\bagg_tx_status\b','iwx_agg_tx_status',text)
 return re.sub(r'\s+','',text)
assert txshape(linux_tx,'iwl_mvm_tx_resp')==txshape(reg,'iwx_tx_resp')
(O/'TX_V7_LAYOUT.txt').write_text('PASS exact normalized field layout: Linux TX_RSP_API_S_VER_6/7 == production iwx_tx_resp. Full little-endian fields, queue/status/SSN offsets preserved.\n')
# SDK patch at its DECLARED reserved slot, derived compiler member-pointer ABI.
header='MacKernelSDK/Headers/IOKit/pci/IOPCIDevice.h';old=subprocess.check_output([G,'show','483940988c502379fb6d3dcde885dca83f00b7aa:'+header],cwd=R,text=True)
(O/'IOPCIDevice.base.h').write_text(old)
(O/'sdk_baseline.cpp').write_text('#define private public // access-only test fixture; never a production header\n#include "IOPCIDevice.base.h"\nusing M=void(IOPCIDevice::*)();M oldslot=&IOPCIDevice::_RESERVEDIOPCIDevice3;\n')
(O/'sdk_probe.cpp').write_text('#include <IOKit/pci/IOPCIDevice.h>\nusing M=IOReturn(IOPCIDevice::*)(UInt32,UInt32,UInt32,IOOptionBits);M configured=&IOPCIDevice::configureInterrupts;\nextern "C" IOReturn probe(IOPCIDevice*p){return p->configureInterrupts(0x20000U,1,1,0);}\n')
flags=[C,'-x','c++','-std=gnu++17','-mkernel','-DKERNEL','-DKERNEL_PRIVATE','-fapple-kext','-fno-exceptions','-fno-rtti','-nostdinc','-I'+str(R/'MacKernelSDK/Headers'),'-I'+str(R/'MacKernelSDK/Headers/i386'),'-I'+str(R/'MacKernelSDK/Headers/libkern/c++'),'-S','-emit-llvm']
for n in ['sdk_baseline','sdk_probe']:subprocess.run(flags+[str(O/(n+'.cpp')),'-o',str(O/(n+'.ll'))],check=True)
a=(O/'sdk_baseline.ll').read_text();c=(O/'sdk_probe.ll').read_text();slotA=int(re.search(r'@oldslot = global \{ i64, i64 \} \{ i64 (\d+), i64 0 \}',a)[1]);slotB=int(re.search(r'@configured = global \{ i64, i64 \} \{ i64 (\d+), i64 0 \}',c)[1]);assert slotA==slotB and slotA>1 and (slotA-1)%8==0;assert ("i64 "+str((slotA-1)//8)) in c
record={'status':'PASS','GL92_sha256':sha(b),'embedded_fw_pnvm_matches':True,'legacy35_embedded_arrays_byte_preserved':True,'actual_version_selector':'PASS','current_wire_abi_assertions':29,'sdk_base':'05094e5e88cec7caedbfb35e8449ed0db94bf95b','sdk_header_sha256':sha((R/header).read_bytes()),'sdk_reserved3_member_pointer':slotA,'sdk_configure_member_pointer':slotB,'compiler_derived_byte_offset':slotB-1,'caller_argument_types':'UInt32/UInt32/UInt32/IOOptionBits=UInt32','limits':'Compiler ABI and pinned source, not runtime IOPCIFamily or hardware proof.'}
(O/'RESULT.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
