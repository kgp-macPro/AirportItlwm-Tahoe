/* SPDX-License-Identifier: GPL-2.0-or-later
 * Exact Ventura IO80211FamilyLegacy AWDL CHANNELS_INFO provider contract.
 * REA ac2dc15b: configureValidAWDLChannels allocates/zeros 0x1a9c bytes;
 * isValidChannel reads count+8, channels+0x32a and the arrays below (400).
 * Do not replace the unrelated historical 128-entry public source type.
 */
#ifndef KGP_LEGACY_AWDL_CHANNELS_HPP
#define KGP_LEGACY_AWDL_CHANNELS_HPP
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace be_awdl {
struct LegacyChannelInfo {
    uint8_t bytes[0x1a9c];
    static constexpr unsigned Capacity=400;
    void clear() { memset(bytes,0,sizeof(bytes));uint32_t v=1;memcpy(bytes,&v,4); }
    uint16_t count() const {uint16_t n;memcpy(&n,bytes+8,2);return n;}
    bool add(uint8_t channel,bool dfs,bool passive,bool mhz40,bool mhz80) {
        const unsigned n=count();if(!channel || n>=Capacity)return false;
        bytes[0x32a+n]=channel;
        bytes[0x64a+n]=dfs;bytes[0x7da+n]=passive;
        bytes[0x96a+n]=mhz40;bytes[0xafa+n]=mhz80;
        const uint16_t next=uint16_t(n+1);memcpy(bytes+8,&next,2);return true;
    }
};
static_assert(sizeof(LegacyChannelInfo)==0x1a9c,"exact native CHANNELS_INFO extent");
}
#endif
