/* Private BZ/GL contracts, KGP 2026. GPL-2.0-or-later.
 * Hardware definitions derive from pinned Intel iwlwifi/AirportBZ references;
 * these ownership helpers are shared verbatim by the Kext and host tests. */
#ifndef ITL_IWX_BZ_CONTRACTS_HPP
#define ITL_IWX_BZ_CONTRACTS_HPP
#include <stdint.h>
#include <stddef.h>
namespace kgp_bz {
// The private GL92 baseline is an explicit firmware/PNVM pair. Do not use
// the compatibility strrchr macro (it returns AFTER the last delimiter).
constexpr char FirmwareName[] = "iwlwifi-gl-c0-fm-c0-92.ucode";
constexpr char PnvmName[] = "iwlwifi-gl-c0-fm-c0.pnvm";
inline const char *pnvmForFirmware(const char *name) {
    if (!name) return nullptr;
    for (size_t i=0; i<sizeof(FirmwareName); ++i)
        if (name[i]!=FirmwareName[i]) return nullptr;
    return PnvmName;
}
constexpr unsigned RxCount = 512; // One coherent 512-entry Gen3 CB, log2=9.
constexpr unsigned CompletionBytes = 4, TransferBytes = 16, RxBytes = 4096;
constexpr uint32_t BusMasterDisabled = 1U << 28;
inline bool ioValid(uint32_t v) { return v != UINT32_MAX && (v & 0xfffffff0U) != 0xa5a5a5a0U; }
inline bool msixAccepted(int setup,int discovered,bool message,bool msixEnabled) {
    return setup==0 && discovered>=0 && message && msixEnabled;
}
inline bool masterStopped(uint32_t v) { return ioValid(v) && (v & BusMasterDisabled); }
inline bool deviceAllowed(uint16_t vendor,uint16_t dev,unsigned mac,unsigned macStep,unsigned rf,unsigned rfStep) {
    return vendor==0x8086 && dev==0x272b && mac==0x47 && macStep==2 && rf==0x112 && rfStep==2;
}
inline bool dmaFits(uint64_t a,uint64_t n,uint64_t align) {
    return n && align && !(align & (align-1)) && !(a & (align-1)) &&
        a <= UINT64_MAX-n && (a>>32)==((a+n)>>32);
}
inline bool version(unsigned got,unsigned expected,bool implicitV1=false) {
    return got==expected || (implicitV1 && expected==1 && got==99);
}
inline uint16_t le16(const void *p) { auto b=static_cast<const uint8_t *>(p);return b[0]|(uint16_t(b[1])<<8); }
inline uint32_t le32(const void *p) { auto b=static_cast<const uint8_t *>(p);return le16(b)|(uint32_t(le16(b+2))<<16); }
/* Bounds envelope used before the production GL92 TLV decoder. */
inline bool validTlvs(const uint8_t *p,size_t n) {
    if(!p || !n) return false;
    while(n) {
        if(n<8) return false;
        size_t len=le32(p+4);p+=8;n-=8;
        if(len>n || len>SIZE_MAX-3) return false;
        size_t padded=(len+3)&~size_t(3);
        if(padded>n) return false;
        p+=padded;n-=padded;
    }
    return true;
}
struct Completion { uint16_t rbid;uint8_t flags,reserved; } __attribute__((packed));
static_assert(sizeof(Completion)==CompletionBytes,"BZ completion ABI");
/* Command slot reuse requires BOTH firmware completion and caller release.
 * The wire carries an index, not this software serial. Reset finality remains
 * the positive hardware terminal + gate contract, not a serial comparison. */
struct CommandLedger {
    static constexpr unsigned Count=128;
    struct Entry {uint32_t serial=0,code=0,generation=0,epoch=0;bool active=false,done=false,caller=false,uncertain=false;} e[Count];
    bool available(unsigned i) const {return i<Count && !e[i].active;}
    uint32_t begin(unsigned i,uint32_t code,uint32_t g,uint32_t epoch,bool async) {
        if(!available(i))return 0;
        auto &x=e[i];if(!++x.serial)++x.serial;
        x.code=code;x.generation=g;x.epoch=epoch;x.active=true;x.done=false;x.caller=async;x.uncertain=false;
        return x.serial;
    }
    bool expected(unsigned i,uint32_t code,uint32_t g,uint32_t epoch) const {
        if(i>=Count)return false;const auto &x=e[i];
        return x.active&&!x.done&&!x.uncertain&&x.code==code&&x.generation==g&&x.epoch==epoch;
    }
    bool complete(unsigned i,uint32_t code,uint32_t g,uint32_t epoch) {
        if(!expected(i,code,g,epoch))return false;auto &x=e[i];x.done=true;
        if(x.caller)x.active=false;return true;
    }
    bool completed(unsigned i,uint32_t serial) const {return i<Count&&e[i].serial==serial&&e[i].done&&!e[i].uncertain;}
    void finish(unsigned i,uint32_t serial,bool success) {
        if(i>=Count || e[i].serial!=serial)return;auto &x=e[i];x.caller=true;
        if(!success){x.uncertain=true;x.active=true;}else if(x.done)x.active=false;
    }
    bool reset(bool positiveTerminal) {
        for(const auto &x:e)if(x.active&&!positiveTerminal)return false;
        for(auto &x:e){if(!++x.serial)++x.serial;x.active=x.done=x.caller=x.uncertain=false;}
        return true;
    }
};
/* A tag identifies a buffer, never a completion-ring position. Tags have no
 * hardware generation field. Cross-lifetime finality therefore additionally
 * requires positive DMA stop/reset, cleared ring memory and serialized IRQs. */
struct RxLedger {
    enum State : uint8_t { Unissued, Prepared, Posted, Completed };
    uint8_t state[RxCount] = {};
    uint16_t descriptorTag[RxCount] = {};
    uint32_t generation=0,epoch=0,serviceSerial=0;
    unsigned write=0,published=0,active=0;
    bool issued=false,closed=true,servicing=false;
    bool reset(uint32_t g,uint32_t e,bool positiveTerminal) {
        if (issued && !positiveTerminal) { closed=true;return false; }
        ++serviceSerial;generation=g;epoch=e;write=published=active=0;
        issued=false;closed=false;servicing=false;
        for(unsigned i=0;i<RxCount;++i){state[i]=Unissued;descriptorTag[i]=0;}
        return true;
    }
    bool current(uint32_t g,uint32_t e) const {return !closed && generation==g && epoch==e;}
    uint32_t begin(uint32_t g,uint32_t e) {
        if(!current(g,e)||servicing)return 0;
        servicing=true;if(!++serviceSerial)++serviceSerial;return serviceSerial;
    }
    bool serviceCurrent(uint32_t cookie,uint32_t g,uint32_t e) const {
        return cookie && cookie==serviceSerial && servicing && current(g,e);
    }
    void end(uint32_t cookie) {if(cookie && cookie==serviceSerial)servicing=false;}
    bool prepare(unsigned dataIndex,uint32_t g,uint32_t e) {
        if(!current(g,e)||dataIndex>=RxCount ||
           (state[dataIndex]!=Unissued && state[dataIndex]!=Completed))return false;
        if(state[dataIndex]==Unissued) {if(active>=RxCount-1)return false;++active;}
        descriptorTag[write]=uint16_t(dataIndex+1);state[dataIndex]=Prepared;
        write=(write+1)&(RxCount-1);return true;
    }
    bool publish(uint32_t g,uint32_t e,unsigned *doorbell) {
        if(!current(g,e)||!doorbell)return false;
        unsigned target=write & ~7U;
        for(unsigned i=published;i!=target;i=(i+1)&(RxCount-1)){
            unsigned tag=descriptorTag[i];if(!tag||tag>RxCount||state[tag-1]!=Prepared)return false;
        }
        for(unsigned i=published;i!=target;i=(i+1)&(RxCount-1))state[descriptorTag[i]-1]=Posted;
        published=target;issued=true;*doorbell=target;return true;
    }
    bool claim(unsigned tag,uint32_t g,uint32_t e,unsigned *dataIndex) {
        if(!current(g,e)||!dataIndex||!tag||tag>RxCount||state[tag-1]!=Posted)return false;
        state[tag-1]=Completed;*dataIndex=tag-1;return true;
    }
    void close() {closed=true;}
};
/* First candidate deliberately fails the entire generation on fragmented
 * notifications. It never dispatches a partial boot/key/command response as
 * complete. No discard-and-success policy; no unbounded fragment allocation. */
inline bool completeMessage(unsigned flags) {return !(flags&1U);}
}
#endif
