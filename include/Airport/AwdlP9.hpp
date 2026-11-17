/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP
 * Private native AWDL boundary probe. No firmware/peer/queue ownership here.
 * Wire observations, not an AirDrop implementation or transmission proof.
 */
#ifndef KGP_AWDL_P9_HPP
#define KGP_AWDL_P9_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace awdl_p9 {
// R2: firmware RX GP2 is genuine local device time. A bounded monotonic
// extrapolation is an estimate, never an on-air TX timestamp or scheduler proof.
inline bool estimateGp2(uint32_t anchor, uint64_t hostAnchorNs,
                        uint64_t nowNs, uint32_t &result)
{
    if (!hostAnchorNs || nowNs < hostAnchorNs || nowNs - hostAnchorNs > 2000000000ULL)
        return false;
    result = anchor + uint32_t((nowNs - hostAnchorNs) / 1000ULL);
    return true; // modulo32 microseconds, including legitimate zero at wrap
}
struct NativeInputGates { uint32_t cd4; uint8_t flags8528, bypass8950; };
// Read-only observations of exact Legacy4C07538B native comparisons, verified
// with REA. Bounds check is mandatory; no interpretation as forced enable state.
inline bool readNativeInputGates(const void *object, size_t classSize, NativeInputGates &out)
{
    if (!object || classSize < 0x8951) return false;
    const auto *bytes = static_cast<const uint8_t *>(object);
    memcpy(&out.cd4, bytes + 0xcd4, sizeof(out.cd4));
    out.flags8528 = bytes[0x8528]; out.bypass8950 = bytes[0x8950];
    return true;
}
constexpr unsigned capacity = 64, prefixSize = 128, templateCapacity = 2048;
enum Kind : uint64_t { Request=1, ActionOutput=2, BpfOutput=3, TxRequest=4,
    TemplateSet=5, TemplateGet=6, Enable=7, Disable=8, TxSelector=9, RequestReturn=10 };
// All fields are unsigned little-endian 64-bit words on this x86_64 target.
// No pointers, kernel addresses, mbuf handles or firmware-owned memory retained.
struct Event { uint64_t serial, time, kind, arg, length, copied, result, context;
    uint8_t prefix[prefixSize]; };
struct Wire { uint64_t schema, version, bytes, session, submitted, overwritten,
    contentionDrops, copied, enabled, clockNumer, clockDenom, staleRejects;
    Event events[capacity]; };
