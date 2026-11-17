/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP.
 * Optional comparison, not a claimed camera fix. Mechanism adapted from
 * retropineaple/ForbiddenItlwm b384a4dbfda5cc975b9c1db02f5e484075310620,
 * AWDLFrameBuilder.hpp::stripDataPathInfra (GPL-2.0), cross-checked with
 * seemoo-lab/OWL and exact EFI-Legacy build/parse DataPath TLV REA.
 * Deliberate Legacy difference: preserve flag0x0002 and its native AWDL
 * address. Only suppress the optional infrastructure BSSID/channel field.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace be_awdl {
enum class DpStateTestResult : uint32_t {
    Unchanged=0, NotEligible=1, Invalid=2, Duplicate=3,
    UnsupportedPrefix=4, Changed=5
};
struct DpStateTestInfo {
    uint16_t beforeFlags=0, afterFlags=0, beforeValueBytes=0, afterValueBytes=0;
};

// Bounded, transactional CPU-copy edit. No native/template object is changed.
// The complete TLV run is checked BEFORE modifying any byte. Unknown tails
// and following TLVs are preserved verbatim. Native UMI/OOB is excluded by
// the caller; only our own broadcast PSF/MIF can satisfy this header check.
inline DpStateTestResult omitDpStateInfra(uint8_t *frame,size_t capacity,
        size_t &length,const uint8_t own[6],DpStateTestInfo &info)
{
    info=DpStateTestInfo{};
    if(!frame || !own || length>capacity || length<40)return DpStateTestResult::Invalid;
    static const uint8_t broadcast[6]={255,255,255,255,255,255};
    static const uint8_t zero[6]={};
    static const uint8_t vendor[6]={127,0,0x17,0xf2,8,0x10};
    if((own[0]&1) || !memcmp(own,zero,6) || frame[0]!=0xd0 || (frame[1]&0xc3) || memcmp(frame+4,broadcast,6) ||
       memcmp(frame+10,own,6) || memcmp(frame+24,vendor,6) ||
       (frame[30]!=0 && frame[30]!=3))return DpStateTestResult::NotEligible;
    size_t target=0,valueBytes=0;unsigned count=0;
    for(size_t at=40;at<length;) {
        if(length-at<3)return DpStateTestResult::Invalid;
        const size_t bytes=size_t(frame[at+1])|(size_t(frame[at+2])<<8);
        if(bytes>length-at-3)return DpStateTestResult::Invalid;
        if(frame[at]==12){target=at;valueBytes=bytes;++count;}
        at+=3+bytes;
    }
    if(!count)return DpStateTestResult::Unchanged;
    if(count!=1)return DpStateTestResult::Duplicate;
    if(valueBytes<2)return DpStateTestResult::Invalid;
    uint8_t *v=frame+target+3;
    const uint16_t flags=uint16_t(v[0])|(uint16_t(v[1])<<8);
    info.beforeFlags=info.afterFlags=flags;
    info.beforeValueBytes=info.afterValueBytes=uint16_t(valueBytes);
    if(!(flags&1))return DpStateTestResult::Unchanged;
    // This experiment is qualified for the native Legacy country+social
    // prefix. Other valid/unknown variants keep their original bytes.
    if((flags&0x0300)!=0x0300)return DpStateTestResult::UnsupportedPrefix;
    constexpr size_t prefix=2+3+2,cut=8;
    if(valueBytes<prefix+cut+((flags&2)?6:0))return DpStateTestResult::Invalid;
    const size_t from=target+3+prefix+cut;
    memmove(frame+target+3+prefix,frame+from,length-from);
    length-=cut;
    valueBytes-=cut;
    const uint16_t next=flags&~uint16_t(1);
    v[0]=uint8_t(next);v[1]=uint8_t(next>>8);
    frame[target+1]=uint8_t(valueBytes);frame[target+2]=uint8_t(valueBytes>>8);
    info.afterFlags=next;info.afterValueBytes=uint16_t(valueBytes);
    return DpStateTestResult::Changed;
}
}
