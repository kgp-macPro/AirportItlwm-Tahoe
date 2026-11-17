/*
 * Copyright (C) 2026 Glen Muthoka
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Protocol constants and wire layout are independently described here so the
 * transport can be tested outside the kernel.  No OWL source is copied.
 */
#ifndef AwdlTransport_hpp
#define AwdlTransport_hpp

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace itlwm_awdl {

static const size_t kMacLength = 6;
static const size_t kDot11HeaderLength = 24;
static const size_t kActionHeaderLength = 16;
static const size_t kMaximumTemplateLength = 1824;
static const size_t kMaximumDynamicLength = 512;
static const size_t kMaximumPeers = 32;
static const size_t kMaximumChannelSequence = 128;
// Service Response (tag 2) and hostname (tag 16) TLVs copied from IO80211's
// MI frame. With AirDrop published the Mac advertises ~6 services plus its
// host name (~450 bytes); 384 silently dropped the _airdrop SRV record.
static const size_t kMaximumServiceTlvLength = 1024;
// Measured GP2 drift against the iPhone master stayed under 1 ms over 65 s.
static const uint32_t kMaximumSyncExtrapolationUs = 30000000;
// On air, queued sync frames left 0.6-3 ms after the window opened.
static const uint32_t kSyncTxLatencyUs = 1024;
static const uint32_t kMaximumMasterTxDelayUs = 8192;
// OWL AWDL_ELECTION_METRIC_INIT; a real follower iPad/Mac advertised 60 too.
static const uint32_t kFollowerSelfMetric = 60;

static const uint8_t kDefaultBssid[kMacLength] =
    { 0x00, 0x25, 0x00, 0xff, 0x94, 0x73 };

struct PeerObservation {
    bool valid;
    bool newPeer;
    uint8_t address[kMacLength];
    uint8_t subtype;
    uint8_t channel;
    int8_t rssi;
    bool followUpdated;
};

struct FollowState {
    bool valid;
    uint8_t masterAddress[kMacLength];
    uint8_t syncSourceAddress[kMacLength]; // actual parent that supplied this timing
    uint32_t masterMetric;
    uint32_t masterCounter;
    uint32_t selfMetric;
    uint32_t selfCounter;
    uint32_t distanceToMaster;
    uint8_t nextAwChannel;
    uint8_t masterChannel;
    uint8_t guardTime;
    uint16_t txCounter;
    uint16_t awPeriod;
    uint16_t actionFramePeriod;
    uint16_t extensionLength;
    uint16_t awLength;
    uint16_t awSequence;
    uint32_t rxGp2;
    uint32_t rxGp2Delta;
    bool rxGp2DeltaValid;
    bool timingValid;
    uint32_t eawPeriodUs;
    uint32_t nextEawGp2;
    uint64_t rxHostTimeNs;
    uint8_t presenceMode;
    uint8_t channelEncoding;
    uint8_t channelDuplicateCount;
    uint8_t channelStepCount;
    uint16_t channelFill;
    uint16_t channelSequence[kMaximumChannelSequence];
    size_t channelSequenceLength;
};

struct PeerWindow {
    uint64_t observedNs;
    uint32_t eawStartGp2;
    uint32_t awUs;
    uint16_t eawNumber;
    uint16_t channelSlots;
};

struct Peer {
    bool active;
    uint8_t address[kMacLength];
    uint8_t channel;
    int8_t rssi;
    uint64_t lastSeen;
    PeerWindow window;
};

class TransportState {
public:
    TransportState() { reset(); }

    void reset()
    {
        memset(this, 0, sizeof(*this));
        memcpy(bssid_, kDefaultBssid, sizeof(bssid_));
    }

    // Firmware GP2 changes lifetime across reset. Do not follow peers/timing
    // from a previous radio epoch, while retaining owned native control data.
    void resetRadioHistory()
    {
        memset(peers_, 0, sizeof(peers_));
        memset(&follow_, 0, sizeof(follow_));
        memset(nativeAnchors_, 0, sizeof(nativeAnchors_));
    }

    static bool validTemplate(const void *payload,size_t length)
    {
        return payload && length && length<=kMaximumTemplateLength &&
            validateTlvs(static_cast<const uint8_t *>(payload),length);
    }
    bool setTemplate(const void *payload, size_t length, bool normalizeHt = true)
    {
        if (!validTemplate(payload,length))
            return false;
        memcpy(template_, payload, length);
        templateLength_ = length;
        memset(selfAddress_, 0, sizeof(selfAddress_));
        const uint8_t *bytes = static_cast<const uint8_t *>(payload);
        size_t offset = 0;
        while (offset + 3 <= length) {
            const uint8_t type = bytes[offset];
            const size_t tlvLength = static_cast<size_t>(bytes[offset + 1]) |
                (static_cast<size_t>(bytes[offset + 2]) << 8);
            offset += 3;
            if (tlvLength > length - offset)
                break;
            // Election Parameters v2 starts with the advertised AWDL master
            // address. When this Mac is master, Apple supplies awdl0's
            // randomized address here even though the controller only exposes
            // the infrastructure MAC through getHardwareAddress().
            if (type == 24 && tlvLength >= kMacLength &&
                !isMulticast(bytes + offset) &&
                !isZeroAddress(bytes + offset)) {
                memcpy(selfAddress_, bytes + offset, kMacLength);
                break;
            }
            offset += tlvLength;
        }
        // BE200 integration passes false: retain native capabilities verbatim.
        if (normalizeHt) normalizeHtCapabilities();
        return true;
    }

    static bool nextEawApplyTime(const FollowState &follow,
                                 uint64_t nowNanoseconds,
                                 uint32_t minimumLeadUs,
                                 uint32_t *currentGp2,
                                 uint32_t *applyTime)
    {
        if (!follow.valid || !follow.timingValid || currentGp2 == NULL ||
            applyTime == NULL || follow.eawPeriodUs == 0 ||
            nowNanoseconds < follow.rxHostTimeNs)
            return false;
        const uint64_t ageUs =
            (nowNanoseconds - follow.rxHostTimeNs) / 1000ULL;
        // A stale anchor can be refreshed by the next master MIF. Refuse to
        // extrapolate it far enough for 32-bit GP2 ordering to become unclear.
        if (ageUs > 2000000ULL)
            return false;
        const uint32_t nowGp2 = follow.rxGp2 + static_cast<uint32_t>(ageUs);
        uint32_t target = follow.nextEawGp2;
        const int32_t lead = static_cast<int32_t>(target - nowGp2);
        if (lead <= static_cast<int32_t>(minimumLeadUs)) {
            const uint32_t pastBy = nowGp2 - target + minimumLeadUs;
            const uint32_t periods = pastBy / follow.eawPeriodUs + 1U;
            target += periods * follow.eawPeriodUs;
        }
        *currentGp2 = nowGp2;
        *applyTime = target;
        return true;
    }

    static int32_t phaseError(uint32_t desired, uint32_t scheduled,
                              uint32_t periodUs)
    {
        if (periodUs == 0)
            return 0;
        const uint32_t delta = (desired - scheduled) % periodUs;
        if (delta > periodUs / 2U)
            return static_cast<int32_t>(delta) -
                   static_cast<int32_t>(periodUs);
        return static_cast<int32_t>(delta);
    }

    // Sync Parameters must describe the master's epoch, not this host's clock:
    // receivers compare tx_down_counter and the AW counter against their own
    // master schedule (OWL awdl_sync_error_tu). As in OWL's
    // awdl_sync_update_last(), the anchor EAW ends at nextEawGp2 and began with
    // the master's AW counter rounded down to a whole EAW.
    static bool masterSyncAt(const FollowState &follow, uint32_t gp2,
                             uint16_t *downCounterTu, uint16_t *awSequence)
    {
        if (!follow.valid || !follow.timingValid || downCounterTu == NULL ||
            awSequence == NULL || follow.eawPeriodUs == 0 ||
            follow.awPeriod == 0 || follow.presenceMode == 0)
            return false;
        const uint32_t eawStart = follow.nextEawGp2 - follow.eawPeriodUs;
        const uint32_t since = gp2 - eawStart;
        // Wrap-safe: an instant before the anchor EAW appears enormous here.
        if (since > kMaximumSyncExtrapolationUs)
            return false;
        const uint32_t eaws = since / follow.eawPeriodUs;
        const uint32_t within = since % follow.eawPeriodUs;
        const uint32_t base = follow.awSequence -
            (follow.awSequence % follow.presenceMode);
        *awSequence = static_cast<uint16_t>(
            base + eaws * follow.presenceMode +
            within / (static_cast<uint32_t>(follow.awPeriod) * 1024U));
        *downCounterTu = static_cast<uint16_t>(
            follow.eawPeriodUs / 1024U - within / 1024U);
        return true;
    }

    // OWL sync.c and Legacy parseAwdlSyncParams: the down-counter reaches
    // the next EAW, not merely the next base AW. Use the same local GP2
    // origin as our native WindowStart sequence. No defaults or clock writes.
    static bool localSyncAt(uint32_t gp2, uint32_t period, uint32_t length,
                            uint32_t presence, uint16_t &counter,
                            uint16_t &sequence, uint16_t &remaining)
    {
        const uint64_t eaw=uint64_t(period)*presence;
        if(!period || !length || !presence || eaw>65535 || length>eaw)
            return false;
        const uint32_t tu=gp2/1024U, elapsed=tu%uint32_t(eaw);
        counter=uint16_t(eaw-elapsed); sequence=uint16_t(tu/period);
        remaining=uint16_t(length>elapsed?length-elapsed:0);
        return true;
    }

    // Channel-sequence entries: legacy encoding is the channel itself; the
    // two-byte form is channel | flags << 8 with flags bits 0-1 the control
    // position (1 lower, 2 upper, 3 primary) and bits 2-3 the bandwidth
    // (3 = 40 MHz). The iPhone's "46/0x1e" is therefore control channel 48.
    static uint16_t controlChannel(uint8_t encoding, uint16_t entry)
    {
        if (encoding == 0)
            return entry & 0xff;
        const uint16_t channel = entry & 0xff;
        const uint8_t flags = static_cast<uint8_t>(entry >> 8);
        if (channel == 0 || ((flags >> 2) & 3) != 3)
            return channel;
        if ((flags & 3) == 1)
            return static_cast<uint16_t>(channel - 2);
        if ((flags & 3) == 2)
            return static_cast<uint16_t>(channel + 2);
        return channel;
    }

    // Where the followed master listens during the EAW containing gp2:
    // entry (AW sequence / presence) mod length of its channel sequence
    // (checked against 158/158 iPhone MIFs' "next AW channel").
    static bool masterSlotAt(const FollowState &follow, uint32_t gp2,
                             uint16_t *channel, uint32_t *eawStartGp2)
    {
        uint16_t down = 0, sequence = 0;
        if (channel == NULL || eawStartGp2 == NULL ||
            follow.channelSequenceLength == 0 ||
            !masterSyncAt(follow, gp2, &down, &sequence))
            return false;
        const size_t index = (sequence / follow.presenceMode) %
            follow.channelSequenceLength;
        *channel = controlChannel(follow.channelEncoding,
                                  follow.channelSequence[index]);
        const uint32_t since = gp2 - (follow.nextEawGp2 - follow.eawPeriodUs);
        *eawStartGp2 = gp2 - since % follow.eawPeriodUs;
        return true;
    }

    // The 5 GHz control channel the master uses most in its sequence; ties
    // keep `current`. 0 when the sequence has no 5 GHz entry.
    static uint16_t dominantChannel(const FollowState &follow, uint16_t current)
    {
        uint16_t channels[kMaximumChannelSequence];
        uint32_t counts[kMaximumChannelSequence];
        size_t distinct = 0;
        for (size_t i = 0; i < follow.channelSequenceLength; ++i) {
            const uint16_t channel = controlChannel(follow.channelEncoding,
                                                    follow.channelSequence[i]);
            if (channel < 32)
                continue;
            size_t j = 0;
            while (j < distinct && channels[j] != channel)
                ++j;
            if (j == distinct) {
                channels[distinct] = channel;
                counts[distinct++] = 0;
            }
            ++counts[j];
        }
        uint16_t best = 0;
        uint32_t bestCount = 0;
        for (size_t j = 0; j < distinct; ++j) {
            if (counts[j] > bestCount ||
                (counts[j] == bestCount && channels[j] == current)) {
                best = channels[j];
                bestCount = counts[j];
            }
        }
        return best;
    }

    // Start of the next EAW (after the one containing gp2, within 32) in
    // which the master listens on the given channel (0: any 5 GHz channel).
    static bool nextMasterSlot(const FollowState &follow, uint32_t gp2,
                               uint16_t wanted, uint32_t *startGp2)
    {
        uint16_t channel = 0;
        uint32_t eawStart = 0;
        if (startGp2 == NULL || !masterSlotAt(follow, gp2, &channel, &eawStart))
            return false;
        for (uint32_t k = 1; k <= 32; ++k) {
            const uint32_t candidate = eawStart + k * follow.eawPeriodUs;
            uint32_t ignored = 0;
            if (masterSlotAt(follow, candidate + 1, &channel, &ignored) &&
                (wanted == 0 ? channel >= 32 : channel == wanted)) {
                *startGp2 = candidate;
                return true;
            }
        }
        return false;
    }

    // The sync frame waits in the AWDL queue until the firmware services it.
    // With an EAW-aligned time event that happens at the next window opening
    // unless one is open now; windowUs == eawPeriodUs means always serviced.
    static bool predictTxGp2(const FollowState &follow,
                             uint64_t nowNanoseconds, uint32_t windowUs,
                             uint32_t *txGp2)
    {
        if (!follow.valid || !follow.timingValid || txGp2 == NULL ||
            follow.eawPeriodUs == 0 || nowNanoseconds < follow.rxHostTimeNs)
            return false;
        const uint64_t ageUs =
            (nowNanoseconds - follow.rxHostTimeNs) / 1000ULL;
        if (ageUs > kMaximumSyncExtrapolationUs)
            return false;
        const uint32_t nowGp2 = follow.rxGp2 + static_cast<uint32_t>(ageUs);
        const uint32_t eawStart = follow.nextEawGp2 - follow.eawPeriodUs;
        const uint32_t within = (nowGp2 - eawStart) % follow.eawPeriodUs;
        if (within + kSyncTxLatencyUs < windowUs)
            *txGp2 = nowGp2 + kSyncTxLatencyUs;
        else
            *txGp2 = nowGp2 - within + follow.eawPeriodUs + kSyncTxLatencyUs;
        return true;
    }

    // Apple's IO80211 builds the AWDL sync template from the capabilities the
    // interface reports. On a real Mac the Broadcom driver reports a full HT
    // capability set, so the template's HT Capabilities tag (7) carries the
    // canonical 9-byte body. This Intel fork under-reports HT capabilities, so
    // Apple emits a truncated 5-byte tag instead. A strict AWDL receiver (a
    // real iPhone, and tshark) walks the MIF TLV chain by declared length and
    // reads the fixed HT-capability fields past the short tag's end, which
    // desyncs the rest of the chain (Sync Parameters, Channel Sequence) and
    // makes the peer reject the frame. Pad the tag to the canonical 9 bytes,
    // preserving Apple's HT Capabilities Info and A-MPDU fields and appending a
    // standard "Rx MCS 0-15 supported" trailer.
    void normalizeHtCapabilities()
    {
        static const size_t kCanonicalHtBody = 9;
        // Trailer bytes that follow HT Cap Info (2) + A-MPDU Params (1): the
        // Rx supported-MCS bitmap (0xffff = MCS 0-15) then two reserved bytes,
        // matching what a real Apple device advertises.
        static const uint8_t kHtTrailer[4] = { 0xff, 0xff, 0x00, 0x00 };
        size_t offset = 0;
        while (offset + 3 <= templateLength_) {
            const uint8_t type = template_[offset];
            const size_t tlvLength = static_cast<size_t>(template_[offset + 1]) |
                (static_cast<size_t>(template_[offset + 2]) << 8);
            if (tlvLength > templateLength_ - offset - 3)
                return; // malformed template; leave it untouched
            if (type == 7 && tlvLength < kCanonicalHtBody) {
                const size_t deficit = kCanonicalHtBody - tlvLength;
                if (templateLength_ + deficit > sizeof(template_))
                    return; // no room to grow; leave it untouched
                uint8_t *body = template_ + offset + 3;
                const size_t tailStart = offset + 3 + tlvLength;
                memmove(body + kCanonicalHtBody, body + tlvLength,
                        templateLength_ - tailStart);
                for (size_t i = 0; i < deficit; ++i)
                    body[tlvLength + i] = kHtTrailer[i < sizeof(kHtTrailer) ?
                                                     i : sizeof(kHtTrailer) - 1];
                template_[offset + 1] =
                    static_cast<uint8_t>(kCanonicalHtBody & 0xff);
                template_[offset + 2] =
                    static_cast<uint8_t>((kCanonicalHtBody >> 8) & 0xff);
                templateLength_ += deficit;
                return; // exactly one HT Capabilities tag is expected
            }
            offset += 3 + tlvLength;
        }
    }

    bool setBssid(const uint8_t *bssid)
    {
        if (bssid == NULL || isMulticast(bssid))
            return false;
        memcpy(bssid_, isZeroAddress(bssid) ? kDefaultBssid : bssid,
               sizeof(bssid_));
        return true;
    }

    void setSyncEnabled(bool enabled) { syncEnabled_ = enabled; }
    void setSyncState(uint32_t state) { syncState_ = state; }
    void setMasterChannel(uint16_t channel) { masterChannel_ = channel; }
    void setSecondaryMasterChannel(uint16_t channel) { secondaryMasterChannel_ = channel; }
    void setPresenceMode(uint32_t mode) { presenceMode_ = mode; }
    // Nonzero: advertise only this channel (the one the radio is actually
    // on) in the Sync Parameters and Channel Sequence TLVs, as OWL does.
    void setOverrideChannel(uint16_t channel) { overrideChannel_ = channel; }
    uint16_t overrideChannel() const { return overrideChannel_; }

    void setSyncParameters(uint32_t awLength, uint32_t awPeriod,
                           uint32_t extensionLength, uint32_t framePeriod)
    {
        availabilityWindowLength_ = awLength;
        availabilityWindowPeriod_ = awPeriod;
        extensionLength_ = extensionLength;
        synchronizationFramePeriod_ = framePeriod;
    }

    bool setChannelSequence(const uint16_t *channels, size_t count,
                            uint8_t encoding, uint8_t stepCount,
                            uint8_t duplicateCount, uint8_t fillChannel)
    {
        if (channels == NULL || count == 0 || count > kMaximumChannelSequence)
            return false;
        memcpy(channelSequence_, channels, count * sizeof(channels[0]));
        channelSequenceLength_ = count;
        channelEncoding_ = encoding;
        channelStepCount_ = stepCount;
        channelDuplicateCount_ = duplicateCount;
        channelFill_ = fillChannel;
        return true;
    }

    PeerObservation observe80211(const uint8_t *frame, size_t length,
                                 uint8_t channel, int8_t rssi,
                                 uint32_t rxGp2, uint64_t now,
                                 const uint8_t *nativeParent=nullptr,
                                 const uint8_t *nativeRoot=nullptr)
    {
        PeerObservation result = {};
        if (!isAwdlAction(frame, length))
            return result;

        result.valid = true;
        result.subtype = frame[kDot11HeaderLength + 6];
        result.channel = channel;
        result.rssi = rssi;
        memcpy(result.address, frame + 10, kMacLength); // 802.11 addr2

        size_t slot = kMaximumPeers;
        size_t oldest = 0;
        for (size_t i = 0; i < kMaximumPeers; ++i) {
            if (peers_[i].active &&
                memcmp(peers_[i].address, result.address, kMacLength) == 0) {
                slot = i;
                break;
            }
            if (!peers_[i].active && slot == kMaximumPeers)
                slot = i;
            if (peers_[i].lastSeen < peers_[oldest].lastSeen)
                oldest = i;
        }
        if (slot == kMaximumPeers)
            slot = oldest;

        result.newPeer = !peers_[slot].active ||
            memcmp(peers_[slot].address, result.address, kMacLength) != 0;
        if (result.newPeer)
            peers_[slot].window = {};
        peers_[slot].active = true;
        memcpy(peers_[slot].address, result.address, kMacLength);
        peers_[slot].channel = channel;
        peers_[slot].rssi = rssi;
        peers_[slot].lastSeen = now;
        // Legacy's actionFrameReport parses Sync Parameters in both MIF and
        // PSF. A PSF with the full timing/election/sequence tuple can provide
        // the RX anchor even when Legacy has yet to select this sender.
        const bool nativeSyncFrame = result.subtype == 0 && nativeParent && nativeRoot;
        if (result.subtype == 3 || nativeSyncFrame)
            result.followUpdated = observeMaster(actionTlvs(frame), actionTlvsLength(length),
                                                 result.address, rxGp2, now,
                                                 masterTxDelay(frame),nativeParent,nativeRoot,
                                                 channel,peers_[slot].window);
        return result;
    }

    // Port the ForbiddenItlwm awake-window preference for broadcast work and
    // a single recently heard peer. Callers supply their own wait bounds.
    bool peerWindowClosed(uint64_t nowNs, uint32_t gp2,
                          const uint8_t self[kMacLength]) const
    {
        if (!self)
            return false;
        const PeerWindow *candidate = nullptr;
        for (const auto &peer : peers_) {
            if (!peer.active || !memcmp(peer.address, self, kMacLength) ||
                nowNs < peer.lastSeen || nowNs - peer.lastSeen >= 2000000000ULL)
                continue;
            if (candidate || !peer.window.awUs || nowNs < peer.window.observedNs ||
                nowNs - peer.window.observedNs >= 2000000000ULL)
                return false;
            candidate = &peer.window;
        }
        if (!candidate)
            return false;
        const uint32_t elapsed = gp2 - candidate->eawStartGp2;
        if (elapsed >= 2300000U)
            return false;
        const unsigned slot = (candidate->eawNumber + elapsed / (4 * candidate->awUs)) % 16;
        const bool present = (candidate->channelSlots & (1U << slot)) != 0;
        return !present || elapsed % candidate->awUs >= 5120U;
    }

    bool deferRepeatingMi(uint64_t nowNs, uint32_t gp2, uint64_t lastAttemptNs,
                          const uint8_t self[kMacLength]) const
    {
        return lastAttemptNs && nowNs >= lastAttemptNs &&
            nowNs - lastAttemptNs < 350000000ULL &&
            peerWindowClosed(nowNs,gp2,self);
    }

    bool deferMulticastRequest(uint64_t nowNs, uint32_t gp2, uint64_t sinceNs,
                               unsigned wanted, const uint8_t self[kMacLength]) const
    {
        // Legacy request bit 4 is multicast. Mixed and unicast requests have
        // not disclosed their destinations yet and must stay native-driven.
        return wanted == 16 && (!sinceNs || (nowNs >= sinceNs && nowNs - sinceNs < 100000000ULL)) &&
            peerWindowClosed(nowNs,gp2,self);
    }

    bool copyFollowState(FollowState *output) const
    {
        if (output == NULL || !follow_.valid)
            return false;
        *output = follow_;
        return true;
    }

    // CPU copies of actual RX anchors, not another election implementation.
    // Capture precedes the native callback, which may choose this new parent
    // synchronously. Root identity and the source parent are independent.
    bool copyNativeAnchor(FollowState *out,const uint8_t parent[6],
                          const uint8_t root[6],uint64_t nowNs,
                          uint64_t maximumAgeNs=2000000000ULL) const
    {
        if(!out || !parent || !root)return false;
        const FollowState *best=nullptr;
        for(const auto &v:nativeAnchors_)
            if(v.valid && v.timingValid && !memcmp(v.syncSourceAddress,parent,6) &&
               !memcmp(v.masterAddress,root,6) && nowNs>=v.rxHostTimeNs &&
               nowNs-v.rxHostTimeNs<=maximumAgeNs &&
               (!best || v.rxHostTimeNs>best->rxHostTimeNs))best=&v;
        // Retain compatibility with the original non-native transport API.
        if(!best && follow_.valid && follow_.timingValid &&
           !memcmp(follow_.syncSourceAddress,parent,6) && !memcmp(follow_.masterAddress,root,6) &&
           nowNs>=follow_.rxHostTimeNs && nowNs-follow_.rxHostTimeNs<=maximumAgeNs)best=&follow_;
        if(!best)return false;
        *out=*best;return true;
    }

    // Exact Legacy programChipWithNewMaster 0x2abb6, internal SET229.
    // Borrowed, unversioned 44-byte written prefix: seq0/down2/root32/parent38.
    // Adopt only an actually received matching anchor; never synthesize one
    // from the request or interpret acceptance as a firmware/radio completion.
    bool acceptNativeAdvertiser(const void *payload,uint64_t nowNs)
    {
        if(!payload)return false;
        const auto *v=static_cast<const uint8_t *>(payload);FollowState found={};
        if(!copyNativeAnchor(&found,v+38,v+32,nowNs,uint64_t(kMaximumSyncExtrapolationUs)*1000ULL) ||
           readLe16(v)!=found.awSequence || readLe16(v+2)!=found.txCounter)return false;
        follow_=found;return true;
    }

    bool hasFollowState() const { return follow_.valid; }

    bool isAwdlAction(const uint8_t *frame, size_t length) const
    {
        if (frame == NULL || length < kDot11HeaderLength + kActionHeaderLength)
            return false;
        // Management/action, protocol version zero.
        if ((frame[0] & 0xfc) != 0xd0)
            return false;
        const uint8_t *action = frame + kDot11HeaderLength;
        if (action[0] != 127 || action[1] != 0x00 || action[2] != 0x17 ||
            action[3] != 0xf2 || action[4] != 8)
            return false;
        // Accept the configured BSSID and the protocol's well-known BSSID.
        const uint8_t *wireBssid = frame + 16;
        if (memcmp(wireBssid, bssid_, kMacLength) != 0 &&
            memcmp(wireBssid, kDefaultBssid, kMacLength) != 0)
            return false;
        return validateTlvs(action + kActionHeaderLength,
                            length - kDot11HeaderLength - kActionHeaderLength);
    }

    bool isAwdlActionBody(const uint8_t *action, size_t length) const
    {
        if (action == NULL || length < kActionHeaderLength)
            return false;
        if (action[0] != 127 || action[1] != 0x00 || action[2] != 0x17 ||
            action[3] != 0xf2 || action[4] != 8)
            return false;
        return validateTlvs(action + kActionHeaderLength,
                            length - kActionHeaderLength);
    }

    // Builds a raw 802.11 vendor action frame around Apple's opaque TLV
    // template. The private SYNC_FRAME_TEMPLATE request supplies the TLVs only,
    // so the driver must add both the 802.11 and 16-byte AWDL action headers.
    // The caller owns sequence control and the hardware/rate/channel decision.
    size_t composeActionFrame(uint8_t *output, size_t capacity,
                              const uint8_t *source, uint16_t sequenceControl,
                              uint32_t transmitTime,
                              bool masterTimed = false,
                              bool preserveNativeElection = false) const
    {
        if (output == NULL || source == NULL || templateLength_ == 0)
            return 0;
        // Legacy's standalone Channel Sequence parser reads two bytes per
        // slot even for encoding 0. Advertise the actual 20 MHz channel in
        // opclass format for the native BE200 fixed-channel path.
        const bool nativeFixedChannel = masterTimed && overrideChannel_ == 44;
        const uint8_t encoding = nativeFixedChannel ? 3 : effectiveEncoding();
        const size_t dynamicLength = dynamicTlvsLength(encoding);
        if (masterTimed && !dynamicLength)
            return 0;
        const size_t total = kDot11HeaderLength + kActionHeaderLength +
            templateLength_ + dynamicLength + serviceTlvsLength_;
        if (capacity < total)
            return 0;
        memset(output, 0, kDot11HeaderLength);
        output[0] = 0xd0;
        memset(output + 4, 0xff, kMacLength);       // destination
        memcpy(output + 10, source, kMacLength);   // source
        memcpy(output + 16, bssid_, kMacLength);   // BSSID
        output[22] = static_cast<uint8_t>(sequenceControl & 0xff);
        output[23] = static_cast<uint8_t>(sequenceControl >> 8);
        uint8_t *action = output + kDot11HeaderLength;
        memset(action, 0, kActionHeaderLength);
        action[0] = 127;
        action[1] = 0x00;
        action[2] = 0x17;
        action[3] = 0xf2;
        action[4] = 8;
        action[5] = 0x10;
        action[6] = 3;
        for (size_t i = 0; i < sizeof(transmitTime); ++i) {
            const uint8_t byte = static_cast<uint8_t>(transmitTime >> (i * 8));
            action[8 + i] = byte;
            action[12 + i] = byte;
        }
        memcpy(action + kActionHeaderLength, template_, templateLength_);
        if (!preserveNativeElection)
            patchTemplateForFollow(action + kActionHeaderLength, templateLength_);
        if (dynamicLength != 0 && appendDynamicTlvs(
                action + kActionHeaderLength + templateLength_, dynamicLength,
                source, transmitTime, masterTimed, encoding, nativeFixedChannel) != dynamicLength)
            return 0;
        if (serviceTlvsLength_ != 0)
            memcpy(action + kActionHeaderLength + templateLength_ + dynamicLength,
                   serviceTlvs_, serviceTlvsLength_);
        return total;
    }

    // BE200 uses native election authority, unlike the upstream host-only
    // follower. R9 could advertise an observed master while native windows
    // still announced self-master. All work here is bounded CPU-only: no
    // callback/sleep or firmware access between save and restore.
    // Native queued Actions may be service-only. Preserve their contents, but
    // never send a supplied Election/Sync naming an obsolete native plan.
    bool nativePlanMatches(const uint8_t *frame,size_t length,
                                  const uint8_t parent[6],const uint8_t root[6]) const {
        if(!frame || !parent || !root || length<kDot11HeaderLength+kActionHeaderLength ||
           !isAwdlActionBody(frame+kDot11HeaderLength,length-kDot11HeaderLength)) return false;
        const auto *tlvs=actionTlvs(frame);const auto bytes=actionTlvsLength(length);
        if(!validateTlvs(tlvs,bytes))return false;
        for(size_t off=0;off<bytes;) {
            if (bytes - off < 3) return false;
            const auto n=readLe16(tlvs+off+1);const auto *v=tlvs+off+3;
            if(tlvs[off]==24 && (n<40 || memcmp(v,root,6) || memcmp(v+6,parent,6)))return false;
            if(tlvs[off]==4 && (n<36 || memcmp(v+21,root,6)))return false;
            off+=3+n;
        }
        return true;
    }

    size_t composeNativeActionFrame(uint8_t *out, size_t capacity,
                                    const uint8_t source[6], uint16_t sequence,
                                    uint32_t gp2, const uint8_t master[6],
                                    const uint8_t self[6], uint64_t nowNs,
                                    const uint8_t *topMaster = nullptr)
    {
        if (!source || !master || !self || memcmp(source,self,6) ||
            isZeroAddress(master) || isMulticast(master)) return 0;
        // A native master change can precede its replacement template. Reject
        // that stale template instead of relabeling another election in flight.
        bool election=false;
        for(size_t off=0;off+3<=templateLength_;) {
            const size_t bytes=readLe16(template_+off+1);
            if(bytes>templateLength_-off-3)return 0;
            if(template_[off]==24) {
                // Exact Legacy buildAWDLElectionParamsVer2: root/top at
                // value0, selected parent/master at value6 (not reversed).
                if(bytes<40 || memcmp(template_+off+3,topMaster?topMaster:master,6) ||
                   memcmp(template_+off+9,master,6))return 0;
                election=true;
            }
            off+=3+bytes;
        }
        if(!election)return 0;
        FollowState selected={};
        if (!selectNativeTiming(selected, source, master, self, nowNs, topMaster))
            return 0;
        const FollowState saved=follow_;
        follow_=selected; // the same native-selected parent/root as WindowStart
        const size_t result=composeActionFrame(out,capacity,source,sequence,gp2,true,true);
        follow_=saved;
        if (result)
            out[kDot11HeaderLength+6]=0; // periodic synchronization frame (PSF)
        return result;
    }

    // IOCTL 225 supplies an owned native MIF template. It supplies no radio
    // schedule. Add current timing only to a locally enveloped TLV template,
    // preserving the exact native service and election bytes.
    size_t completeNativeMi(uint8_t *frame, size_t length, size_t capacity,
                            uint32_t gp2, const uint8_t *master,
                            const uint8_t *self, uint64_t nowNs,
                            const uint8_t *root)
    {
        if (length > capacity || !nativePlanMatches(frame,length,master,root) ||
            frame[kDot11HeaderLength+6] != 3)
            return 0;
        bool election = false;
        for (size_t i = 0; i < actionTlvsLength(length);) {
            const auto *tlv = actionTlvs(frame) + i;
            if (actionTlvsLength(length) - i < 3 || tlv[0] == 4 || tlv[0] == 18)
                return 0;
            election |= tlv[0] == 24;
            i += 3 + readLe16(tlv+1);
        }
        FollowState selected={};
        const auto *source = frame + 10;
        if (!election || !selectNativeTiming(selected,source,master,self,nowNs,root))
            return 0;
        const FollowState saved=follow_;
        follow_=selected;
        const bool fixed = overrideChannel_ == 44;
        const size_t bytes = appendDynamicTlvs(frame+length,capacity-length,
            source,gp2,true,fixed ? 3 : effectiveEncoding(),fixed);
        follow_=saved;
        return bytes ? length + bytes : 0;
    }

    // IO80211 builds the complete Master Indication frame, including the
    // Service Response TLVs (tag 2: _airdrop PTR/SRV/TXT) that peers use to
    // discover services and the host-name TLV (16) SRV targets refer to, and
    // hands it over as AWDL_OOB_AUTO_REQUEST for firmware to repeat. Keep
    // only those TLVs; the timing fields stay ours.
    // Returns the number of service TLVs kept, or -1 if the body is invalid.
    int setServiceTlvsFromFrame(const uint8_t *body, size_t length)
    {
        if (body == NULL)
            return -1;
        const uint8_t *tlvs = body;
        size_t count = length;
        if (isAwdlActionBody(body, length)) {
            tlvs += kActionHeaderLength;
            count -= kActionHeaderLength;
        }
        if (count == 0 || !validateTlvs(tlvs, count))
            return -1;
        size_t kept = 0;
        size_t used = 0;
        size_t offset = 0;
        serviceTlvsDropped_ = 0;
        while (offset + 3 <= count) {
            const uint8_t type = tlvs[offset];
            const size_t tlvLength = static_cast<size_t>(tlvs[offset + 1]) |
                (static_cast<size_t>(tlvs[offset + 2]) << 8);
            if (tlvLength > count - offset - 3)
                break;
            if ((type == 2 || type == 16) &&
                used + 3 + tlvLength <= kMaximumServiceTlvLength) {
                memcpy(serviceTlvs_ + used, tlvs + offset, 3 + tlvLength);
                used += 3 + tlvLength;
                ++kept;
            } else if (type == 2 || type == 16) {
                ++serviceTlvsDropped_;
            }
            offset += 3 + tlvLength;
        }
        serviceTlvsLength_ = used;
        return static_cast<int>(kept);
    }

    size_t serviceTlvsLength() const { return serviceTlvsLength_; }
    uint32_t serviceTlvsDropped() const { return serviceTlvsDropped_; }

    size_t templateLength() const { return templateLength_; }
    bool syncEnabled() const { return syncEnabled_; }
    void copyBssid(uint8_t *output) const
    {
        if (output != NULL)
            memcpy(output, bssid_, kMacLength);
    }
    bool copySelfAddress(uint8_t *output) const
    {
        if (output == NULL || isZeroAddress(selfAddress_))
            return false;
        memcpy(output, selfAddress_, kMacLength);
        return true;
    }
    bool hasPeer(const uint8_t *address) const
    {
        for (size_t i = 0; i < kMaximumPeers; ++i)
            if (peers_[i].active && memcmp(peers_[i].address, address, kMacLength) == 0)
                return true;
        return false;
    }

    size_t peerCount() const
    {
        size_t count = 0;
        for (size_t i = 0; i < kMaximumPeers; ++i)
            if (peers_[i].active)
                ++count;
        return count;
    }
    const Peer *peer(size_t index) const
    {
        return index < kMaximumPeers && peers_[index].active ? &peers_[index] : NULL;
    }

private:
    bool selectNativeTiming(FollowState &selected, const uint8_t *source,
                            const uint8_t *master, const uint8_t *self,
                            uint64_t nowNs, const uint8_t *root) const
    {
        if (!source || !master || !self || memcmp(source,self,6) ||
            isZeroAddress(master) || isMulticast(master))
            return false;
        if (!memcmp(master,self,6))
            return !root || !memcmp(root,self,6);
        return copyNativeAnchor(&selected,master,root ? root : master,nowNs);
    }
    static uint16_t readLe16(const uint8_t *input)
    {
        return static_cast<uint16_t>(input[0]) |
            (static_cast<uint16_t>(input[1]) << 8);
    }

    static uint32_t readLe32(const uint8_t *input)
    {
        return static_cast<uint32_t>(input[0]) |
            (static_cast<uint32_t>(input[1]) << 8) |
            (static_cast<uint32_t>(input[2]) << 16) |
            (static_cast<uint32_t>(input[3]) << 24);
    }

    // The AWDL header carries PHY and target TX times (sender clock, us).
    // The sender computed its txcounter for the target time, so the frame's
    // air time is late by their difference (observed 50-3500 us on iPhone).
    static uint32_t masterTxDelay(const uint8_t *frame)
    {
        const uint8_t *header = frame + kDot11HeaderLength;
        const uint32_t phy = readLe32(header + 8);
        const uint32_t target = readLe32(header + 12);
        const uint32_t delay = phy - target;
        return delay <= kMaximumMasterTxDelayUs ? delay : 0;
    }

    static const uint8_t *actionTlvs(const uint8_t *frame)
    {
        return frame + kDot11HeaderLength + kActionHeaderLength;
    }

    static size_t actionTlvsLength(size_t frameLength)
    {
        return frameLength - kDot11HeaderLength - kActionHeaderLength;
    }

    static bool counterNewer(uint32_t candidate, uint32_t current)
    {
        return static_cast<int32_t>(candidate - current) > 0;
    }

    bool observeMaster(const uint8_t *bytes, size_t length,
                       const uint8_t *source, uint32_t rxGp2,
                       uint64_t nowNanoseconds, uint32_t txDelayUs,
                       const uint8_t *nativeParent,
                       const uint8_t *nativeRoot,
                       uint8_t rxChannel, PeerWindow &peerWindow)
    {
        FollowState candidate = {};
        uint8_t syncRoot[6]={};
        bool haveSync = false;
        bool haveSequence = false;
        bool haveElection = false;
        size_t offset = 0;
        while (offset + 3 <= length) {
            const uint8_t type = bytes[offset];
            const size_t tlvLength = readLe16(bytes + offset + 1);
            offset += 3;
            if (tlvLength > length - offset)
                return false;
            const uint8_t *value = bytes + offset;
            if (type == 4 && tlvLength >= 36) {
                candidate.nextAwChannel = value[0];
                candidate.txCounter = readLe16(value + 1);
                candidate.masterChannel = value[3];
                candidate.guardTime = value[4];
                candidate.awPeriod = readLe16(value + 5);
                candidate.actionFramePeriod = readLe16(value + 7);
                candidate.extensionLength = readLe16(value + 11);
                candidate.awLength = readLe16(value + 13);
                candidate.presenceMode = value[27];
                candidate.awSequence = readLe16(value + 29);
                memcpy(syncRoot,value+21,6);
                haveSync = candidate.awPeriod != 0;
            } else if (type == 18 && tlvLength >= 6) {
                const size_t count = static_cast<size_t>(value[0]) + 1;
                const uint8_t encoding = value[1];
                const size_t entryLength = encoding == 0 ? 1 : 2;
                if (count <= kMaximumChannelSequence &&
                    count <= (tlvLength - 6) / entryLength) {
                    candidate.channelEncoding = encoding;
                    candidate.channelDuplicateCount = value[2];
                    candidate.channelStepCount = value[3];
                    candidate.channelFill = readLe16(value + 4);
                    candidate.channelSequenceLength = count;
                    for (size_t i = 0; i < count; ++i) {
                        const uint8_t *entry = value + 6 + i * entryLength;
                        candidate.channelSequence[i] = entryLength == 1 ?
                            entry[0] : readLe16(entry);
                    }
                    haveSequence = true;
                }
            } else if (type == 24 && tlvLength >= 40) {
                memcpy(candidate.masterAddress, value, kMacLength);
                candidate.masterCounter = readLe32(value + 12);
                candidate.distanceToMaster = readLe32(value + 16);
                candidate.masterMetric = readLe32(value + 20);
                candidate.selfMetric = readLe32(value + 24);
                candidate.selfCounter = readLe32(value + 36);
                haveElection = !isZeroAddress(candidate.masterAddress) &&
                    !isMulticast(candidate.masterAddress);
            }
            offset += tlvLength;
        }
        const bool native = nativeParent && nativeRoot;
        // Legacy rejects a remaining one or two bytes as a truncated TLV
        // header. Do not let such a frame refresh its companion RX anchor.
        if ((native && offset != length) || !haveSync || !haveSequence || !haveElection)
            return false;

        // Prefer the master itself as the schedule source. A follower advertises
        // the same election winner but may carry a locally adjusted sequence.
        const bool sourceIsMaster = memcmp(source, candidate.masterAddress,
                                           kMacLength) == 0;
        const bool nativeSelected=native && !memcmp(source,nativeParent,6) &&
            !memcmp(candidate.masterAddress,nativeRoot,6) && !memcmp(syncRoot,nativeRoot,6);
        const bool sameMaster = follow_.valid &&
            !memcmp(follow_.masterAddress,candidate.masterAddress,6);
        if(!native && !sourceIsMaster)return false;
        if(!native && follow_.valid && !sameMaster && candidate.masterMetric<follow_.masterMetric)return false;
        if(!native && sameMaster && candidate.masterCounter!=follow_.masterCounter &&
           !counterNewer(candidate.masterCounter,follow_.masterCounter))return false;
        memcpy(candidate.syncSourceAddress,source,6);
        candidate.rxGp2 = rxGp2;
        candidate.rxHostTimeNs = nowNanoseconds;
        const uint32_t eawPeriodTu =
            static_cast<uint32_t>(candidate.presenceMode) *
            static_cast<uint32_t>(candidate.awPeriod);
        if (eawPeriodTu != 0 && candidate.txCounter != 0 &&
            candidate.txCounter <= eawPeriodTu &&
            eawPeriodTu <= UINT32_MAX / 1024U) {
            candidate.eawPeriodUs = eawPeriodTu * 1024U;
            // OWL's sync epoch reduces to the next EAW boundary in the
            // receiver's clock: RX GP2, less the sender's PHY-vs-target
            // delay, plus the advertised TU down-counter.
            candidate.nextEawGp2 = rxGp2 - txDelayUs +
                static_cast<uint32_t>(candidate.txCounter) * 1024U;
            candidate.timingValid = true;
        }
        // A transmitter's own advertised slots can still inform a bounded
        // send preference when its Sync Root differs from its Election Root.
        // This observation must never qualify the separate parent anchor.
        if (native && nowNanoseconds >= peerWindow.observedNs) {
            peerWindow = {};
            if (candidate.timingValid && rxChannel == 44 &&
                candidate.presenceMode == 4 && candidate.awPeriod >= 8 &&
                candidate.awPeriod <= 64 && candidate.channelSequenceLength == 16 &&
                candidate.channelEncoding == 3 && candidate.channelStepCount == 3 &&
                !candidate.channelDuplicateCount && candidate.channelFill == 0xffff) {
                uint16_t slots = 0;
                for (size_t i = 0; i < 16; ++i) {
                    const uint16_t entry = candidate.channelSequence[i];
                    if (entry == 0x732c || entry == 0x802c)
                        slots |= uint16_t(1U << i);
                }
                const uint16_t eaw = candidate.awSequence / 4;
                const uint32_t start = candidate.nextEawGp2 - candidate.eawPeriodUs;
                // A captured arrival off its projected slot does not qualify.
                if ((slots & (1U << (eaw % 16))) && rxGp2 - start < candidate.eawPeriodUs)
                    peerWindow = {nowNanoseconds,start,uint32_t(candidate.awPeriod)*1024U,eaw,slots};
            }
        }
        if (sameMaster) {
            // GP2 is a wrapping 32-bit microsecond clock. Unsigned subtraction
            // preserves the elapsed interval across the wrap boundary.
            candidate.rxGp2Delta = rxGp2 - follow_.rxGp2;
            candidate.rxGp2DeltaValid = true;
        }
        candidate.valid = true;
        if(native && candidate.timingValid && !memcmp(syncRoot,candidate.masterAddress,6)) {
            size_t slot=0;
            for(;slot<kMaximumPeers;++slot)
                if(nativeAnchors_[slot].valid && !memcmp(nativeAnchors_[slot].syncSourceAddress,source,6))break;
            if(slot==kMaximumPeers) {
                slot=0;for(size_t i=1;i<kMaximumPeers;++i)
                    if(nativeAnchors_[i].rxHostTimeNs<nativeAnchors_[slot].rxHostTimeNs)slot=i;
            }
            // A delayed RX mailbox item may not roll back a newer observation.
            if(!nativeAnchors_[slot].valid || nowNanoseconds>=nativeAnchors_[slot].rxHostTimeNs)
                nativeAnchors_[slot]=candidate;
        }
        if(native && !nativeSelected)return false;
        if(native && follow_.valid && !memcmp(follow_.syncSourceAddress,source,6) &&
           nowNanoseconds<follow_.rxHostTimeNs)return false;
        follow_ = candidate;
        return true;
    }

    size_t channelSequenceWireLength(uint8_t encoding) const
    {
        const size_t count = effectiveSequenceLength();
        if (count == 0)
            return 0;
        const size_t entryLength = encoding == 0 ? 1 : 2;
        return 6 + count * entryLength;
    }

    uint8_t effectiveEncoding() const
    {
        if (follow_.valid)
            return follow_.channelEncoding;
        return overrideChannel_ != 0 ? 0 : channelEncoding_;
    }

    // One entry advertising the override channel in the given encoding. The
    // opclass form matches Google's AWDL (Quick Share) and Apple peers:
    // channel byte plus operating class 128 at 5 GHz, 81 at 2.4 GHz.
    uint16_t overrideEntry(uint8_t encoding) const
    {
        if (encoding == 0)
            return overrideChannel_;
        return static_cast<uint16_t>(overrideChannel_ |
            ((overrideChannel_ > 14 ? 0x80 : 0x51) << 8));
    }

    // itlwm_awdl_chan_override=1: advertise only the channel the AWDL radio
    // really attends, in every slot, even while following a master. Copying
    // the master's multi-channel schedule made peers send to us on channels
    // and in slots where we were absent.
    const uint16_t *effectiveSequence(uint16_t *scratch, uint8_t encoding,
                                      bool nativeFixedChannel) const
    {
        if (overrideChannel_ != 0) {
            const size_t count = effectiveSequenceLength();
            // BE200 currently fixes AWDL to 44/20. This field describes the
            // AWDL PHY, independent of the associated infrastructure width.
            const uint16_t entry = nativeFixedChannel ?
                static_cast<uint16_t>(overrideChannel_ | (115U << 8)) :
                overrideEntry(encoding);
            for (size_t i = 0; i < count && i < kMaximumChannelSequence; ++i)
                scratch[i] = entry;
            return scratch;
        }
        if (follow_.valid)
            return follow_.channelSequence;
        return channelSequence_;
    }

    size_t effectiveSequenceLength() const
    {
        return follow_.valid ? follow_.channelSequenceLength :
            channelSequenceLength_;
    }

    uint8_t effectiveDuplicateCount() const
    {
        return follow_.valid ? follow_.channelDuplicateCount :
            channelDuplicateCount_;
    }

    uint8_t effectiveStepCount() const
    {
        return follow_.valid ? follow_.channelStepCount : channelStepCount_;
    }

    uint16_t effectiveFillChannel() const
    {
        if (overrideChannel_ != 0)
            return 0xffff;
        return follow_.valid ? follow_.channelFill : channelFill_;
    }

    size_t dynamicTlvsLength(uint8_t encoding) const
    {
        const size_t sequenceLength = channelSequenceWireLength(encoding);
        if (sequenceLength == 0)
            return 0;
        return (36 + sequenceLength + 2) + (3 + sequenceLength + 3);
    }

    static void writeLe16(uint8_t *output, uint16_t value)
    {
        output[0] = static_cast<uint8_t>(value & 0xff);
        output[1] = static_cast<uint8_t>(value >> 8);
    }

    static void writeLe32(uint8_t *output, uint32_t value)
    {
        output[0] = static_cast<uint8_t>(value & 0xff);
        output[1] = static_cast<uint8_t>((value >> 8) & 0xff);
        output[2] = static_cast<uint8_t>((value >> 16) & 0xff);
        output[3] = static_cast<uint8_t>(value >> 24);
    }

    uint32_t followerSelfMetric() const
    {
        if (follow_.masterMetric > kFollowerSelfMetric)
            return kFollowerSelfMetric;
        return follow_.masterMetric > 0 ? follow_.masterMetric - 1 : 0;
    }

    void patchTemplateForFollow(uint8_t *bytes, size_t length) const
    {
        if (!follow_.valid)
            return;
        size_t offset = 0;
        while (offset + 3 <= length) {
            const uint8_t type = bytes[offset];
            const size_t tlvLength = readLe16(bytes + offset + 1);
            offset += 3;
            if (tlvLength > length - offset)
                return;
            uint8_t *value = bytes + offset;
            // Apple's self metric keeps rising because IO80211 believes this
            // node is master. Advertised above the followed master's, it
            // claims mastership while naming another master, an inconsistent
            // tree. Follow OWL: a follower advertises the initial metric.
            if (type == 24 && tlvLength >= 40) {
                memcpy(value, follow_.masterAddress, kMacLength);
                memcpy(value + 6, follow_.masterAddress, kMacLength);
                writeLe32(value + 12, follow_.masterCounter);
                writeLe32(value + 16, 1);
                writeLe32(value + 20, follow_.masterMetric);
                writeLe32(value + 24, followerSelfMetric());
            } else if (type == 5 && tlvLength >= 21) {
                value[3] = 1;
                memcpy(value + 5, follow_.masterAddress, kMacLength);
                writeLe32(value + 11, follow_.masterMetric);
                writeLe32(value + 15, followerSelfMetric());
            }
            offset += tlvLength;
        }
    }

    static size_t writeChannelSequence(uint8_t *output, size_t capacity,
                                       const uint16_t *channels, size_t count,
                                       uint8_t encoding, uint8_t duplicateCount,
                                       uint8_t stepCount, uint16_t fillChannel)
    {
        const size_t entryLength = encoding == 0 ? 1 : 2;
        const size_t required = 6 + count * entryLength;
        if (output == NULL || channels == NULL || count == 0 ||
            capacity < required)
            return 0;
        output[0] = static_cast<uint8_t>(count - 1);
        output[1] = encoding;
        output[2] = duplicateCount;
        output[3] = stepCount;
        writeLe16(output + 4, fillChannel);
        size_t offset = 6;
        for (size_t i = 0; i < count; ++i) {
            output[offset++] = static_cast<uint8_t>(channels[i] & 0xff);
            if (entryLength == 2)
                output[offset++] = static_cast<uint8_t>(channels[i] >> 8);
        }
        return offset;
    }

    size_t appendDynamicTlvs(uint8_t *output, size_t capacity,
                             const uint8_t *source,
                             uint32_t transmitTime,
                             bool masterTimed, uint8_t encoding,
                             bool nativeFixedChannel) const
    {
        const size_t sequenceLength = channelSequenceWireLength(encoding);
        const size_t required = dynamicTlvsLength(encoding);
        if (output == NULL || source == NULL || sequenceLength == 0 ||
            capacity < required)
            return 0;

        const uint16_t awPeriod = follow_.valid ? follow_.awPeriod :
            (availabilityWindowPeriod_ == 0 ? 16 :
            static_cast<uint16_t>(availabilityWindowPeriod_));
        const uint16_t awLength = follow_.valid ? follow_.awLength :
            (availabilityWindowLength_ == 0 ? awPeriod :
            static_cast<uint16_t>(availabilityWindowLength_));
        const uint16_t extensionLength = follow_.valid ?
            follow_.extensionLength :
            (extensionLength_ == 0 ? awLength :
            static_cast<uint16_t>(extensionLength_));
        const uint16_t framePeriod = follow_.valid ?
            follow_.actionFramePeriod :
            (synchronizationFramePeriod_ == 0 ? 110 :
            static_cast<uint16_t>(synchronizationFramePeriod_));
        const uint8_t presence = follow_.valid ? follow_.presenceMode :
            (presenceMode_ == 0 ? 4 :
            static_cast<uint8_t>(presenceMode_));
        // Without a master-timed GP2 instant, keep the original local-clock
        // fields; they are only coherent when this node is its own master.
        const uint32_t nowTu = transmitTime / 1024U;
        uint16_t downCounter = static_cast<uint16_t>(
            awPeriod - (nowTu % awPeriod));
        uint16_t awSequence = static_cast<uint16_t>(nowTu / awPeriod);
        uint16_t remainingAwLength = awLength > downCounter ?
            static_cast<uint16_t>(awLength - downCounter) : 0;
        if (masterTimed && !follow_.valid) {
            if (!localSyncAt(transmitTime,availabilityWindowPeriod_,availabilityWindowLength_,
                             presenceMode_,downCounter,awSequence,remainingAwLength))
                return 0;
        } else if (masterTimed) {
            // A recent host observation does not make every GP2 value
            // usable. Never pair a foreign root with our local phase.
            if (!masterSyncAt(follow_, transmitTime, &downCounter, &awSequence))
                return 0;
            const uint32_t elapsed =
                static_cast<uint32_t>(awPeriod) * presence - downCounter;
            remainingAwLength = awLength > elapsed ?
                static_cast<uint16_t>(awLength - elapsed) : 0;
        }
        const uint8_t extensionCount = presence > 0 ? presence - 1 : 0;
        const uint16_t masterChannel = overrideChannel_ != 0 ?
            overrideChannel_ : (follow_.valid ? follow_.masterChannel :
            masterChannel_);
        const uint8_t nextAwChannel = overrideChannel_ != 0 ?
            static_cast<uint8_t>(overrideChannel_) : (follow_.valid ?
            follow_.nextAwChannel : static_cast<uint8_t>(masterChannel));
        uint16_t scratch[kMaximumChannelSequence];
        const uint16_t *sequence = effectiveSequence(scratch, encoding, nativeFixedChannel);

        const size_t syncTotal = 36 + sequenceLength + 2;
        memset(output, 0, syncTotal);
        output[0] = 4;
        writeLe16(output + 1, static_cast<uint16_t>(syncTotal - 3));
        output[3] = nextAwChannel;
        writeLe16(output + 4, downCounter);
        output[6] = static_cast<uint8_t>(masterChannel & 0xff);
        writeLe16(output + 8, awPeriod);
        writeLe16(output + 10, framePeriod);
        writeLe16(output + 12, 0x1800);
        writeLe16(output + 14, extensionLength);
        writeLe16(output + 16, awLength);
        writeLe16(output + 18, remainingAwLength);
        output[20] = extensionCount;
        output[21] = extensionCount;
        output[22] = extensionCount;
        output[23] = extensionCount;
        memcpy(output + 24, follow_.valid ? follow_.masterAddress : source,
               kMacLength);
        output[30] = presence;
        writeLe16(output + 32, awSequence);
        writeLe16(output + 34, awSequence);
        if (writeChannelSequence(output + 36, sequenceLength,
                                 sequence, effectiveSequenceLength(),
                                 encoding, effectiveDuplicateCount(),
                                 effectiveStepCount(), effectiveFillChannel()) != sequenceLength)
            return 0;

        uint8_t *channelTlv = output + syncTotal;
        const size_t channelTotal = 3 + sequenceLength + 3;
        memset(channelTlv, 0, channelTotal);
        channelTlv[0] = 18;
        writeLe16(channelTlv + 1,
                  static_cast<uint16_t>(channelTotal - 3));
        if (writeChannelSequence(channelTlv + 3, sequenceLength,
                                 sequence, effectiveSequenceLength(),
                                 encoding, effectiveDuplicateCount(),
                                 effectiveStepCount(), effectiveFillChannel()) != sequenceLength)
            return 0;
        return syncTotal + channelTotal;
    }

    static bool isZeroAddress(const uint8_t *address)
    {
        uint8_t any = 0;
        for (size_t i = 0; i < kMacLength; ++i)
            any |= address[i];
        return any == 0;
    }

    static bool isMulticast(const uint8_t *address) { return (address[0] & 1) != 0; }

    static bool validateTlvs(const uint8_t *bytes, size_t length)
    {
        // Empty TLV tails occur on small PSFs. Otherwise every TLV is type +
        // little-endian 16-bit length and must remain inside the frame.
        size_t offset = 0;
        while (offset < length) {
            if (length - offset < 3) {
                while (offset < length)
                    if (bytes[offset++] != 0)
                        return false;
                return true;
            }
            const size_t tlvLength = static_cast<size_t>(bytes[offset + 1]) |
                (static_cast<size_t>(bytes[offset + 2]) << 8);
            offset += 3;
            if (tlvLength > length - offset)
                return false;
            offset += tlvLength;
        }
        return true;
    }

    bool syncEnabled_;
    uint8_t bssid_[kMacLength];
    uint8_t selfAddress_[kMacLength];
    uint8_t template_[kMaximumTemplateLength];
    size_t templateLength_;
    uint8_t serviceTlvs_[kMaximumServiceTlvLength];
    size_t serviceTlvsLength_;
    uint32_t serviceTlvsDropped_;
    uint32_t syncState_;
    uint32_t presenceMode_;
    uint16_t masterChannel_;
    uint16_t secondaryMasterChannel_;
    uint16_t overrideChannel_;
    uint32_t availabilityWindowLength_;
    uint32_t availabilityWindowPeriod_;
    uint32_t extensionLength_;
    uint32_t synchronizationFramePeriod_;
    uint16_t channelSequence_[kMaximumChannelSequence];
    size_t channelSequenceLength_;
    uint8_t channelEncoding_;
    uint8_t channelStepCount_;
    uint8_t channelDuplicateCount_;
    uint8_t channelFill_;
    FollowState follow_;
    FollowState nativeAnchors_[kMaximumPeers]; // owned RX copies, reset with radio epoch
    Peer peers_[kMaximumPeers];
};

} // namespace itlwm_awdl

#endif /* AwdlTransport_hpp */
