/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP. Bounded diagnostic CPU copies; no TX ownership.
 */
#ifndef KGP_BE_AWDL_TX_ADMISSION_HPP
#define KGP_BE_AWDL_TX_ADMISSION_HPP
#include "BeAwdlTxDetail.hpp"
namespace be_awdl {
struct TxAdmissionEvidence {
    enum {Count=32};
    struct Entry {uint64_t v[20];uint8_t header[144];};
    uint64_t h[16]={233,1,128+Count*sizeof(Entry)+256,Count};
    Entry entries[Count]={};
    uint64_t waitKey[2][16]={};
    void record(uint64_t now,const uint64_t (&state)[16],const uint8_t *frame,size_t bytes) {
        const unsigned stage=unsigned(state[2]);if(stage>=1 && stage<=6)++h[7+stage];
        // Coalesce identical no-packet waits; preserve their observation count.
        if(!bytes && (stage==1 || stage==6)) {
            const unsigned k=stage==6;h[14+k]=now;
            if(!memcmp(waitKey[k],state,sizeof(state))){++h[6];return;}
            memcpy(waitKey[k],state,sizeof(state));
        }
        const uint64_t serial=++h[4];if(serial>Count)++h[5];
        auto &e=entries[(serial-1)%Count];memset(&e,0,sizeof(e));
        e.v[0]=serial;e.v[1]=now;memcpy(e.v+2,state,sizeof(state));
        const size_t kept=TxDetail::headerBytes(frame,bytes);e.v[18]=bytes;e.v[19]=kept;
        if(kept)memcpy(e.header,frame,kept);
    }
};
static_assert(sizeof(TxAdmissionEvidence::Entry)==304,"admission diagnostic entry");
static_assert(sizeof(TxAdmissionEvidence)==10112,"admission diagnostic property");
}
#endif
