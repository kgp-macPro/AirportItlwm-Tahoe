// Read-only host CLI using the same native-contract classifier as the Kext.
// GPL-2.0. No daemon modification, root access, HCI or running process access.
#include <CommonCrypto/CommonDigest.h>
#include <vector>
#include <fstream>
#include <cstdio>
#include "KGPBLEQualify.hpp"
static bool hash(const uint8_t *b,size_t n,uint8_t *out){return n<=48*1024*1024 && CC_SHA256(b,CC_LONG(n),out)!=nullptr;}
int main(int argc,char **argv){
 if(argc!=2){std::fputs("usage: BLE_QUALIFY /path/to/bluetoothd\n",stderr);return 2;}
 std::ifstream f(argv[1],std::ios::binary|std::ios::ate);auto size=f.tellg();
 if(size<32 || size>48*1024*1024){std::puts("{\"compatible\":false,\"reason\":\"unreadable or unbounded file\"}");return 2;}
 std::vector<uint8_t>b(static_cast<size_t>(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),size))return 2;
 KGPBLE::Plan p;uint32_t detail=0xffffffff;auto r=KGPBLE::qualify(b.data(),b.size(),hash,p,&detail);
 std::printf("{\"compatible\":%s,\"reason_code\":%u,\"reason\":\"%s\",\"missing_function\":\"%s\",\"pages\":%u,\"integrity_cdhash\":\"",
 r==KGPBLE::Good?"true":"false",r,KGPBLE::reasonName(r),detail<KGPBLE::contractCount?KGPBLE::contracts[detail].name:"",p.pageCount);
 for(unsigned i=0;i<20;++i)std::printf("%02x",p.cdhash[i]);
 std::puts("\",\"limit\":\"Function compatibility, not native Ready, HCI success, advertising or AirDrop proof\"}");return r==KGPBLE::Good?0:2;
}
