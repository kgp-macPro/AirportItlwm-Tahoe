/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP. Bounded CPU evidence, no native/FW ownership.
 */
#ifndef KGP_BE_AWDL_EVIDENCE_HPP
#define KGP_BE_AWDL_EVIDENCE_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace be_awdl {
struct NativePeerEvidence {
    uint8_t address[6]={}; uint32_t reachability=0,syncFrames=0,observations=0;
    uint64_t posted=0;uint16_t sequence=0;
    uint8_t pending=0,cached=0,overlap=0,version=0,parsedSequence=0;
};
// Exact Legacy ac2dc15b: findPeer/AFReport/SyncParser/shouldStopAFProcessing.
// Caller MUST hold the manager's native recursive peer-list lock and qualify
// the actual AWDL peer class. A positive lookup alone can be a default peer.
inline bool readNativePeer(const void *object,size_t bytes,NativePeerEvidence &o) {
    if(!object || bytes<0xcc6)return false;
    const auto *p=static_cast<const uint8_t *>(object);
    memcpy(o.address,p+0x20,6);memcpy(&o.reachability,p+0x8a4,4);
    uint64_t ndp=0;memcpy(&ndp,p+0xa0,8);o.posted=ndp!=0;
    o.pending=p[0x250];o.cached=p[0x251];memcpy(&o.syncFrames,p+0x5e0,4);
    o.overlap=p[0x868];o.version=p[0x881];o.parsedSequence=p[0xcc5];
    memcpy(&o.sequence,p+0x8a2,2);memcpy(&o.observations,p+0x8a8,4);
    return true;
}
struct WireEvidence {
    // Two bounded records per kind: retained first and latest. Full counts
    // remain, but frequent raw NoDS sampling is capped at10Hz, no RX credit
    // or mbuf retained. Payload may be truncated and is private test evidence.
    enum {TxAction=1,RxAction=2,TxData=3,RxNoDs=4,NativeRxData=5,PreparedAction=6,PreparedData=7};
    struct Entry {uint64_t ticket,kind,hostNs,clockChannel,lifetime,length,result,flags;uint8_t bytes[640];};
    uint64_t header[8]={213,1,sizeof(uint64_t)*8+sizeof(Entry)*14,0,0,0,0,0};
    Entry entries[14]={};
    uint64_t record(unsigned kind,const void *data,size_t bytes,uint64_t now,
                    uint32_t gp2,uint16_t channel,uint32_t generation,uint32_t epoch) {
        if(kind<1 || kind>7 || !data || !bytes)return 0;
        if(kind==RxNoDs) {
            ++header[6];const auto &last=entries[(kind-1)*2+1].ticket?entries[(kind-1)*2+1]:entries[(kind-1)*2];
            if(last.ticket && now>=last.hostNs && now-last.hostNs<100000000ULL){++header[5];return 0;}
        }
        const unsigned first=(kind-1)*2,slot=entries[first].ticket?first+1:first;
        auto &e=entries[slot];memset(&e,0,sizeof(e));e.ticket=++header[3];e.kind=kind;e.hostNs=now;
        e.clockChannel=uint64_t(gp2)|(uint64_t(channel)<<32);e.lifetime=(uint64_t(generation)<<32)|epoch;
        const size_t copy=bytes<sizeof(e.bytes)?bytes:sizeof(e.bytes);e.length=uint64_t(bytes)|(uint64_t(copy)<<32);
        e.result=UINT64_MAX;memcpy(e.bytes,data,copy);header[4]=slot;return e.ticket;
    }
    void complete(uint64_t ticket,uint64_t result,uint64_t flags=0) {
        if(!ticket)return;
        for(auto &e:entries)if(e.ticket==ticket){e.result=result;e.flags=flags;return;}
        ++header[7]; // reentry overwrote evidence; never assign it to another packet
    }
};
static_assert(sizeof(WireEvidence)==9920,"bounded wire evidence ABI213");
struct DataCompletionEvidence {
    // A bounded CPU-only record paired to the original mbuf ticket.
    // No payload, pointer, reference, DMA state or native status mutation.
    enum : unsigned { Count=24 };
    struct Entry {
        uint64_t ticket, submittedNs, completedNs, macLe, status, detail, lifetime, flags;
    };
    uint64_t header[8]={221,1,8*sizeof(uint64_t)+(Count+2)*sizeof(Entry)};
    Entry entries[Count]={};
    Entry firstFailure={},lastFailure={};

    void record(uint64_t ticket,uint64_t submittedNs,uint64_t nowNs,const uint8_t *destination,
                int status,uint32_t detail,uint32_t generation,uint32_t epoch,
                bool current,bool success) {
        uint64_t address=0;
        if(destination)for(unsigned i=0;i<6;++i)address|=uint64_t(destination[i])<<(8*i);
        auto &entry=entries[header[3]%Count];
        entry={ticket,submittedNs,nowNs,address,uint64_t(int64_t(status)),detail,
               (uint64_t(generation)<<32)|epoch,
               uint64_t(destination!=nullptr)|(uint64_t(current)<<1)|(uint64_t(success)<<2)};
        ++header[3];
        if(status==0x02 && success)++header[4];
        if(!success) {
            if(!header[5])firstFailure=entry;
            lastFailure=entry;
            ++header[5];
        }
    }
};
static_assert(sizeof(DataCompletionEvidence)==1728,"bounded data terminal evidence ABI221");
}
#endif
