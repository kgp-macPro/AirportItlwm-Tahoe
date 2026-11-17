/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP. Passive Legacy service/offload observations only.
 * No cached payload, native owner, peer lifetime, refresh or merge policy.
 */
#ifndef KGP_LEGACY_AWDL_SERVICE_EVIDENCE_HPP
#define KGP_LEGACY_AWDL_SERVICE_EVIDENCE_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace be_awdl {
struct LegacyServiceEvidence {
    enum : unsigned { Events=32 };
    struct Event { uint64_t v[12]={}; };
    uint64_t stats[16]={228,1,16*8+Events*sizeof(Event)};
    Event events[Events]={};
    static uint16_t le16(const uint8_t *p) { return uint16_t(p[0])|(uint16_t(p[1])<<8); }
    // Bounded structural observation, NOT a replacement for Apple's parser.
    // -1 means no unique Service Parameters/SUI or an incomplete TLV stream.
    static int inspect(const uint8_t *f,size_t n,unsigned &groups) {
        groups=0;
        if(!f || n<40 || memcmp(f+24,"\x7f\x00\x17\xf2\x08\x10",6))return -1;
        int sui=-1;
        for(size_t at=40;at<n;) {
            if(n-at<3)return -1;
            const size_t bytes=size_t(le16(f+at+1))+3;
            if(bytes>n-at)return -1;
            if(f[at]==6) {
                if(sui!=-1 || bytes<8)return -1;
                sui=le16(f+at+6);
            }
            if(f[at]==2)++groups;
            at+=bytes;
        }
        return sui;
    }
    void record(const uint8_t *f,size_t n,uint64_t observed,uint64_t now,
                uint64_t publication,uint32_t generation,uint32_t epoch,
                uint64_t before,uint64_t after,bool returned) {
        unsigned groups=0;const int sui=inspect(f,n,groups);
        const uint64_t serial=++stats[3];
        if(serial>Events)++stats[4];
        ++stats[5];stats[6]+=groups;
        if(sui<0)++stats[7];
        if(before==UINT64_MAX)++stats[8];
        if(after==UINT64_MAX)++stats[9];
        if(returned)++stats[10];
        uint64_t mac=0,hash=1469598103934665603ULL;
        if(f && n>=16)memcpy(&mac,f+10,6);
        for(size_t i=0;f && i<n;++i){hash^=f[i];hash*=1099511628211ULL;}
        // Native words: SUI | changed-byte<<16 | offload-count-byte<<24.
        // Native bool is just a return value, never a service-admission ACK.
        const uint64_t v[12]={serial,now,mac,publication,
            (uint64_t(generation)<<32)|epoch,observed,uint64_t(sui<0?UINT32_MAX:uint32_t(sui)),
            groups,before,after,hash,uint64_t(returned)};
        memcpy(events[(serial-1)%Events].v,v,sizeof(v));
    }
};
static_assert(sizeof(LegacyServiceEvidence::Event)==96,"passive service record228");
static_assert(offsetof(LegacyServiceEvidence,events)==128,"contiguous evidence228");
} // namespace be_awdl
#endif
