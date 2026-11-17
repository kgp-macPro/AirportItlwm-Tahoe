/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP.
 * Private receive-domain isolation. No sequence/Retry decisions or radio policy.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace be_awdl {
struct InfraIsolation {
    enum ManagementScope { Infrastructure = 0, ScanOnly = 1, Foreign = 2 };
    // A private receiver can deliver management/control frames which are
    // not owned by the associated station. net80211 STA lookup deliberately
    // returns ic_bss, even for a foreign transmitter; its management handlers
    // therefore require the driver to qualify the receive domain first.
    // Scan beacons remain useful without becoming AP state/quality feedback.
    static ManagementScope managementScope(const uint8_t *f, size_t n,
                                           const uint8_t *own, const uint8_t *ap)
    {
        if (!f || !n || !own || !ap) return Foreign;
        const unsigned type=f[0]&0x0c, subtype=f[0]&0xf0;
        if (type!=0 && type!=4) return Infrastructure;
        if (n<(type==4?16U:24U)) return Foreign;
        const bool receiver=(f[4]&1) || same(f+4,own);
        const bool transmitter=same(f+10,ap);
        if (type==4) return same(f+4,own) && transmitter?Infrastructure:Foreign;
        if (f[1]&3) return Foreign; // management frames are NoDS
        const bool bssid=same(f+16,ap);
        if (transmitter && bssid) return receiver?Infrastructure:Foreign;
        // Contradictory AP identities are not a new scan BSS, and must not
        // resolve back to the associated AP's node by transmitter address.
        if (transmitter || bssid) return Foreign;
        // Foreign protected management must never select STA0's crypto key.
        // This legacy scan path negotiates no protected-beacon capability.
        return !(f[1]&0x40) && (subtype==0x80 || subtype==0x50)?ScanOnly:Foreign;
    }
    // Legacy-group missed-beacon v3 carries a MAC ID, v4 a firmware link
    // ID. Their five-u32 envelope is identical. Never apply another role's
    // loss notification to the associated AP or guess a future layout.
    static unsigned beaconDomain(unsigned version, size_t bytes, uint32_t id,
                                 uint32_t infraMac, uint32_t infraLink)
    {
        if(bytes<20 || (version!=3 && version!=4)) return 2;
        return id==(version==4?infraLink:infraMac)?0:1;
    }
    // A private 20-MHz receiver must not capture the associated station's
    // data through PROMISC. Include the AP's full negotiated bandwidth.
    // Unknown/inconsistent chandefs are restrictive, never permission.
    static bool separated(uint16_t primary, uint16_t center, unsigned width,
                          uint16_t receiver)
    {
        if (!primary || !receiver) return false;
        if ((primary < 3000) != (receiver < 3000)) return true;
        if (width != 20 && width != 40 && width != 80 && width != 160) return false;
        if (width == 20 && !center) center = primary;
        if (!center) return false;
        const unsigned offset = primary > center ? primary-center : center-primary;
        if (offset > width/2-10 || (width > 20 && (offset % 20) != 10)) return false;
        const unsigned distance = receiver > center ? receiver-center : center-receiver;
        return distance >= width/2+10;
    }
    static bool same(const uint8_t *a, const uint8_t *b)
    {
        for (unsigned i=0;i<6;++i) if (a[i]!=b[i]) return false;
        return true;
    }
    // Mirror the station's existing net80211 admission BEFORE its per-BSS
    // decrypt/duplicate/reorder state is touched. Recognized AWDL is already
    // copied to its native domain by the caller. Management remains available
    // to scanning; foreign management must not write this BSS's history.
    static bool foreignData(const uint8_t *f, size_t n, const uint8_t *own,
                            const uint8_t *bssid)
    {
        if (!f || n<24 || !own || !bssid || (f[0]&0x0c)!=8) return false;
        return (f[1]&3)!=2 || !same(f+10,bssid) ||
            (!(f[4]&1) && !same(f+4,own));
    }
    static bool foreignManagement(const uint8_t *f, size_t n, const uint8_t *bssid)
    {
        return f && n>=24 && bssid && (f[0]&0x0c)==0 && !same(f+10,bssid);
    }
};
}
