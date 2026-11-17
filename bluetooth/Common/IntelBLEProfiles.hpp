/* SPDX-License-Identifier: MIT
 * Intel AX/BE eligibility and read-only HCI evidence wire contract.
 * No Apple object layout, macOS build number or process patch is in this core.
 */
#ifndef KGP_INTEL_BLE_PROFILES_HPP
#define KGP_INTEL_BLE_PROFILES_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace IntelBLE {
static constexpr uint32_t Magic=0x31454c42;
static constexpr const char *Property="KGPIntelBLECapabilitiesV1";
// Wire property name stays stable; schema2 proves only mode-neutral reads.
// Advertising-set count/data length are unknown, not fabricated minima.
static constexpr uint32_t Schema=2,Publisher=0x02050302;
static constexpr uint8_t CoreQueryMask=7;
struct Evidence {
    uint32_t magic,bytes,version,publisher;
    uint64_t registryId;
    uint16_t vendor,product,usbRevision,manufacturer;
    uint8_t hciVersion,lmpVersion;
    uint16_t hciRevision,lmpSubversion;
    uint8_t commands[64],leFeatures[8];
    uint8_t advertisingSets,queryMask;
    uint16_t advertisingMaxLength;
} __attribute__((packed));
static_assert(sizeof(Evidence)==114,"BLE evidence wire extent");
enum class Family { Unknown,AX201,AX200,AX210,AX211,BE200,IntelGen3_0035,IntelGen3_0038,IntelCompatible };
struct Profile {uint16_t product;Family family;const char *name;bool legacyReviewed;};
// These are descriptive labels from pinned loader personalities, NOT an
// eligibility allowlist. Other Intel Bluetooth products use real HCI evidence.
static const Profile profiles[]={
    {0x0026,Family::AX201,"AX201/0026",false},
    {0x0029,Family::AX200,"AX200/0029",false},
    {0x0032,Family::AX210,"AX210/0032",false},
    {0x0033,Family::AX211,"AX211/0033",false},
    {0x0036,Family::BE200,"BE200/0036",true},
    {0x0035,Family::IntelGen3_0035,"Intel-Gen3/0035",false},
    {0x0038,Family::IntelGen3_0038,"Intel-Gen3/0038",false},
};
inline const Profile *find(uint16_t vendor,uint16_t product){
    if(vendor!=0x8087 || !product)return nullptr;
    for(const auto &p:profiles)if(p.product==product)return &p;
    static const Profile generic{0,Family::IntelCompatible,"Intel capability-selected",false};
    return &generic;
}
inline bool namedProduct(uint16_t vendor,uint16_t product){
    const auto *p=find(vendor,product);return p && p->product==product;
}
enum class Result { Standard,Waiting,LegacyReviewed,Verified,Malformed,IdentityMismatch,MissingQueries,UnsupportedHci };
inline Result evaluate(uint16_t vendor,uint16_t product,uint16_t revision,uint64_t registry,bool firmwareLoaded,const void *blob,size_t bytes){
    const auto *p=find(vendor,product);if(!p)return Result::Standard;
    if(!firmwareLoaded)return Result::Waiting;
    if(p->legacyReviewed)return Result::LegacyReviewed; // unchanged BLE-R25-01 eligibility
    if(!blob)return Result::Waiting;
    if(bytes!=sizeof(Evidence))return Result::Malformed;Evidence e{};memcpy(&e,blob,sizeof(e));
    if(e.magic!=Magic || e.version!=Schema || e.bytes!=sizeof(e) || e.publisher!=Publisher)return Result::Malformed;
    if(!registry || e.registryId!=registry || e.vendor!=vendor || e.product!=product || e.usbRevision!=revision)return Result::IdentityMismatch;
    if(e.queryMask!=CoreQueryMask)return Result::MissingQueries;
    if(e.advertisingSets || e.advertisingMaxLength)return Result::Malformed;
    // The eight existing edits force extended paths; never select them if
    // the real controller does not report those standard HCI operations.
    if(e.manufacturer!=2 || e.hciVersion<9 || e.lmpVersion<9 || !(e.commands[25]&4) ||
       !(e.leFeatures[1]&0x10) || (e.commands[36]&0xfe)!=0xfe || (e.commands[37]&0xe0)!=0xe0)return Result::UnsupportedHci;
    return Result::Verified;
}
inline bool enabled(Result r){return r==Result::LegacyReviewed || r==Result::Verified;}
inline bool needsEvidence(uint16_t vendor,uint16_t product){const auto *p=find(vendor,product);return p && !p->legacyReviewed;}
// Parse an actual Command Complete, including opcode, status and exact extent.
// No fabricated response, Ready transition, event mask, reset or advertising.
inline bool commandComplete(uint16_t opcode,const uint8_t *p,size_t n,uint8_t *out,size_t expected){
    // Zero command credits do not invalidate this completed reply. The owned
    // exchange waits for real replenishment before another request is allowed.
    if(!p || n!=6+expected || p[0]!=0x0e || size_t(p[1])+2!=n ||
       uint16_t(p[3]|(p[4]<<8))!=opcode || p[5]!=0)return false;
    if(expected)memcpy(out,p+6,expected);return true;
}
} // namespace IntelBLE
#endif
