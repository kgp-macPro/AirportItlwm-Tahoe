/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP. AWDL CPU/wire helpers; no DMA/native ownership.
 * GL92 envelopes: Intel Linux v6.12 adc218676eef (GPL-2.0 OR BSD-3-Clause).
 */
#ifndef KGP_BE_AWDL_DATA_TRANSPORT_HPP
#define KGP_BE_AWDL_DATA_TRANSPORT_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "BeAwdlData.hpp"
namespace be_awdl {
// Linux v6.12 iwl_mvm_rx_tx_cmd_single accepts SUCCESS and DIRECT_DONE.
// DIRECT_DONE can finish group delivery locally, where no peer ACK is due.
// Require the stronger status for unicast until its GL92 ACK is established.
static inline constexpr bool dataTxSucceeded(int status,bool group)
{
    return status==0x01 || (group && status==0x02);
}
static inline bool concurrentGoAllowed(uint32_t raw,bool current,bool run,bool sameChannel)
{
    // NVM VALID0, IBSS1, ACTIVE3, RADAR4, GO_CONCURRENT6, DFS7.
    return current && run && sameChannel && (raw&1) && !(raw&0x90) &&
        (((raw&0x0a)==0x0a) || (raw&0x40));
}
// The data transport is legacy OFDM/20MHz. GO permission must be checked
// for that exact primary channel, never inferred for the STA's wider PHY.
static inline bool dataChannelAllowed(uint32_t raw,bool current,bool run,bool sameChannel)
{
    return (raw&0x100) && concurrentGoAllowed(raw,current,run,sameChannel);
}
struct NativeDataState {uint32_t mode=0,available=0;uint8_t open=0,presenceMode=0;};
static inline bool readNativeDataState(const void *manager,size_t classBytes,NativeDataState &out)
{
    // Exact Legacy ac2dc15b, dequeue0x44ea8/0x6916a. Caller must also
    // qualify the actual AWDL manager class. Read-only, no forced state.
    if(!manager || classBytes<0x8951)return false;
    const auto *p=static_cast<const uint8_t *>(manager);memcpy(&out.mode,p+0x6f2c,4);
    memcpy(&out.available,p+0xb20,4);memcpy(&out.open,p+0x260,1);memcpy(&out.presenceMode,p+0xce8,1);return true;
}
constexpr uint32_t DataBeaconIntervalTu=100,DataDtimPeriod=1;
struct Beacon13 {uint16_t bytes,flags;uint32_t shortSsid,reserved,link,timIndex,timSize,ecsa,csa;} __attribute__((packed));
static_assert(sizeof(Beacon13)==32,"GL92 beacon13 header");
static inline size_t dataBeacon(uint8_t *out,size_t capacity,const uint8_t own[6])
{
    static const uint8_t ies[]={0,0,1,8,0x8c,0x12,0x98,0x24,0xb0,0x48,0x60,0x6c,5,4,0,1,0,0};
    const size_t size=36+sizeof(ies);if(!out||!own||capacity<size||(own[0]&1))return 0;
    memset(out,0,size);out[0]=0x80;memset(out+4,255,6);memcpy(out+10,own,6);memcpy(out+16,own,6);
    out[32]=uint8_t(DataBeaconIntervalTu);out[33]=uint8_t(DataBeaconIntervalTu>>8);out[34]=1;memcpy(out+36,ies,sizeof(ies));return size;
}
static inline size_t dataFrame(uint8_t *out,size_t capacity,const uint8_t *ether,size_t bytes,const uint8_t own[6],uint16_t sequence,const uint8_t *nativeBssid=nullptr)
{
    static const uint8_t llc[]={0xaa,0xaa,3,0,0x17,0xf2,0x08,0};
    static const uint8_t bssid[]={0,0x25,0,0xff,0x94,0x73};
    if(!out||!ether||!own||bytes<14+40||bytes>DataMaxNativeBytes||memcmp(ether+6,own,6))return 0;
    const uint16_t type=(uint16_t(ether[12])<<8)|ether[13];size_t payload=bytes-14;const uint8_t *ip=ether+14;
    const size_t nativeIp=type==payload?dataPayload(ip,payload):0;
    const bool pre=nativeIp!=0;
    if(pre)ip+=nativeIp;else if(type!=0x86dd)return 0;
    // Exact Legacy addAwdlSnapHeader0x4456e adds16 bytes to the original
    // Ethernet packet and writes its enlarged payload length. A1500-byte
    // IPv6 packet therefore has1516 bytes of LLC/AWDL, not1484 bytes of IP.
    const size_t ipBytes=payload-(pre?nativeIp:0);
    const size_t declared=40U+(size_t(ip[4])<<8)+ip[5];
    if((ip[0]&0xf0)!=0x60 || ipBytes<40 || declared>DataMaxIPv6Bytes || declared>ipBytes)return 0;
    const size_t result=24+payload+(pre?0:16);if(result>DataMaxWireBytes || capacity<result)return 0;
    memset(out,0,24);out[0]=8;memcpy(out+4,ether,6);memcpy(out+10,own,6);memcpy(out+16,nativeBssid?nativeBssid:bssid,6);
    out[22]=uint8_t(sequence<<4);out[23]=uint8_t(sequence>>4);
    if(pre)memcpy(out+24,ether+14,payload);else{memcpy(out+24,llc,8);out[32]=3;out[33]=4;out[34]=uint8_t(sequence);out[35]=uint8_t(sequence>>8);out[36]=out[37]=0;out[38]=0x86;out[39]=0xdd;memcpy(out+40,ether+14,payload);}
    return result;
}
static inline bool assignMulticastSequence(uint8_t *frame,size_t bytes,unsigned ac,uint16_t (&next)[4])
{
    // Exact Legacy ac2dc15b: addAwdlSnapHeader initially leaves sequence=0.
    // AWDLMulticastPeer::prepareForTransmit does not fill it at dequeue.
    // Intel's generic Gen3 TX command transmits the LLC payload unchanged.
    // Finish only this private host-built multicast copy, never the native
    // original or an already prepared native unicast sequence (zero is valid).
    static const uint8_t prefix[]={0xaa,0xaa,3,0,0x17,0xf2,8,0,3,4};
    if(!frame || bytes<80 || frame[0]!=8 || frame[1] ||
       memcmp(frame+24,prefix,sizeof(prefix)) || !dataPayload(frame+24,bytes-24))return false;
    if(!(frame[4]&1))return true;
    if(ac>=4)return false;
    // Native receive accounting uses four independent AC streams: high
    // bits12..13 select AC; low12 are the AWDL sequence, not dot11 seq_ctrl.
    // No restart on ROC/window/firmware epoch: old frames may still exist.
    const uint16_t sequence=uint16_t((ac<<12)|(next[ac]&0x0fff));
    frame[34]=uint8_t(sequence);frame[35]=uint8_t(sequence>>8);
    next[ac]=uint16_t((next[ac]+1)&0x0fff);
    return true;
}
}
#endif
