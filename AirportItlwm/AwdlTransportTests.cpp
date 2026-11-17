#include "AwdlTransport.hpp"

#include <assert.h>
#include <stdio.h>

using namespace itlwm_awdl;

static void makeAction(uint8_t *frame, size_t length, uint8_t sourceTail)
{
    memset(frame, 0, length);
    frame[0] = 0xd0;
    memset(frame + 4, 0xff, 6);
    frame[10] = 0x02;
    frame[15] = sourceTail;
    memcpy(frame + 16, kDefaultBssid, 6);
    uint8_t *action = frame + kDot11HeaderLength;
    action[0] = 127;
    action[1] = 0x00;
    action[2] = 0x17;
    action[3] = 0xf2;
    action[4] = 8;
    action[5] = 0x10;
    action[6] = 3;
}

int main()
{
    TransportState state;
    assert(!state.hasFollowState());
    assert(!state.syncEnabled());
    state.setSyncEnabled(true);
    assert(state.syncEnabled());
    uint8_t frame[kDot11HeaderLength + kActionHeaderLength + 5];
    makeAction(frame, sizeof(frame), 1);
    frame[kDot11HeaderLength + kActionHeaderLength + 0] = 21;
    frame[kDot11HeaderLength + kActionHeaderLength + 1] = 2;
    frame[kDot11HeaderLength + kActionHeaderLength + 2] = 0;
    frame[kDot11HeaderLength + kActionHeaderLength + 3] = 0x10;
    frame[kDot11HeaderLength + kActionHeaderLength + 4] = 1;

    PeerObservation first = state.observe80211(frame, sizeof(frame), 6, -42, 0, 100);
    assert(first.valid && first.newPeer && first.subtype == 3);
    assert(state.peerCount() == 1);
    PeerObservation again = state.observe80211(frame, sizeof(frame), 44, -51, 0, 200);
    assert(again.valid && !again.newPeer && state.peerCount() == 1);

    uint8_t malformed[sizeof(frame)];
    memcpy(malformed, frame, sizeof(frame));
    malformed[kDot11HeaderLength + kActionHeaderLength + 1] = 20;
    assert(!state.observe80211(malformed, sizeof(malformed), 6, -42, 0, 300).valid);
    malformed[kDot11HeaderLength + 1] = 1;
    assert(!state.isAwdlAction(malformed, sizeof(malformed)));

    const uint8_t actionTemplate[] = { 21, 2, 0, 0x10, 1 };
    assert(state.setTemplate(actionTemplate, sizeof(actionTemplate)));
    uint8_t raw[256];
    const uint8_t source[6] = { 2, 3, 4, 5, 6, 7 };
    size_t rawLength = state.composeActionFrame(raw, sizeof(raw), source,
                                                0x1230, 0x78563412);
    assert(rawLength == kDot11HeaderLength + kActionHeaderLength +
                        sizeof(actionTemplate));
    assert(raw[0] == 0xd0 && memcmp(raw + 10, source, 6) == 0);
    assert(raw[22] == 0x30 && raw[23] == 0x12);
    assert(raw[24] == 127 && raw[25] == 0 && raw[26] == 0x17 &&
           raw[27] == 0xf2 && raw[28] == 8 && raw[29] == 0x10 &&
           raw[30] == 3);
    assert(raw[32] == 0x12 && raw[33] == 0x34 && raw[34] == 0x56 &&
           raw[35] == 0x78 && raw[36] == 0x12 && raw[37] == 0x34 &&
           raw[38] == 0x56 && raw[39] == 0x78);
    assert(memcmp(raw + kDot11HeaderLength + kActionHeaderLength,
                  actionTemplate,
                  sizeof(actionTemplate)) == 0);
    assert(state.isAwdlAction(raw, rawLength));

    const uint8_t electionTemplate[] = {
        24, 6, 0, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    assert(state.setTemplate(electionTemplate, sizeof(electionTemplate)));
    uint8_t advertised[6] = {};
    assert(state.copySelfAddress(advertised));
    assert(memcmp(advertised, electionTemplate + 3, sizeof(advertised)) == 0);
    assert(state.setTemplate(actionTemplate, sizeof(actionTemplate)));

    uint16_t channels[16] = {};
    channels[0] = channels[8] = channels[12] = 6;
    assert(state.setChannelSequence(channels, 16, 0, 3, 0, 0xff));
    state.setSyncParameters(16, 16, 16, 110);
    state.setMasterChannel(6);
    state.setPresenceMode(4);
    rawLength = state.composeActionFrame(raw, sizeof(raw), source,
                                         0x1240, 0x78563412);
    assert(rawLength == 133);
    const size_t dynamic = kDot11HeaderLength + kActionHeaderLength +
        sizeof(actionTemplate);
    assert(raw[dynamic] == 4);
    assert(raw[dynamic + 1] == 57 && raw[dynamic + 2] == 0);
    assert(raw[dynamic + 3] == 6 && raw[dynamic + 6] == 6);
    assert(raw[dynamic + 36] == 15 && raw[dynamic + 37] == 0);
    assert(raw[dynamic + 42] == 6);
    assert(raw[dynamic + 60] == 18);
    assert(raw[dynamic + 61] == 25 && raw[dynamic + 62] == 0);
    assert(state.isAwdlAction(raw, rawLength));

    // Advertised-channel override: every slot and the master channel report
    // the radio's real channel, encoded as plain channel numbers.
    state.setOverrideChannel(48);
    rawLength = state.composeActionFrame(raw, sizeof(raw), source,
                                         0x1250, 0x78563412);
    assert(rawLength == 133);
    assert(raw[dynamic + 3] == 48 && raw[dynamic + 6] == 48);
    assert(raw[dynamic + 37] == 0);
    for (size_t i = 0; i < 16; ++i) {
        assert(raw[dynamic + 42 + i] == 48);
        assert(raw[dynamic + 60 + 3 + 6 + i] == 48);
    }
    assert(state.isAwdlAction(raw, rawLength));
    state.setOverrideChannel(0);
    rawLength = state.composeActionFrame(raw, sizeof(raw), source,
                                         0x1260, 0x78563412);
    assert(raw[dynamic + 3] == 6 && raw[dynamic + 42] == 6 &&
           raw[dynamic + 43] == 0);

    // A real MIF shape: sync parameters, opclass channel sequence, and
    // election-v2. Only a frame sent by the advertised master is accepted as
    // the schedule anchor; follower frames cannot overwrite it.
    uint8_t mif[kDot11HeaderLength + kActionHeaderLength + 3 + 36 +
                3 + 41 + 3 + 40] = {};
    makeAction(mif, sizeof(mif), 0x14);
    const uint8_t master[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x14 };
    memcpy(mif + 10, master, sizeof(master));
    uint8_t *tlv = mif + kDot11HeaderLength + kActionHeaderLength;
    tlv[0] = 4; tlv[1] = 36;
    tlv[3] = 48; tlv[4] = 48; tlv[6] = 0; tlv[8] = 16;
    tlv[10] = 110; tlv[14] = 16; tlv[16] = 16; tlv[30] = 4;
    tlv[32] = 93;
    tlv += 3 + 36;
    tlv[0] = 18; tlv[1] = 41;
    tlv[3] = 15; tlv[4] = 3; tlv[6] = 3; tlv[7] = 0xff; tlv[8] = 0xff;
    const uint16_t peerSequence[16] = {
        0x8030, 0, 0x8095, 0, 0, 0x802c, 0x8030, 0x8030,
        0x5106, 0, 0x802c, 0, 0, 0x802c, 0x8030, 0x8030
    };
    for (size_t i = 0; i < 16; ++i) {
        tlv[9 + i * 2] = static_cast<uint8_t>(peerSequence[i]);
        tlv[10 + i * 2] = static_cast<uint8_t>(peerSequence[i] >> 8);
    }
    tlv += 3 + 41;
    tlv[0] = 24; tlv[1] = 40;
    memcpy(tlv + 3, master, sizeof(master));
    memcpy(tlv + 9, master, sizeof(master));
    tlv[15] = 0xa6; tlv[16] = 0x08;
    tlv[23] = 65; tlv[27] = 65; tlv[39] = 0xa6; tlv[40] = 0x08;
    PeerObservation masterObservation = state.observe80211(
        mif, sizeof(mif), 48, -35, 0xfffff000U, 400);
    assert(masterObservation.valid && masterObservation.followUpdated);
    FollowState follow = {};
    assert(state.copyFollowState(&follow));
    assert(state.hasFollowState());
    assert(memcmp(follow.masterAddress, master, sizeof(master)) == 0);
    assert(follow.masterMetric == 65 && follow.masterCounter == 2214);
    assert(follow.awPeriod == 16 && follow.actionFramePeriod == 110);
    assert(follow.txCounter == 48 && follow.awSequence == 93);
    assert(follow.rxGp2 == 0xfffff000U && !follow.rxGp2DeltaValid);
    assert(follow.timingValid && follow.eawPeriodUs == 65536U);
    assert(follow.nextEawGp2 == 0x0000b000U);
    assert(follow.channelEncoding == 3 && follow.channelSequenceLength == 16);
    assert(follow.channelSequence[0] == 0x8030 &&
           follow.channelSequence[2] == 0x8095 &&
           follow.channelSequence[8] == 0x5106);

    // itlwm_awdl_chan_override while following: advertise only the attended
    // channel, byte for byte like an interoperable AWDL peer:
    // TLV 18 = 0f 03 00 03 ff ff + (2c 80) x16, and sync next-AW/master = 44.
    {
        state.setOverrideChannel(44);
        uint8_t honest[1024];
        const size_t honestLength = state.composeActionFrame(
            honest, sizeof(honest), source, 0x1270, 0x78563412);
        assert(honestLength > 0);
        const uint8_t header[] = { 18, 41, 0, 15, 3, 0, 3, 0xff, 0xff };
        size_t at = 0;
        for (size_t i = 0; i + sizeof(header) <= honestLength; ++i)
            if (memcmp(honest + i, header, sizeof(header)) == 0) { at = i; break; }
        assert(at != 0);
        for (size_t i = 0; i < 16; ++i)
            assert(honest[at + 9 + i * 2] == 44 && honest[at + 10 + i * 2] == 0x80);
        size_t sync = 0;
        for (size_t i = kDot11HeaderLength + kActionHeaderLength; i + 3 < honestLength; ++i)
            if (honest[i] == 4 && honest[i + 1] == 73 && honest[i + 2] == 0) { sync = i; break; }
        assert(sync != 0);
        assert(honest[sync + 3] == 44 && honest[sync + 6] == 44);
        assert(honest[sync + 36 + 4] == 0xff && honest[sync + 36 + 5] == 0xff);
        state.setOverrideChannel(0);
        const size_t copyLength = state.composeActionFrame(
            honest, sizeof(honest), source, 0x1280, 0x78563412);
        assert(copyLength > 0);
        at = 0;
        for (size_t i = 0; i + sizeof(header) <= copyLength; ++i)
            if (memcmp(honest + i, header, sizeof(header)) == 0) { at = i; break; }
        // Without the override the follower copies the master (and now keeps
        // the 16-bit 0xffff fill instead of truncating it to 0x00ff).
        assert(at != 0 && honest[at + 9] == 0x30 && honest[at + 10] == 0x80);
    }

    // A second accepted MIF crosses the 32-bit GP2 wrap. The delta remains
    // exactly one 16-TU AW (16,384 microseconds).
    tlv[15] = 0xa7;
    tlv[39] = 0xa7;
    PeerObservation wrappedObservation = state.observe80211(
        mif, sizeof(mif), 48, -35, 0x00003000U, 450);
    assert(wrappedObservation.valid && wrappedObservation.followUpdated);
    assert(state.copyFollowState(&follow));
    assert(follow.masterCounter == 2215);
    assert(follow.rxGp2 == 0x00003000U && follow.rxGp2DeltaValid);
    assert(follow.rxGp2Delta == 16384U);
    assert(follow.nextEawGp2 == 0x0000f000U);

    uint32_t estimatedGp2 = 0;
    uint32_t applyTime = 0;
    assert(TransportState::nextEawApplyTime(
        follow, 450 + 1000000ULL, 8192, &estimatedGp2, &applyTime));
    assert(estimatedGp2 == 0x000033e8U && applyTime == 0x0000f000U);
    // Once the advertised boundary is too close, roll forward exactly one
    // 64-TU EAW while preserving 32-bit wrap behavior.
    assert(TransportState::nextEawApplyTime(
        follow, 450 + 50000000ULL, 8192, &estimatedGp2, &applyTime));
    assert(estimatedGp2 == 0x0000f350U && applyTime == 0x0001f000U);
    assert(!TransportState::nextEawApplyTime(
        follow, 450 + 3000000000ULL, 8192, &estimatedGp2, &applyTime));
    assert(TransportState::phaseError(0x00002000U, 0x00001000U,
                                      64U * 1024U) == 4096);
    assert(TransportState::phaseError(0x00001000U, 0x00002000U,
                                      64U * 1024U) == -4096);
    assert(TransportState::phaseError(0x00001000U, 0xfffff000U,
                                      64U * 1024U) == 8192);

    // Once a master has been learned, composition must advertise this Mac as
    // a distance-one follower while retaining Apple's local election fields.
    uint8_t followerTemplate[3 + 21 + 3 + 40] = {};
    followerTemplate[0] = 5; followerTemplate[1] = 21;
    followerTemplate[3 + 3] = 7; // old election distance
    memset(followerTemplate + 3 + 5, 0x11, 6);
    followerTemplate[3 + 11] = 9; // old master metric
    followerTemplate[3 + 15] = 0x55; // Apple self metric, replaced
    const size_t electionV2 = 3 + 21;
    followerTemplate[electionV2] = 24;
    followerTemplate[electionV2 + 1] = 40;
    memset(followerTemplate + electionV2 + 3, 0x22, 12);
    followerTemplate[electionV2 + 3 + 16] = 7; // distance
    followerTemplate[electionV2 + 3 + 20] = 9; // master metric
    followerTemplate[electionV2 + 3 + 24] = 0x66; // self metric, replaced
    followerTemplate[electionV2 + 3 + 36] = 0x77; // self counter
    assert(state.setTemplate(followerTemplate, sizeof(followerTemplate)));
    rawLength = state.composeActionFrame(raw, sizeof(raw), source,
                                         0x1270, 0x78563412);
    assert(rawLength != 0);
    const size_t templateStart = kDot11HeaderLength + kActionHeaderLength;
    const uint8_t *oldElection = raw + templateStart + 3;
    assert(oldElection[3] == 1);
    assert(memcmp(oldElection + 5, master, sizeof(master)) == 0);
    assert(oldElection[11] == 65 && oldElection[15] == kFollowerSelfMetric);
    const uint8_t *newElection = raw + templateStart + electionV2 + 3;
    assert(memcmp(newElection, master, sizeof(master)) == 0);
    assert(memcmp(newElection + 6, master, sizeof(master)) == 0);
    assert(newElection[12] == 0xa7 && newElection[13] == 0x08);
    assert(newElection[16] == 1 && newElection[20] == 65);
    assert(newElection[24] == kFollowerSelfMetric && newElection[36] == 0x77);

    const size_t followedDynamic = templateStart + sizeof(followerTemplate);
    assert(raw[followedDynamic] == 4);
    assert(raw[followedDynamic + 3] == 48);
    assert(memcmp(raw + followedDynamic + 24, master, sizeof(master)) == 0);
    assert(raw[followedDynamic + 36] == 15);
    assert(raw[followedDynamic + 37] == 3);
    assert(raw[followedDynamic + 42] == 0x30 &&
           raw[followedDynamic + 43] == 0x80);
    const size_t followedChannel = followedDynamic + 76;
    assert(raw[followedChannel] == 18);
    assert(raw[followedChannel + 3] == 15 &&
           raw[followedChannel + 4] == 3);
    assert(raw[followedChannel + 9] == 0x30 &&
           raw[followedChannel + 10] == 0x80);
    assert(state.isAwdlAction(raw, rawLength));

    mif[15] = 0x15; // source is now a follower, while tag 24 keeps the master.
    assert(!state.observe80211(mif, sizeof(mif), 48, -40, 0x7000U, 500).followUpdated);

    // HT Capabilities normalization regression. Apple's real Intel AWDL
    // template carries the tag order 6,24,12,7,17,21,20 with a truncated
    // 5-byte HT Capabilities tag (7). setTemplate must pad it to the canonical
    // 9 bytes so the composed MIF's TLV chain stays in sync end to end -- the
    // defect that made every transmitted MIF "Malformed Packet" to tshark and
    // rejected by the iPhone.
    uint8_t apple[3 + 13 + 3 + 40 + 3 + 47 + 3 + 5 + 3 + 14 + 3 + 2 + 3 + 6];
    memset(apple, 0, sizeof(apple));
    size_t ap = 0;
    apple[ap] = 6;  apple[ap + 1] = 13; ap += 3 + 13;   // Service Parameters
    apple[ap] = 24; apple[ap + 1] = 40;                 // Election v2
    memcpy(apple + ap + 3, master, sizeof(master));
    ap += 3 + 40;
    apple[ap] = 12; apple[ap + 1] = 47; ap += 3 + 47;   // Data Path State
    apple[ap] = 7;  apple[ap + 1] = 5;                  // HT Capabilities (short)
    apple[ap + 3] = 0x00; apple[ap + 4] = 0x00; apple[ap + 5] = 0x62;
    apple[ap + 6] = 0x00; apple[ap + 7] = 0x00;
    ap += 3 + 5;
    apple[ap] = 17; apple[ap + 1] = 14; ap += 3 + 14;   // IEEE802.11 container
    apple[ap] = 21; apple[ap + 1] = 2;  ap += 3 + 2;    // Version
    apple[ap] = 20; apple[ap + 1] = 6;  ap += 3 + 6;
    assert(ap == sizeof(apple));
    assert(state.setTemplate(apple, sizeof(apple)));
    // The stored template grew by exactly four bytes (the HT tag 5 -> 9).
    assert(state.templateLength() == sizeof(apple) + 4);
    uint8_t bigRaw[512];
    rawLength = state.composeActionFrame(bigRaw, sizeof(bigRaw), source,
                                         0x1280, 0x78563412);
    assert(rawLength != 0);
    // Walk the whole composed MIF: every tag must stay in bounds, the chain
    // must consume exactly to the end (no desync), and Sync Parameters (4),
    // Channel Sequence (18), Election v2 (24) and a 9-byte HT Capabilities (7)
    // must all be present and reachable.
    const uint8_t *walkTlvs = bigRaw + kDot11HeaderLength + kActionHeaderLength;
    const size_t walkLength = rawLength - kDot11HeaderLength -
        kActionHeaderLength;
    size_t walk = 0;
    bool sawSync = false, sawSequence = false, sawElectionV2 = false,
         sawCanonicalHt = false;
    while (walk + 3 <= walkLength) {
        const uint8_t type = walkTlvs[walk];
        const size_t length = static_cast<size_t>(walkTlvs[walk + 1]) |
            (static_cast<size_t>(walkTlvs[walk + 2]) << 8);
        assert(length <= walkLength - walk - 3);
        if (type == 4) sawSync = true;
        else if (type == 18) sawSequence = true;
        else if (type == 24) sawElectionV2 = true;
        else if (type == 7) sawCanonicalHt = (length == 9);
        walk += 3 + length;
    }
    assert(walk == walkLength);
    assert(sawSync && sawSequence && sawElectionV2 && sawCanonicalHt);

    // Master-epoch sync fields, checked against the live iPhone master's own
    // MIFs (radiotap TSF us, txcounter TU, AW counter).
    // The anchor is shifted to straddle the 32-bit GP2 wrap.
    static const uint32_t kPhone[][3] = {
        {1159621878U, 48, 773}, {1160039041U, 24, 798},
        {1170046445U, 43, 1409}, {1180545806U, 29, 2050},
        {1188984979U, 43, 2565},
    };
    const uint32_t wrapShift = 0xffff0000U - kPhone[0][0];
    FollowState timed = {};
    timed.valid = true;
    timed.timingValid = true;
    timed.awPeriod = 16;
    timed.presenceMode = 4;
    timed.eawPeriodUs = 64U * 1024U;
    timed.rxGp2 = kPhone[0][0] + wrapShift;
    timed.nextEawGp2 = timed.rxGp2 + kPhone[0][1] * 1024U;
    timed.awSequence = static_cast<uint16_t>(kPhone[0][2]);
    timed.rxHostTimeNs = 5000000000ULL;
    for (size_t i = 0; i < sizeof(kPhone) / sizeof(kPhone[0]); ++i) {
        uint16_t down = 0, sequence = 0;
        assert(TransportState::masterSyncAt(timed, kPhone[i][0] + wrapShift,
                                            &down, &sequence));
        assert(sequence == kPhone[i][2]);
        // A single MIF anchor carries ~1 TU of jitter; stay inside the
        // receiver sync tolerance (OWL AWDL_SYNC_THRESHOLD = 3 TU).
        assert(down + 3 >= kPhone[i][1] && down <= kPhone[i][1] + 3);
    }
    uint16_t unusedDown = 0, unusedSequence = 0;
    assert(!TransportState::masterSyncAt(timed, timed.rxGp2 - 70000U,
                                         &unusedDown, &unusedSequence));
    assert(!TransportState::masterSyncAt(timed, timed.rxGp2 + 31000000U,
                                         &unusedDown, &unusedSequence));
    // Anchor is 16 TU into its EAW. Outside the 16-TU aligned window the
    // frame waits for the next EAW; with a continuous window it leaves now.
    uint32_t txGp2 = 0;
    assert(TransportState::predictTxGp2(timed, timed.rxHostTimeNs, 16U * 1024U,
                                        &txGp2));
    assert(txGp2 == timed.nextEawGp2 + kSyncTxLatencyUs);
    assert(TransportState::predictTxGp2(timed, timed.rxHostTimeNs,
                                        timed.eawPeriodUs, &txGp2));
    assert(txGp2 == timed.rxGp2 + kSyncTxLatencyUs);
    assert(TransportState::predictTxGp2(timed, timed.rxHostTimeNs + 50000000ULL,
                                        16U * 1024U, &txGp2));
    assert(txGp2 == timed.rxGp2 + 50000U + kSyncTxLatencyUs);

    // Service Response TLVs from IO80211's MI frame, in the wire form a real
    // an AirDrop receiver sends: PTR, SRV, TXT for
    // 000000000001._airdrop._tcp, between other tags that must be dropped.
    static const uint8_t kPtr[] = {
        0x02, 24, 0, 0x03, 0x00, 0xc0, 0x07, 0x0c, 0x0f, 0x00, 0x00, 0x00, 0x0c,
        '0', '0', '0', '0', '0', '0', '0', '0',
        '0', '0', '0', '1', 0xc0, 0x00 };
    static const uint8_t kSrv[] = {
        0x02, 35, 0, 0x10, 0x00, 0x0c, '0', '0', '0', '0', '0', '0', '0', '0',
        '0', '0', '0', '1', 0xc0, 0x07, 0x21, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x22, 0x42, 0x04, 't', 'e', 's', 't', 0xc0, 0x0c };
    static const uint8_t kTxt[] = {
        0x02, 32, 0, 0x10, 0x00, 0x0c, '0', '0', '0', '0', '0', '0', '0', '0',
        '0', '0', '0', '1', 0xc0, 0x07, 0x10, 0x0a, 0x00, 0x00, 0x00, 0x09, 'f',
        'l', 'a', 'g', 's', '=', '3', '5', '9' };
    uint8_t miBody[kActionHeaderLength + 160] = {};
    size_t miLength = 0;
    miBody[0] = 127; miBody[1] = 0; miBody[2] = 0x17; miBody[3] = 0xf2;
    miBody[4] = 8; miBody[5] = 0x10; miBody[6] = 3;
    miLength = kActionHeaderLength;
    static const uint8_t kServiceParams[] = { 0x06, 4, 0, 0, 0, 0, 0x19 };
    memcpy(miBody + miLength, kServiceParams, sizeof(kServiceParams));
    miLength += sizeof(kServiceParams);
    memcpy(miBody + miLength, kPtr, sizeof(kPtr)); miLength += sizeof(kPtr);
    memcpy(miBody + miLength, kSrv, sizeof(kSrv)); miLength += sizeof(kSrv);
    memcpy(miBody + miLength, kTxt, sizeof(kTxt)); miLength += sizeof(kTxt);
    static const uint8_t kHt[] = { 0x07, 2, 0, 0x00, 0x00 };
    memcpy(miBody + miLength, kHt, sizeof(kHt)); miLength += sizeof(kHt);
    TransportState services;
    assert(services.setServiceTlvsFromFrame(miBody, miLength) == 3);
    assert(services.serviceTlvsLength() == sizeof(kPtr) + sizeof(kSrv) + sizeof(kTxt));
    // A bare TLV chain (no action header) is accepted as well.
    assert(services.setServiceTlvsFromFrame(miBody + kActionHeaderLength,
                                            miLength - kActionHeaderLength) == 3);
    // A truncated chain is rejected and leaves the kept records unchanged.
    assert(services.setServiceTlvsFromFrame(miBody, miLength - 3) == -1);
    assert(services.serviceTlvsLength() == sizeof(kPtr) + sizeof(kSrv) + sizeof(kTxt));
    // Composed MIFs end with exactly those records.
    assert(services.setTemplate(actionTemplate, sizeof(actionTemplate)));
    uint8_t serviceRaw[1024];
    const size_t serviceRawLength = services.composeActionFrame(
        serviceRaw, sizeof(serviceRaw), source, 0x1230, 0x78563412);
    assert(serviceRawLength != 0);
    const uint8_t *tail = serviceRaw + serviceRawLength - services.serviceTlvsLength();
    assert(memcmp(tail, kPtr, sizeof(kPtr)) == 0);
    assert(memcmp(tail + sizeof(kPtr), kSrv, sizeof(kSrv)) == 0);
    assert(memcmp(tail + sizeof(kPtr) + sizeof(kSrv), kTxt, sizeof(kTxt)) == 0);
    assert(services.isAwdlAction(serviceRaw, serviceRawLength));
    // The host-name TLV (16) that SRV targets refer to is kept as well.
    static const uint8_t kHost[] = { 0x10, 6, 0, 0x04, 'h', 'o', 's', 't', 0x00 };
    uint8_t withHost[sizeof(kHost) + sizeof(kSrv)];
    memcpy(withHost, kHost, sizeof(kHost));
    memcpy(withHost + sizeof(kHost), kSrv, sizeof(kSrv));
    assert(services.setServiceTlvsFromFrame(withHost, sizeof(withHost)) == 2);
    assert(services.serviceTlvsDropped() == 0);
    // A frame without services clears them.
    assert(services.setServiceTlvsFromFrame(kHt, sizeof(kHt)) == 0);
    assert(services.serviceTlvsLength() == 0);

    // Master slot lookup: the iPhone's busy schedule (
    // AW 1090 in slot 0), encoded as channel | flags << 8.
    assert(TransportState::controlChannel(1, 46 | (0x1e << 8)) == 48);
    assert(TransportState::controlChannel(1, 151 | (0x1d << 8)) == 149);
    assert(TransportState::controlChannel(1, 6 | (0x2b << 8)) == 6);
    assert(TransportState::controlChannel(0, 44) == 44);
    FollowState slots = timed;
    slots.awSequence = 1090;
    slots.channelEncoding = 1;
    slots.channelSequenceLength = 16;
    static const uint16_t kBusy[16] = {
        46 | (0x1e << 8), 46 | (0x1e << 8), 151 | (0x1d << 8), 0, 0, 0, 0,
        46 | (0x1e << 8), 6 | (0x2b << 8), 46 | (0x1e << 8), 42 | (0x1e << 8),
        46 | (0x1e << 8), 0, 0, 0, 46 | (0x1e << 8) };
    memcpy(slots.channelSequence, kBusy, sizeof(kBusy));
    // Anchor EAW (AW 1088-1091) is slot 16 % 16 = 0 -> ch 48; next EAW is
    // slot 1 (48), then slot 2 (149), then 3-6 off, then slot 7 (48).
    uint16_t slotChannel = 0;
    uint32_t eawStart = 0;
    assert(TransportState::masterSlotAt(slots, slots.rxGp2, &slotChannel, &eawStart));
    assert(slotChannel == 48 && eawStart == slots.nextEawGp2 - slots.eawPeriodUs);
    assert(TransportState::masterSlotAt(slots, slots.nextEawGp2 + 5, &slotChannel,
                                        &eawStart));
    assert(slotChannel == 48 && eawStart == slots.nextEawGp2);
    assert(TransportState::masterSlotAt(slots, slots.nextEawGp2 + slots.eawPeriodUs + 5,
                                        &slotChannel, &eawStart));
    assert(slotChannel == 149);
    uint32_t nextStart = 0;
    assert(TransportState::nextMasterSlot(slots, slots.nextEawGp2 + slots.eawPeriodUs + 5,
                                          48, &nextStart));
    assert(nextStart == slots.nextEawGp2 + 6 * slots.eawPeriodUs);
    // Any 5 GHz slot: slot 2 (149) follows slot 1 directly.
    assert(TransportState::nextMasterSlot(slots, slots.nextEawGp2 + 5, 0, &nextStart));
    assert(nextStart == slots.nextEawGp2 + slots.eawPeriodUs);
    // Busy schedule: six 48 slots beat one 149 and one 44.
    assert(TransportState::dominantChannel(slots, 44) == 48);
    // Off-LAN schedule (the iPhone moved to 44): mostly 44/48-block lower.
    FollowState offLan = slots;
    for (size_t i = 0; i < 16; ++i)
        offLan.channelSequence[i] = 46 | (0x1d << 8);
    offLan.channelSequence[8] = 6 | (0x2b << 8);
    assert(TransportState::dominantChannel(offLan, 48) == 44);
    FollowState idle = slots;
    memset(idle.channelSequence, 0, sizeof(idle.channelSequence));
    assert(TransportState::dominantChannel(idle, 48) == 0);

    // New GL92 reset boundary: forget peer/GP2 history, retain native payloads.
    const size_t keptTemplate = state.templateLength();
    state.resetRadioHistory();
    assert(state.peerCount() == 0 && !state.hasFollowState());
    assert(state.templateLength() == keptTemplate);

    puts("AwdlTransport tests passed");
    return 0;
}
