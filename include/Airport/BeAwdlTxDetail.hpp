/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP. Diagnostic copies only; no firmware/native ownership.
 */
#ifndef KGP_BE_AWDL_TX_DETAIL_HPP
#define KGP_BE_AWDL_TX_DETAIL_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace be_awdl {
// Gate-serialized CPU storage. No mbuf, node, DMA pointer or application data.
// Updates require the SAME original and generation/epoch. Diagnostic loss must
// never change an admission, doorbell, reclamation or native completion.
struct TxDetail {
    enum {Count=256};
    enum : uint64_t {Header=1,Command=2,Doorbell=4,Response=8,Retired=16,
        Returned=32,ResponseTruncated=64,UnknownTerminal=128};
    struct Entry {uint64_t v[18];uint8_t header[144],command[64],response[64];};
    // magic/schema/size/capacity/records/update-serial/overwritten/overwritten-
    // unretired/missing-update/unknown-header/truncated-response/orphan-response.
    uint64_t h[16]={224,1,128+Count*sizeof(Entry),Count};
    Entry entries[Count]={};
    static size_t headerBytes(const uint8_t *p,size_t n) {
        if(!p || n<24 || n>2304)return 0;
        static const uint8_t llc[10]={0xaa,0xaa,3,0,0x17,0xf2,8,0,3,4};
        if(n<80 || p[0]!=8 || p[1] || memcmp(p+24,llc,10) ||
           (p[36]&15) || p[38]!=0x86 || p[39]!=0xdd || (p[40]>>4)!=6)return 24;
        const size_t payload=(size_t(p[44])<<8)|p[45];
        if(payload>n-80)return 24;
        if(p[46]==6 && payload>=20 && n>=100) {
            const size_t tcp=(p[92]>>4)*4;
            if(tcp>=20 && tcp<=60 && tcp<=payload && tcp<=n-80)return 80+tcp;
        }
        if(p[46]==17 && payload>=8 && n>=88)return 88;
        return 80; // base IPv6 only, never guess past an extension/fragment
    }
    Entry *find(uint64_t ticket,uint32_t generation,uint32_t epoch) {
        if(!ticket){++h[8];++h[5];return nullptr;}
        auto &e=entries[(ticket-1)%Count];
        if(e.v[0]!=ticket || e.v[3]!=((uint64_t(generation)<<32)|epoch)) {++h[8];++h[5];return nullptr;}
        return &e;
    }
    void changed(Entry &e){e.v[2]=++h[5];}
    void begin(uint64_t ticket,uint32_t generation,uint32_t epoch,uint64_t now,
               unsigned q,unsigned index,const uint8_t *frame,size_t bytes,
               uint64_t window,uint64_t durationTu,uint64_t windowStartNs=0,uint32_t lastGp2=0) {
        if(!ticket)return;
        auto &e=entries[(ticket-1)%Count];
        if(e.v[0]) {++h[6];if(!(e.v[12]&Retired))++h[7];}
        memset(&e,0,sizeof(e));e.v[0]=ticket;e.v[1]=++h[4];
        e.v[3]=(uint64_t(generation)<<32)|epoch;e.v[4]=now;
        e.v[11]=(uint64_t(q)<<32)|index;e.v[14]=window;e.v[15]=durationTu;
        e.v[16]=windowStartNs;e.v[17]=lastGp2;
        const size_t kept=headerBytes(frame,bytes);
        e.v[13]=(bytes&0xffff)|(uint64_t(kept)<<16);
        if(kept)memcpy(e.header,frame,kept);
        if(kept>=80)e.v[12]|=Header;else ++h[9];
        changed(e);
    }
    void command(uint64_t ticket,uint32_t generation,uint32_t epoch,uint64_t now,
                 const void *data,size_t bytes) {
        auto *e=find(ticket,generation,epoch);if(!e)return;
        // Exact reviewed TX_CMD header + gen3 fields + 24-byte MAC header.
        if(!data || bytes!=56){++h[12];++h[5];return;}
        if(e->v[12]&Command)++h[14];
        memcpy(e->command,data,bytes);e->v[5]=now;e->v[12]|=Command;
        e->v[13]|=uint64_t(bytes)<<24;changed(*e);
    }
    void doorbell(uint64_t ticket,uint32_t generation,uint32_t epoch,uint64_t now) {
        auto *e=find(ticket,generation,epoch);if(!e)return;
        e->v[6]=now;e->v[12]|=Doorbell;changed(*e);
    }
    void returned(uint64_t ticket,uint32_t generation,uint32_t epoch,uint64_t now,int result) {
        auto *e=find(ticket,generation,epoch);if(!e)return;
        e->v[9]=now;e->v[10]=uint64_t(int64_t(result));e->v[12]|=Returned;changed(*e);
    }
    void response(uint64_t ticket,uint32_t generation,uint32_t epoch,uint64_t now,
                  const void *data,size_t bytes) {
        auto *e=find(ticket,generation,epoch);if(!e)return;
        // Include the RX envelope, actual response, and SCD SSN. No payload.
        if(!data || bytes<56){++h[12];++h[5];return;}
        if(e->v[12]&Response)++h[13];
        const size_t kept=bytes<sizeof(e->response)?bytes:sizeof(e->response);
        memset(e->response,0,sizeof(e->response));memcpy(e->response,data,kept);
        e->v[7]=now;e->v[12]=(e->v[12]&~ResponseTruncated)|Response;
        e->v[13]=(e->v[13]&0xffffffffULL)|((uint64_t(bytes)&0xffff)<<32)|(uint64_t(kept)<<48);
        if(kept<bytes){e->v[12]|=ResponseTruncated;++h[10];}changed(*e);
    }
    void retired(uint64_t ticket,uint32_t generation,uint32_t epoch,uint64_t now,int status) {
        auto *e=find(ticket,generation,epoch);if(!e)return;
        e->v[8]=now;e->v[12]|=Retired;if(status<0)e->v[12]|=UnknownTerminal;changed(*e);
    }
    void orphanResponse(){++h[11];++h[5];}
};
static_assert(sizeof(TxDetail::Entry)==416,"R28 diagnostic record extent");
static_assert(sizeof(TxDetail)==106624,"R28 diagnostic property extent");
}
#endif
