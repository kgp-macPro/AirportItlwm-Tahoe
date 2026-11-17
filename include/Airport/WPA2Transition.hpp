/*
 * Adapted from laobamac/itlwm AirportItlwm/WCL/WCL.hpp,
 * commit 7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e.
 * Copyright (c) 2026 laobamac and contributors; original notices retained.
 * Distributed under this project's GNU GPL version 2 or later (see LICENSE).
 * R27 controlled comparison: operate only on a private scan-result copy.
 */
#ifndef AIRPORT_WPA2_TRANSITION_HPP
#define AIRPORT_WPA2_TRANSITION_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace AirportWPA2Transition {
inline uint16_t read16(const uint8_t *p) {
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}

inline bool validIEs(const uint8_t *data, size_t length) {
    if (!data || length > 2048)
        return false;
    while (length) {
        if (length < 2 || size_t(data[1]) + 2 > length)
            return false;
        size_t next = size_t(data[1]) + 2;
        data += next;
        length -= next;
    }
    return true;
}

inline size_t selectWPA2TransitionMode(uint8_t *ies, size_t length) {
    if (!validIEs(ies, length))
        return length;
    bool converted = false;
    for (size_t offset = 0; offset < length; ) {
        uint8_t *ie = ies + offset;
        size_t size = size_t(ie[1]) + 2;
        if (offset + size > length)
            break;
        if (ie[0] != 48 || size < 10 || read16(ie + 2) != 1) {
            offset += size;
            continue;
        }
        size_t pairwiseCount = read16(ie + 8);
        if (!pairwiseCount || pairwiseCount > (size - 10) / 4) {
            offset += size;
            continue;
        }
        size_t countOffset = 10 + 4 * pairwiseCount;
        if (size - countOffset < 2) {
            offset += size;
            continue;
        }
        size_t count = read16(ie + countOffset);
        size_t suitesOffset = countOffset + 2;
        if (count < 2 || count > (size - suitesOffset) / 4) {
            offset += size;
            continue;
        }
        size_t tailOffset = suitesOffset + 4 * count;
        // PSK/SAE transition mode advertises PMF capable, but not required.
        if (size - tailOffset < 2 || (read16(ie + tailOffset) & 0xc0) != 0x80) {
            offset += size;
            continue;
        }
        bool psk = false, sae = false, other = false;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t *suite = ie + suitesOffset + 4 * i;
            if (suite[0] != 0 || suite[1] != 0x0f || suite[2] != 0xac)
                other = true;
            else if (suite[3] == 2)
                psk = true;
            else if (suite[3] == 6)
                continue; // PSK-SHA256 is recognized, but ordinary PSK is still required.
            else if (suite[3] == 8 || suite[3] == 24)
                sae = true;
            else
                other = true;
        }
        if (!psk || !sae || other) {
            offset += size;
            continue;
        }
        const uint8_t pskSuite[] = {0, 0x0f, 0xac, 2};
        memcpy(ie + suitesOffset, pskSuite, sizeof(pskSuite));
        ie[countOffset] = 1;
        ie[countOffset + 1] = 0;
        size_t removed = 4 * (count - 1);
        // Keep RSN capabilities, PMKIDs, group-management cipher and all
        // following information elements byte-for-byte intact.
        memmove(ie + suitesOffset + 4, ie + tailOffset,
                length - offset - tailOffset);
        ie[1] -= removed;
        length -= removed;
        offset += size - removed;
        converted = true;
    }
    if (converted) {
        for (size_t offset = 0; offset < length; ) {
            uint8_t *ie = ies + offset;
            size_t size = size_t(ie[1]) + 2;
            if (offset + size > length)
                break;
            if (ie[0] == 244) {
                memmove(ie, ie + size, length - offset - size);
                length -= size;
                continue;
            }
            offset += size;
        }
    }
    return length;
}
}
#endif
