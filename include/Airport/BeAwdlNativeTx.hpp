/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP
 * Owned, bounded native Action admission. No mbuf/DMA/native peer ownership.
 */
#ifndef KGP_BE_AWDL_NATIVE_TX_HPP
#define KGP_BE_AWDL_NATIVE_TX_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace be_awdl {
class NativeActions {
public:
    enum { Capacity=4, MaximumBody=1840 };
    struct Key { uint64_t generation, epoch, publication, control, templateSerial, serviceSerial; };
    struct Item { Key key; uint64_t queuedNs, ticket; size_t length;
        uint8_t destination[6], bssid[6]; bool oob, needsMiTiming;
        uint8_t body[MaximumBody]; };
    unsigned pending() const { return count; }
    bool hasImmediate() const {for(unsigned i=0;i<count;++i)if(items[(head+i)%Capacity].oob)return true;return false;}
    // Exact Legacy buildMasterIndicationTemplate0x406b4 returns TLVs,
    // while native bpf/outputAction callbacks may already include the16-byte
    // vendor header. These are distinct valid input contracts.
    static size_t miBody(uint8_t *out,size_t cap,const uint8_t *p,size_t n,
                         bool *needsTiming=nullptr) {
        if(!out || !p || !n || n>1200)return 0;
        const bool complete=n>=16 && p[0]==127 && p[1]==0 && p[2]==0x17 && p[3]==0xf2 && p[4]==8;
        const size_t start=complete?16:0;
        if (start==n) return 0; // invalid empty MI must not overwrite the cached body
        if(n-start<3 || cap<n+(complete?0:16))return 0;
        for(size_t at=start;at<n;) {
            if(n-at<3)return 0;
            const size_t len=size_t(p[at+1])|(size_t(p[at+2])<<8);
            if(len>n-at-3)return 0;at+=3+len;
        }
        if(needsTiming)*needsTiming=!complete;
        if(complete){memcpy(out,p,n);return n;}
        // Native buildMasterIndicationTemplate supplies TLVs, with no subtype.
        // MIF is subtype 3 in OWL and ForbiddenItlwm.
        memset(out,0,16);out[0]=127;out[2]=0x17;out[3]=0xf2;out[4]=8;out[5]=0x10;
        out[6]=3;
        memcpy(out+16,p,n);return n+16;
    }
    // Native service Actions must not starve the existing periodic Sync path.
    // Periodic producers remain <=5 attempts/s. A bounded actual149 immediate
    // request can run at the next admitted service; one frame per service.
    static unsigned select(uint64_t now,uint64_t lastSync,uint64_t lastNative,unsigned pending,bool immediate=false,bool syncAllowed=true) {
        if (syncAllowed && (!lastSync || (now>=lastSync && now-lastSync>=200000000ULL))) return 1;
        if (pending && (immediate || !lastNative || (now>=lastNative && now-lastNative>=200000000ULL))) return 2;
        return 0;
    }
    static bool same(const Key &a,const Key &b) {
        return a.generation==b.generation && a.epoch==b.epoch &&
            a.publication==b.publication && a.control==b.control &&
            a.templateSerial==b.templateSerial && a.serviceSerial==b.serviceSerial;
    }
    void discardStale(const Key &key,uint64_t now,uint64_t &stale) {
        while(count) {
            const Item &v=items[head];
            if (same(v.key,key) && now>=v.queuedNs && now-v.queuedNs<=200000000ULL) break;
            head=(head+1)%Capacity;--count;++stale;
        }
    }
    // Both producers and consumer run on the controller workloop. A copied
    // native body is neither a queued firmware packet nor a TX completion.
    bool push(const void *body,size_t length,const Key &key,uint64_t now,uint64_t ticket=0,
              const uint8_t *destination=nullptr,const uint8_t *bssid=nullptr) {
        if (!body || length<16 || length>MaximumBody || count==Capacity) return false;
        if(destination && (!bssid || ((destination[0]&1) && memcmp(destination,"\xff\xff\xff\xff\xff\xff",6)) ||
           !memcmp(destination,"\0\0\0\0\0\0",6)))return false;
        Item &v=items[(head+count)%Capacity]; v.key=key;v.queuedNs=now;v.ticket=ticket;v.length=length;
        v.oob=destination!=nullptr;v.needsMiTiming=false;
        memset(v.destination,255,6);memset(v.bssid,0,6);
        if(destination){memcpy(v.destination,destination,6);memcpy(v.bssid,bssid,6);}
        memcpy(v.body,body,length);++count;return true;
    }
    bool pop(Item &out,const Key &key,uint64_t now,uint64_t &stale) {
        while (count) {
            Item &v=items[head];head=(head+1)%Capacity;--count;
            // No replay into a new VIF/plan/radio lifetime. Bounded freshness
            // is host admission only, not an on-air timing assertion.
            if (!same(v.key,key) || now<v.queuedNs || now-v.queuedNs>200000000ULL) {
                ++stale;continue;
            }
            out=v;return true;
        }
        return false;
    }
    unsigned clear() { unsigned n=count;head=count=0;return n; }
    static size_t frame(uint8_t *out,size_t capacity,const Item &v,
                        const uint8_t source[6],const uint8_t bssid[6],
                        uint16_t sequence,uint32_t gp2) {
        if (!out || !source || !bssid || v.length<16 || v.length>MaximumBody ||
            capacity<24+v.length || (source[0]&1) ||
            !memcmp(source,"\0\0\0\0\0\0",6)) return 0;
        const uint8_t *a=v.body;
        if (a[0]!=127 || a[1]!=0 || a[2]!=0x17 || a[3]!=0xf2 || a[4]!=8) return 0;
        if(v.oob && memcmp(v.bssid,bssid,6))return 0;
        memset(out,0,24);out[0]=0xd0;memcpy(out+4,v.destination,6);
        memcpy(out+10,source,6);memcpy(out+16,bssid,6);
        out[22]=uint8_t(sequence);out[23]=uint8_t(sequence>>8);
        memcpy(out+24,v.body,v.length);
        // Existing qualified GP2 estimator, sampled at driver submission.
        // Neither this value nor enqueue success is an on-air measurement.
        for(unsigned i=0;i<4;++i)out[32+i]=out[36+i]=uint8_t(gp2>>(8*i));
        return 24+v.length;
    }
private:
    Item items[Capacity] = {}; unsigned head=0,count=0;
};
// Native UMI status is not a queue-enqueue result or scheduler SSN. This
// bounded CPU mailbox carries only an identified, actual single-TX response.
// It is produced/consumed under the existing main workloop gate.
class ActionResults {
public:
    struct Result {uint64_t ticket;uint32_t status,generation,epoch;};
    bool push(const Result &v){if(count==8)return false;items[(head+count)%8]=v;++count;return true;}
    bool pop(Result &v){if(!count)return false;v=items[head];head=(head+1)%8;--count;return true;}
    void clear(){head=count=0;}
private:
    Result items[8]={};unsigned head=0,count=0;
};
}
#endif
