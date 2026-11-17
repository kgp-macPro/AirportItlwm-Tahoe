/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP
 * Exact Ventura Legacy4C07538B availability ABI, qualified with local REA.
 * CPU values only. No private writes, mbuf/DMA ownership or firmware emulation.
 */
#ifndef KGP_BE_AWDL_AVAILABILITY_HPP
#define KGP_BE_AWDL_AVAILABILITY_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace be_awdl {
// The matched native GET/SET134 wrappers require/copy exactly24 bytes.
// Native configure supplies state/master/mode; unknown bytes stay opaque.
struct SyncRequest {
    uint32_t version, state;
    uint8_t master[6], reserved14[2];
    uint8_t mode16, opaque17[7];
};
struct WindowStart {
    uint32_t sequence, state;
    uint8_t master[6], topMaster[6];
    uint32_t firmwareTimeBE, channelVersion, channel, channelFlags;
};
// Exact Legacy0x21214:42 is an intermediate extension, NOT final closure.
// Event43 consumes time+4, associated-AP beacon count+12 and, only when
// ccaVersion==1, CCA words+24..36. Unused/unsupported telemetry stays zero.
struct WindowEnd {
    uint32_t reserved0, firmwareTimeBE, reserved8, observedInfraBeacons;
    uint16_t ccaVersion, reserved18;
    uint32_t reserved20, ccaDuration, ccaTx, ccaRx, ccaOther;
};
static_assert(sizeof(SyncRequest)==24 && offsetof(SyncRequest,mode16)==16,"native134 ABI");
static_assert(sizeof(WindowStart)==36 && offsetof(WindowStart,firmwareTimeBE)==20 &&
              offsetof(WindowStart,channelVersion)==24,"native41 ABI");
static_assert(sizeof(WindowEnd)==40 && offsetof(WindowEnd,firmwareTimeBE)==4 &&
              offsetof(WindowEnd,observedInfraBeacons)==12 &&
              offsetof(WindowEnd,ccaVersion)==16 && offsetof(WindowEnd,ccaOther)==36,
              "matched native43 consumed prefix; optional CCA disabled");
constexpr uint32_t StartMessage=41, EndMessage=43, SyncChangedMessage=46;
inline uint32_t big32(uint32_t x) { return (x>>24)|((x>>8)&0xff00)|((x<<8)&0xff0000)|(x<<24); }
inline bool unicast(const uint8_t p[6]) {
    return !(p[0]&1) && (p[0]|p[1]|p[2]|p[3]|p[4]|p[5]);
}
struct NativeConfig {
    uint32_t flags, monitorPeriod,afTxMode;
    uint8_t sequenceLength, duplicateCount, master[6], topMaster[6], self[6];
};
// REA configure/reportDataPathEvents/monitorTimeline/logAWStart establish these
// reads. A native class-name/retain/same-workloop check is required by caller.
inline bool readConfig(const void *object,size_t classSize,NativeConfig &out) {
    if (!object || classSize<0x8a4a) return false;
    const auto *p=static_cast<const uint8_t *>(object);
    memcpy(&out.flags,p+0x8528,4); memcpy(&out.monitorPeriod,p+0x84b8,4);
    out.sequenceLength=p[0x8a34]; out.duplicateCount=p[0x8a49];
    memcpy(out.master,p+0x2801,6); memcpy(out.topMaster,p+0x27fb,6);
    memcpy(out.self,p+0x27ec,6); // configure/setElectionParamsToSelf source identity
    memcpy(&out.afTxMode,p+0x27a8,4); // exact setAFTXMode0x312c0/get0x48422
    return true;
}
inline bool sameNativePlan(const NativeConfig &current,const NativeConfig &announced) {
    return (current.flags&3)==3 && !(current.flags&0x10) &&
        current.monitorPeriod==announced.monitorPeriod &&
        current.sequenceLength==announced.sequenceLength &&
        current.duplicateCount==announced.duplicateCount &&
        !memcmp(current.master,announced.master,6) &&
        !memcmp(current.topMaster,announced.topMaster,6) &&
        !memcmp(current.self,announced.self,6);
}
inline bool ready(const NativeConfig &c,const SyncRequest &r) {
    // These are actual configured/sync-active flags and initialized native
    // divisors, not Finder window state. No guessed state or capability bit.
    return (c.flags&3)==3 && !(c.flags&0x10) && c.monitorPeriod &&
           c.sequenceLength && r.version<=1 && r.state>=1 && r.state<=3 &&
           unicast(c.master) && unicast(c.topMaster) && unicast(c.self);
}
inline bool timing(uint32_t lengthTu,uint32_t periodTu,uint32_t presence,
                   uint32_t &durationTu,uint64_t &intervalNs) {
    // Native0x2682c expects period*presence for one completed extended AW;
    // 16TU every64TU is not the native presence4/period16 contract. This
    // remains one finite co-channel firmware lease, not invented extensions
    // or a promise of exact phase scheduling by SESSION_PROTECTION.
    if (!lengthTu || lengthTu>128 || periodTu<lengthTu || periodTu>65535 ||
        !presence || presence>255 || uint64_t(periodTu)*presence>128) return false;
    durationTu=periodTu*presence; intervalNs=uint64_t(durationTu)*1024000ULL;
    return true;
}
inline bool txWindow(uint64_t phase,uint64_t startNs,uint64_t durationTu,uint64_t nowNs) {
    if (phase!=2 || !startNs || !durationTu || durationTu>128 || nowNs<startNs) return false;
    const uint64_t lengthNs=durationTu*1024000ULL;
    // Software admission margin only, not a promised firmware/air latency.
    return lengthNs>4000000ULL && nowNs-startNs<lengthNs-4000000ULL;
}
}
#endif
