/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP
 * AWDL wire recognition / native 802.3 handoff, using the protocol described
 * in Bananz0 400019b. Native LLC ownership/decapsulation qualified with REA
 * against Legacy4C07538B, PeerManager::inputPacket0x69adc.
 */
#ifndef KGP_BE_AWDL_DATA_HPP
#define KGP_BE_AWDL_DATA_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace be_awdl {
static const uint8_t DataLlc[8]={0xaa,0xaa,3,0,0x17,0xf2,8,0};
constexpr size_t DataMaxIPv6Bytes=1500;
constexpr size_t DataMaxWireBytes=2304; // existing bounded HAL RX mailbox
constexpr size_t DataMaxPayloadBytes=DataMaxWireBytes-24;
constexpr size_t DataMaxNativeBytes=14+DataMaxPayloadBytes;
// Exact Legacy ac2dc15b awdlLllcInput0x10f9a: the low nibble of the
// little-endian flags counts optional {type,length,payload} TLVs BEFORE
// EtherType. Preserve those bytes for native interpretation; do not assume
// the IPv6 EtherType is always at offset14 or invent option contents.
// Return the IPv6 offset within the complete LLC/AWDL payload, or0.
inline size_t dataPayload(const uint8_t *p,size_t n) {
    if(!p || n<16+40 || n>DataMaxPayloadBytes || memcmp(p,DataLlc,8) || p[8]!=3 || p[9]!=4)return 0;
    size_t at=14;
    for(unsigned options=p[12]&15;options;--options) {
        if(at>n || n-at<2 || !p[at+1] || size_t(p[at+1])>n-at-2)return 0;
        at+=2+p[at+1];
    }
    if(at>n || n-at<2+40 || p[at]!=0x86 || p[at+1]!=0xdd)return 0;
    at+=2;
    const size_t ipBytes=40U+(unsigned(p[at+4])<<8)+p[at+5];
    if((p[at]>>4)!=6 || ipBytes>DataMaxIPv6Bytes || ipBytes>n-at)return 0;
    return at;
}
inline size_t dataHeader(const uint8_t *p,size_t n,bool hardwareDeaggregated=false) {
    if (!p || n<24 || n>DataMaxWireBytes || (p[0]!=0x08 && p[0]!=0x88) ||
        (p[1]&0xc7) || (p[22]&0xf)) return 0; // no DS/fragment/protected/order
    size_t header=p[0]==0x88 ? 26:24;
    if (n<header+16+40 || (header==26 && (p[24]&0x80) && !hardwareDeaggregated)) return 0;
    // ForbiddenItlwm AWDLDataPayload::parseIPv6DataPayload observes this
    // additional LLC envelope on peer frames. Accept exactly one complete
    // inner payload. Its length cannot authorize A-MSDU splitting or padding.
    static const uint8_t outer[6]={0xaa,0xaa,3,0,0,0};
    if (!memcmp(p+header,outer,sizeof(outer))) {
        const size_t innerBytes=(size_t(p[header+6])<<8)|p[header+7];
        if (innerBytes!=n-header-8)return 0;
        header+=8;
    }
    // AirDrop's IPv6 path only. Supply complete headers to native parsing,
    // never a truncated or mis-typed Ethernet payload with guessed metadata.
    if (!dataPayload(p+header,n-header)) return 0;
    return header; // start of the inner LLC expected by the Legacy receiver
}
// GL92 RX_MPDU_MFLG2_AMSDU identifies an already hardware-deaggregated
// subframe. Like Linux adc218676eef mvm/rxmq.c, clear the stale QoS A-MSDU
// bit before interpreting this as one packet. Never infer deaggregation from
// packet bytes, split a raw aggregate, or modify the infrastructure's mbuf.
// The strict inner LLC/IPv6 checks above still qualify the complete subframe.
inline bool copyReceivedData(uint8_t *out,size_t capacity,const uint8_t *p,
                             size_t n,bool hardwareDeaggregated) {
    if (!out || capacity<n || !dataHeader(p,n,hardwareDeaggregated)) return false;
    memcpy(out,p,n);
    if (hardwareDeaggregated && out[0]==0x88) out[24]&=uint8_t(~0x80U);
    return true;
}
// Preserve LLC+AWDL headers. The exact native peer manager decapsulates these
// after genuine peer admission; it may drop/consume the packet. Do not strip
// headers and then pretend its return is delivery or an on-air ACK.
inline size_t nativeData(uint8_t *out,size_t cap,const uint8_t *p,size_t n,
                         const uint8_t own[6],const uint8_t bssid[6]) {
    const size_t h=dataHeader(p,n);
    if (!h || !out || !own || !bssid || cap<14+n-h ||
        (p[10]&1) || !memcmp(p+10,"\0\0\0\0\0\0",6) ||
        !memcmp(p+10,own,6) || memcmp(p+16,bssid,6) ||
        (!(p[4]&1) && memcmp(p+4,own,6))) return 0;
    memcpy(out,p+4,6);memcpy(out+6,p+10,6);
    const size_t payload=n-h;out[12]=uint8_t(payload>>8);out[13]=uint8_t(payload);
    memcpy(out+14,p+h,payload);return 14+payload;
}
}
#endif
