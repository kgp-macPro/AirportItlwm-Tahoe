// KGP/BlueToolFixup experimental native-contract qualification, GPL-2.0.
// No OS build/UUID allowlist. Function-layout hashes exclude only annotated
// PC-relative address operands; edit pages still require their ORIGINAL hash.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "KGPBLEPatterns.hpp"
#include "KGPBLEContracts.hpp"
namespace KGPBLE {
using Hasher=bool(*)(const uint8_t *,size_t,uint8_t *);
inline bool equal(const uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;++i)if(a[i]!=b[i])return false;return true;}
inline uint32_t u32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint64_t u64(const uint8_t *p){return u32(p)|(uint64_t(u32(p+4))<<32);}
inline uint32_t be32(const uint8_t *p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
inline bool fits(uint64_t off,uint64_t bytes,uint64_t size){return off<=size && bytes<=size-off;}
struct Edit {uint64_t fileOffset;uint8_t length,find[20],replace[20];};
struct Page {uint64_t offset;uint8_t sha256[32];};
struct Plan {uint64_t sliceOffset,sliceSize;uint8_t cdhash[20];Edit edits[8];Page pages[8];uint32_t pageCount;};
struct Image {const uint8_t *b;size_t size;uint64_t base,slice,vm,text,ts,constant,cs,functions,fs,signature,ss;};
enum Reason:uint32_t {Good=0,MalformedImage,UnsupportedCodeDirectory,MissingFunctionContract,AmbiguousFunctionContract,
 MissingEdit,AmbiguousEdit,WrongOperandRelationship,PageBoundary,HashFailure,UnexpectedFunctionLayout};
inline const char *reasonName(Reason r){switch(r){
 case Good:return "matching native instruction layout";case MalformedImage:return "malformed/unsupported Mach-O layout";
 case UnsupportedCodeDirectory:return "unsupported/incomplete CodeDirectory";case MissingFunctionContract:return "unrecognised function contract (not a build exclusion)";
 case AmbiguousFunctionContract:return "ambiguous function contract";case MissingEdit:return "missing required edit";
 case AmbiguousEdit:return "ambiguous required edit";case WrongOperandRelationship:return "inconsistent native field/call relationship";
 case PageBoundary:return "edit crosses a validation page";case HashFailure:return "integrity calculation failed";
 default:return "function layout incomplete";}}
inline bool parseImage(const uint8_t *b,size_t n,Image &v){
 v={};if(!b || n<32 || n>48*1024*1024)return false;
 uint64_t base=0,slice=n;
 if(be32(b)==0xcafebabe){
  uint32_t count=be32(b+4),found=0;if(!count || count>16 || !fits(8,uint64_t(count)*20,n))return false;
  for(uint32_t i=0;i<count;++i){const auto *a=b+8+i*20;if(be32(a)==0x1000007){
   if(++found!=1 || be32(a+4)!=3)return false;base=be32(a+8);slice=be32(a+12);}}
  if(found!=1 || !fits(base,slice,n))return false;
 }
 if(slice<32 || u32(b+base)!=0xfeedfacf || u32(b+base+4)!=0x1000007 || u32(b+base+12)!=2)return false;
 const auto *s=b+base;uint32_t count=u32(s+16),lcbytes=u32(s+20);if(count>2048 || !fits(32,lcbytes,slice))return false;
 uint64_t p=32;unsigned textSections=0,constants=0,starts=0,signatures=0;
 for(uint32_t i=0;i<count;++i){if(!fits(p,8,32+lcbytes))return false;uint32_t c=u32(s+p),sz=u32(s+p+4);
  if(sz<8 || !fits(p,sz,32+lcbytes))return false;
  if(c==0x19){if(sz<72)return false;uint32_t sections=u32(s+p+64);if(!fits(72,uint64_t(sections)*80,sz))return false;
   if(equal(s+p+8,reinterpret_cast<const uint8_t *>("__TEXT\0"),7)){v.vm=u64(s+p+24);if(u64(s+p+40)!=0)return false;}
   for(uint32_t j=0;j<sections;++j){const auto *q=s+p+72+j*80;uint64_t off=u32(q+48),bytes=u64(q+40);
    if(equal(q,reinterpret_cast<const uint8_t *>("__text\0"),7) && equal(q+16,reinterpret_cast<const uint8_t *>("__TEXT\0"),7)){
     if(++textSections!=1 || !fits(off,bytes,slice))return false;v.text=off;v.ts=bytes;}
    if(equal(q,reinterpret_cast<const uint8_t *>("__const\0"),8) && equal(q+16,reinterpret_cast<const uint8_t *>("__TEXT\0"),7)){
     if(++constants!=1 || !fits(off,bytes,slice))return false;v.constant=off;v.cs=bytes;}
   }
  }else if(c==0x26 || c==0x1d){if(sz!=16)return false;uint64_t off=u32(s+p+8),bytes=u32(s+p+12);
   if(!fits(off,bytes,slice) || !bytes)return false;
   if(c==0x26){if(++starts!=1)return false;v.functions=off;v.fs=bytes;}
   else{if(++signatures!=1)return false;v.signature=off;v.ss=bytes;}}
  p+=sz;
 }
 if(p!=32+lcbytes || textSections!=1 || constants!=1 || starts!=1 || signatures!=1 || !v.vm)return false;
 v.b=b;v.size=n;v.base=base;v.slice=slice;return true;
}
inline Reason codeDirectory(const Image &v,Hasher hash,Plan &p){
 const auto *s=v.b+v.base+v.signature;
 if(v.ss<12 || be32(s)!=0xfade0cc0 || be32(s+4)>v.ss || be32(s+4)<12)return UnsupportedCodeDirectory;
 uint32_t n=be32(s+8),length=be32(s+4),found=0;if(n>32 || !fits(12,uint64_t(n)*8,length))return UnsupportedCodeDirectory;
 for(uint32_t i=0;i<n;++i){if(be32(s+12+i*8)!=0)continue;
  uint64_t off=be32(s+16+i*8);if(++found!=1 || !fits(off,44,length))return UnsupportedCodeDirectory;
  const auto *cd=s+off;uint32_t bytes=be32(cd+4),slots=be32(cd+28),limit=be32(cd+32),hashOff=be32(cd+16);
  if(be32(cd)!=0xfade0c02 || !fits(off,bytes,length) || bytes<44 || cd[36]!=32 || cd[37]!=2 || cd[39]!=12 ||
    !limit || limit>v.signature || slots!=(uint64_t(limit)+4095)/4096 || !fits(hashOff,uint64_t(slots)*32,bytes))return UnsupportedCodeDirectory;
  // Bind all additional edit pages to this CodeDirectory. Unrelated pages
  // can carry existing system/root/base fixes; they are not our authority.
  for(uint32_t j=0;j<p.pageCount;++j){uint64_t a=p.pages[j].offset-v.base;
   if(p.pages[j].offset<v.base || a%4096 || !fits(a,4096,limit))return UnsupportedCodeDirectory;
   uint32_t slot=uint32_t(a/4096);uint8_t d[32];
   if(slot>=slots || !hash(v.b+v.base+a,4096,d))return HashFailure;
   if(!equal(d,cd+hashOff+size_t(slot)*32,32))return UnsupportedCodeDirectory;}
  uint8_t d[32];if(!hash(cd,bytes,d))return HashFailure;
  for(unsigned j=0;j<20;++j)p.cdhash[j]=d[j];
 }
 return found==1?Good:UnsupportedCodeDirectory;
}
inline bool nextFunction(const Image &v,uint64_t &pos,uint64_t &start,uint64_t &next){
 uint64_t value=0;unsigned shift=0;
 while(pos<v.fs){uint8_t a=v.b[v.base+v.functions+pos++];if(shift>=63)return false;value|=uint64_t(a&127)<<shift;
  if(!(a&128)){if(!value || value>v.slice-start)return false;next=start+value;return true;}shift+=7;}
 return false;
}
inline int64_t rel(const uint8_t *b,uint64_t off,unsigned disp,unsigned length){return int64_t(off+length)+int32_t(u32(b+off+disp));}
inline bool relativeByte(const FunctionContract &c,size_t off){
 for(unsigned i=0;i<c.relativeCount;++i)if(off>=c.relative[i] && off-c.relative[i]<4)return true;
 return false;
}
inline bool prefixMatches(const uint8_t *b,const FunctionContract &c){
 for(unsigned i=0;i<16 && i<c.size;++i)if(!relativeByte(c,i) && b[i]!=c.prefix[i])return false;
 return true;
}
// Caller-owned temporary workloop/host storage: no 8-KiB kernel stack frame,
// allocation or disassembly in the native page-validation hook.
inline bool layoutHash(const uint8_t *b,const FunctionContract &c,uint8_t *scratch,size_t bytes,Hasher hash,uint8_t *digest){
 if(!scratch || c.size>bytes)return false;
 for(unsigned i=0;i<c.size;++i)scratch[i]=b[i];
 for(unsigned i=0;i<c.relativeCount;++i){size_t off=c.relative[i];
  if(!fits(off,4,c.size) || (i && off<size_t(c.relative[i-1])+4))return false;
  for(unsigned j=0;j<4;++j)scratch[off+j]=0;
 }
 return hash(scratch,c.size,digest);
}
inline bool functionAt(const Image &v,uint64_t address,uint32_t bytes){
 uint64_t pos=0,start=0,next=0;if(!nextFunction(v,pos,start,next))return false;start=next;
 while(nextFunction(v,pos,start,next)){
  if(start==address)return start>=v.text && next<=v.text+v.ts && next-start==bytes;
  if(start>address)return false;start=next;
 }
 return false;
}
inline Reason qualify(const uint8_t *b,size_t n,Hasher hash,Plan &out,uint32_t *detail=nullptr,uint8_t *scratch=nullptr,size_t scratchBytes=0){
 if(detail)*detail=0xffffffff;
 out={};Image v;if(!hash || !parseImage(b,n,v))return MalformedImage;
 if(!scratch || scratchBytes<contractWorkspaceBytes)return HashFailure;
 Plan p={};p.sliceOffset=v.base;p.sliceSize=v.slice;
 // All function starts come from Apple's bounded LC_FUNCTION_STARTS table.
 uint64_t found[contractCount]={};uint32_t counts[contractCount]={};uint64_t cursor=0,start=0,next=0;
 bool have=nextFunction(v,cursor,start,next);if(!have)return UnexpectedFunctionLayout;start=next;
 while(nextFunction(v,cursor,start,next)){
  if(start>=v.text && next<=v.text+v.ts && next>start){
   for(unsigned k=0;k<contractCount;++k){
    // Many native bool setters share the same instruction layout. Resolve
    // these two by the actual calls below, never by a global first match.
    if(k==F_AddressSetter || k==F_ConnectionSetter)continue;
    const auto &c=contracts[k];if(next-start==c.size && c.size>=8 && prefixMatches(b+v.base+start,c)){
    uint8_t digest[32];if(!layoutHash(b+v.base+start,c,scratch,scratchBytes,hash,digest))return HashFailure;
    if(equal(digest,c.sha256,32)){found[k]=start;++counts[k];}
   }}
  }
  start=next;
 }
 for(unsigned k=0;k<contractCount;++k){if(k!=F_AddressSetter && k!=F_ConnectionSetter && counts[k]!=1){if(detail)*detail=k;return counts[k]?AmbiguousFunctionContract:MissingFunctionContract;}}
 struct Spec {unsigned function;uint32_t offset;uint8_t size;const uint8_t *find,*replace;uint8_t editOffset,editSize;};
 const Spec specs[]={
  {F_SpecQuery,0x6b,sizeof(SpecAdvSetsCheck_find),SpecAdvSetsCheck_find,SpecAdvSetsCheck_replace,3,6},
  {F_CoreInit,0x5db,sizeof(ExtendedAdvertisingCheck_find),ExtendedAdvertisingCheck_find,ExtendedAdvertisingCheck_replace,3,6},
  {F_ExtendedSetsQuery,0x6b,sizeof(ExtendedAdvertisingSetsCheck_find),ExtendedAdvertisingSetsCheck_find,ExtendedAdvertisingSetsCheck_replace,3,6},
  {F_CoreInit,0xdd8,sizeof(AdvertisingSetAddressGate_find),AdvertisingSetAddressGate_find,AdvertisingSetAddressGate_replace,11,5},
  {F_AddressSync,0xf0,sizeof(AdvertisingSetAddressSync_find),AdvertisingSetAddressSync_find,AdvertisingSetAddressSync_replace,3,6},
  {F_CoreInit,0xa22,sizeof(ExtendedScanning_find),ExtendedScanning_find,ExtendedScanning_replace,0,10},
  {F_CoreInit,0xa06,sizeof(ExtendedCreateConnection_find),ExtendedCreateConnection_find,ExtendedCreateConnection_replace,0,7}};
 for(unsigned i=0;i<7;++i){const auto &s=specs[i];uint64_t off=found[s.function]+s.offset;
  if(s.size>20 || s.offset+s.size>contracts[s.function].size || s.editOffset+s.editSize>s.size)return MissingEdit;
  for(unsigned j=0;j<s.size;++j)if(!relativeByte(contracts[s.function],s.offset+j) && b[v.base+off+j]!=s.find[j])return MissingEdit;
  // Replace only the intended instructions. Never copy a reference build's
  // call/RIP displacement into the newly selected, originally signed image.
  auto &e=p.edits[i];e.fileOffset=v.base+off+s.editOffset;e.length=s.editSize;
  for(unsigned j=0;j<e.length;++j){e.find[j]=b[e.fileOffset+j];e.replace[j]=s.replace[s.editOffset+j];}
 }
 unsigned matches=0;uint64_t maskOff=0;
 for(uint64_t i=v.constant;i+sizeof(LeEventMask_find)<=v.constant+v.cs;++i)if(equal(b+v.base+i,LeEventMask_find,sizeof(LeEventMask_find))){maskOff=i;++matches;}
 if(matches!=1)return matches?AmbiguousEdit:MissingEdit;
 auto &mask=p.edits[7];mask.fileOffset=v.base+maskOff;mask.length=sizeof(LeEventMask_find);
 for(unsigned j=0;j<mask.length;++j){mask.find[j]=LeEventMask_find[j];mask.replace[j]=LeEventMask_replace[j];}
 const auto *s=b+v.base;uint64_t clear=found[F_CoreInit]+0xdd8,addr=found[F_AddressSync]+0xf0,conn=found[F_CoreInit]+0xa06;
 const unsigned setters[]={F_AddressSetter,F_ConnectionSetter};
 const int64_t targets[]={rel(s,clear+11,1,5),rel(s,conn+7,1,5)};
 for(unsigned i=0;i<2;++i){auto k=setters[i];const auto &c=contracts[k];uint8_t digest[32];
  if(targets[i]<0 || !functionAt(v,uint64_t(targets[i]),c.size) ||
     !layoutHash(s+targets[i],c,scratch,scratchBytes,hash,digest) || !equal(digest,c.sha256,32))return WrongOperandRelationship;
  found[k]=uint64_t(targets[i]);
 }
 if(found[F_AddressSetter]==found[F_ConnectionSetter] ||
    rel(s,found[F_AddressSetter]+4,3,7)==rel(s,found[F_ConnectionSetter]+4,3,7))return WrongOperandRelationship;
 // Inspected instructions plus resolved call/global relationships, not a
 // claim that all native initialization or firmware behavior is verified.
 if(rel(s,clear+11,1,5)!=int64_t(found[F_AddressSetter]) || rel(s,conn+7,1,5)!=int64_t(found[F_ConnectionSetter]) ||
    s[found[F_AddressSetter]+4]!=0x40 || s[found[F_AddressSetter]+5]!=0x88 || s[found[F_AddressSetter]+6]!=0x3d ||
    rel(s,found[F_AddressSetter]+4,3,7)!=rel(s,addr+9,2,7) ||
    !equal(s+found[F_EventMaskConsumer]+0x1f,reinterpret_cast<const uint8_t *>("\x48\x8d"),2) ||
    rel(s,found[F_EventMaskConsumer]+0x1f,3,7)!=int64_t(maskOff))return WrongOperandRelationship;
 // Native globals must be real data, in the signed slice; one shared address
 // flag, not two similarly named source-level values. No direct global edits.
 int64_t field=rel(s,addr+9,2,7);if(field<0 || uint64_t(field)>=v.signature || s[field]!=1)return WrongOperandRelationship;
 for(unsigned i=0;i<8;++i){const auto &e=p.edits[i];uint64_t page=e.fileOffset&~uint64_t(4095);
  if((e.fileOffset+e.length-1)/4096!=page/4096 || !fits(page,4096,n))return PageBoundary;
  bool exists=false;for(unsigned k=0;k<p.pageCount;++k)exists|=p.pages[k].offset==page;
  if(!exists){auto &q=p.pages[p.pageCount++];q.offset=page;if(!hash(b+page,4096,q.sha256))return HashFailure;}
 }
 auto code=codeDirectory(v,hash,p);if(code!=Good)return code;
 out=p;return Good; // Only publication point: all required contracts passed.
}
inline const Page *pageInfo(const Plan &p,uint64_t off){for(unsigned i=0;i<p.pageCount;++i)if(p.pages[i].offset==off)return &p.pages[i];return nullptr;}
inline uint32_t apply(const Plan &p,uint8_t *b,size_t bytes,uint64_t off,bool approved){
 if(!approved || !b || bytes!=4096 || !pageInfo(p,off))return 0;uint32_t mask=0;
 for(unsigned i=0;i<8;++i){const auto &e=p.edits[i];if((e.fileOffset&~uint64_t(4095))==off){
  if(!equal(b+(e.fileOffset-off),e.find,e.length))return 0;mask|=1u<<i;}}
 for(unsigned i=0;i<8;++i){const auto &e=p.edits[i];if(mask&(1u<<i))for(unsigned j=0;j<e.length;++j)b[e.fileOffset-off+j]=e.replace[j];}
 return mask;
}
}