static_assert(sizeof(Event)==192,"event ABI");
static_assert(sizeof(Wire)==12384,"wire ABI");
// No logging allocation and no blocking lock. A racing writer drops its own
// record; it never overwrites an in-flight snapshot. Loss is explicit.
class Trace {
    uint32_t lock_=0, enabled_=0;
    uint64_t drops_=0, stale_=0, token_=0;
    Wire state_={};
    bool enter() { uint32_t expected=0; return __atomic_compare_exchange_n(
        &lock_,&expected,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED); }
    void leave() { __atomic_store_n(&lock_,0U,__ATOMIC_RELEASE); }
public:
    bool enabled() const { return __atomic_load_n(&enabled_,__ATOMIC_ACQUIRE)!=0; }
    bool start() {
        if (!enter()) return false;
        if (enabled()) {leave();return false;}
        uint64_t session=state_.session+1;
        if (!session) {leave();return false;}
        memset(&state_,0,sizeof(state_));
        state_.schema=209;state_.version=1;state_.bytes=sizeof(Wire);
        state_.session=session;__atomic_store_n(&token_,session,__ATOMIC_RELEASE);
        __atomic_store_n(&enabled_,1U,__ATOMIC_RELEASE);leave();return true;
    }
    void stop() { __atomic_store_n(&enabled_,0U,__ATOMIC_RELEASE); }
    uint64_t ticket() const {
        if (!enabled()) return 0;
        return __atomic_load_n(&token_,__ATOMIC_ACQUIRE);
    }
    bool record(uint64_t time, Kind kind, uint64_t arg, uint64_t length,
                const void *p, size_t n, uint64_t result, uint64_t context) {
        return recordWithTicket(ticket(),time,kind,arg,length,p,n,result,context);
    }
    bool recordWithTicket(uint64_t ticket, uint64_t time, Kind kind, uint64_t arg,
                uint64_t length,const void *p,size_t n,uint64_t result,uint64_t context) {
        if (!ticket) return false;
        if (!enter()) {__atomic_fetch_add(&drops_,1ULL,__ATOMIC_RELAXED);return false;}
        if (!enabled() || ticket!=state_.session) {
            __atomic_fetch_add(&stale_,1ULL,__ATOMIC_RELAXED);leave();return false;
        }
        if (state_.submitted==UINT64_MAX) {leave();return false;}
        uint64_t serial=++state_.submitted;
        Event &e=state_.events[(serial-1)%capacity];memset(&e,0,sizeof(e));
        e.serial=serial;e.time=time;e.kind=kind;e.arg=arg;e.length=length;
        e.result=result;e.context=context;
        e.copied=p?(n<prefixSize?n:prefixSize):0;
        if (e.copied>length) e.copied=length;
        if (e.copied) memcpy(e.prefix,p,e.copied);
        state_.overwritten=serial>capacity?serial-capacity:0;leave();return true;
    }
    bool snapshot(Wire &out, uint32_t numer, uint32_t denom) {
        if (!enter()) return false;
        if (!state_.session) {leave();return false;}
        memcpy(&out,&state_,sizeof(out));
        out.contentionDrops=__atomic_load_n(&drops_,__ATOMIC_RELAXED);
        out.staleRejects=__atomic_load_n(&stale_,__ATOMIC_RELAXED);
        out.copied=out.submitted<capacity?out.submitted:capacity;
        out.enabled=enabled();out.clockNumer=numer;out.clockDenom=denom;
        leave();return true;
    }
};
// CPU-only immutable-under-lock template. Replacement validates before commit;
// a short getter never writes past the caller's capacity or frees the old value.
// The caller supplies a framework-marshalled kernel pointer, never copyin here.
class Template {
    uint32_t lock_=0;
    uint32_t length_=0;
    uint8_t bytes_[templateCapacity]={};
    bool enter() {uint32_t e=0;return __atomic_compare_exchange_n(&lock_,&e,1,
        false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED);}
    void leave(){__atomic_store_n(&lock_,0U,__ATOMIC_RELEASE);}
public:
    enum Result { OK=0, Invalid=1, TooLarge=2, Busy=3, Empty=4, Short=5 };
    Result replace(const void *p,uint32_t n) {
        if (!p || !n) return Invalid;
        if (n>templateCapacity) return TooLarge;
        if (!enter()) return Busy;
        memcpy(bytes_,p,n);length_=n;leave();return OK;
    }
    Result copy(void *p,uint32_t &n) {
        if (!enter()) return Busy;
        uint32_t available=n;n=length_;
        if (!length_) {leave();return Empty;}
        if (!p || available<length_) {leave();return Short;}
        memcpy(p,bytes_,length_);leave();return OK;
    }
};
// Strict capture classifier, not injection permission. 802.11 action frame
// header24 + AWDL action header16; body includes vendor category/OUI/type/version.
// See pinned MIT Owly SPEC and GPL OWL for protocol context; no code imported.
enum Frame { Other=0, Action=1, Truncated=2, Malformed=3, Protected=4 };
inline Frame classify(const uint8_t *p,size_t n) {
    if (!p || n<2) return Truncated;
    if ((p[0]&0xfc)!=0xd0) return Other;
    if (p[1]&0x40) return Protected;
    if ((p[1]&3)!=0 || (p[1]&4)) return Malformed; // DS/MoreFragments
    if (n<40) return Truncated;
    if (p[24]!=0x7f || p[25]!=0 || p[26]!=0x17 || p[27]!=0xf2 || p[28]!=8)
        return Other;
    if (p[29]!=0x10 || (p[30]!=0 && p[30]!=3) || p[31]!=0) return Malformed;
    if ((p[10]&1) || !(p[10]|p[11]|p[12]|p[13]|p[14]|p[15]) || (p[22]&15))
        return Malformed;
    size_t at=40;
    while (at<n) {if (n-at<3)return Truncated;
        size_t len=size_t(p[at+1])|(size_t(p[at+2])<<8);
        at+=3;if(len>n-at)return Truncated;at+=len;}
    return Action;
}
}
#endif
