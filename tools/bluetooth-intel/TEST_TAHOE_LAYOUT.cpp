// Production classifier + real Apple-file inputs, read only. GPL-2.0.
// No native execution, HCI requests or controller/advertising qualification.
#include <CommonCrypto/CommonDigest.h>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "KGPBLEQualify.hpp"
using namespace KGPBLE;
static bool hash(const uint8_t *b,size_t n,uint8_t *out){return CC_SHA256(b,CC_LONG(n),out)!=nullptr;}
static void require(bool yes,const char *name){if(!yes){std::fprintf(stderr,"FAIL: %s\n",name);std::abort();}}
static Reason classify(const std::vector<uint8_t>&b,Plan &p){
 std::vector<uint8_t>scratch(contractWorkspaceBytes);
 return qualify(b.data(),b.size(),hash,p,nullptr,scratch.data(),scratch.size());
}
static void reject(const std::vector<uint8_t>&b,const char *name,Reason expected=Good){
 Plan p;std::memset(&p,0x55,sizeof(p));auto result=classify(b,p);Plan zero={};
 require(result!=Good && equal(reinterpret_cast<uint8_t*>(&p),reinterpret_cast<uint8_t*>(&zero),sizeof(p)),name);
 if(expected!=Good)require(result==expected,name);
}
static void put32(uint8_t *p,uint32_t n){for(unsigned j=0;j<4;++j)p[j]=uint8_t(n>>(8*j));}
int main(int argc,char **argv){
 require(argc==2,"one native-file input required");std::ifstream f(argv[1],std::ios::binary);
 std::vector<uint8_t>b{std::istreambuf_iterator<char>(f),{}};Plan p;
 require(classify(b,p)==Good,"real native layout selected");Image v;require(parseImage(b.data(),b.size(),v),"bounded image");
 auto patched=b;uint32_t all=0;
 for(unsigned k=0;k<p.pageCount;++k){auto off=p.pages[k].offset;
  std::vector<uint8_t>page(b.begin()+off,b.begin()+off+4096),original=page;
  require(apply(p,page.data(),4096,off,false)==0 && page==original,"no changes without native approval");
  require(apply(p,page.data(),4095,off,true)==0 && page==original,"wrong-sized page rejected");
  for(unsigned j=0;j<8;++j)if((p.edits[j].fileOffset&~uint64_t(4095))==off){
   auto broken=page;broken[p.edits[j].fileOffset-off]^=1;auto saved=broken;
   require(apply(p,broken.data(),4096,off,true)==0 && broken==saved,"page edit is atomic on changed precondition");
  }
  uint32_t mask=apply(p,page.data(),4096,off,true);require(mask!=0,"selected page edited");all|=mask;
  auto saved=page;require(apply(p,page.data(),4096,off,true)==0 && page==saved,"already edited page not reapplied");
  std::memcpy(patched.data()+off,page.data(),4096);
 }
 require(all==allSites,"all eight original edits reachable");
 for(size_t i=0;i<b.size();++i){uint8_t expected=b[i];
  for(auto &e:p.edits)if(i>=e.fileOffset && i-e.fileOffset<e.length)expected=e.replace[i-e.fileOffset];
  require(patched[i]==expected,"no bytes outside selected instructions/data changed");
 }
 for(unsigned j=0;j<8;++j){auto x=b;x[p.edits[j].fileOffset]^=1;reject(x,"modified edit rejected");}
 // An external rel32 is excluded from layout hashes, but the two native bool
 // setters are NOT interchangeable: resolve their call/global relationship.
 auto clear=p.edits[3].fileOffset,conn=p.edits[6].fileOffset+7;
 auto addrSetter=int64_t(clear+5)+int32_t(u32(b.data()+clear+1));
 auto connSetter=int64_t(conn+5)+int32_t(u32(b.data()+conn+1));
 require(addrSetter!=connSetter,"distinct original setter roles");
 auto x=b;put32(x.data()+clear+1,uint32_t(connSetter-int64_t(clear+5)));
 reject(x,"address call redirected to same-layout connection setter",WrongOperandRelationship);
 x=b;put32(x.data()+conn+1,uint32_t(addrSetter-int64_t(conn+5)));
 reject(x,"connection call redirected to address setter",WrongOperandRelationship);
 // Change the shared address flag operand, not its opcode. A normalized
 // signature alone would permit this; the explicit operand graph must not.
 x=b;auto addrCompare=p.edits[4].fileOffset+6;x[addrCompare+2]^=0x10;
 reject(x,"address flag no longer aliases actual setter",WrongOperandRelationship);
 x=b;auto target=rel(b.data()+v.base,p.edits[4].fileOffset-v.base+6,2,7);
 x[v.base+target]=0;reject(x,"address flag initial state invalid",WrongOperandRelationship);
 x=b;put32(x.data()+connSetter+7,uint32_t(int64_t(v.base+target)-connSetter-11));
 reject(x,"distinct setter functions must not alias the same bool field",WrongOperandRelationship);
 // Even otherwise compatible normalized instructions need their original
 // signed page, not just a matching opcode fingerprint.
 x=b;x[p.edits[0].fileOffset+40]^=1;reject(x,"changed signed edit page");
 x=b;x[v.base+v.signature+16]^=0x40;reject(x,"wrong CodeDirectory");
 x.assign(b.begin(),b.begin()+20);reject(x,"truncated image",MalformedImage);
 Plan empty;std::memset(&empty,0x55,sizeof(empty));Plan zero={};
 require(qualify(b.data(),b.size(),hash,empty)==HashFailure &&
  equal(reinterpret_cast<uint8_t*>(&empty),reinterpret_cast<uint8_t*>(&zero),sizeof(empty)),"no workspace cannot publish plan");
 uint8_t digest[32];hash(patched.data(),patched.size(),digest);
 std::printf("{\"status\":\"PASS\",\"pages\":%u,\"cpu_patched_sha256\":\"",p.pageCount);
 for(auto q:digest)std::printf("%02x",q);
 std::puts("\",\"scope\":\"Production layout selection, setter alias negatives, integrity and page atomicity; no kernel/HCI/RF execution\"}");
}
