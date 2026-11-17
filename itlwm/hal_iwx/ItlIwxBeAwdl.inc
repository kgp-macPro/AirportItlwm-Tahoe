/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP
 * Experimental BE200 GL92 co-channel discovery. Firmware layout/ordering:
 * Intel Linux a38297e3fb01 mvm/mld-mac.c, mld-sta.c, link.c (dual BSD/GPL).
 * No IWM MAC/binding command, infrastructure policy or private DMA shortcut.
 */
#include <Airport/AwdlP9.hpp>
#include <Airport/BeAwdlAvailability.hpp>
#include <Airport/BeAwdlData.hpp>
#include <Airport/BeAwdlDataTransport.hpp>
#include <Airport/BeAwdlInfraIsolation.hpp>

static uint64_t beAwdlNowNs()
{
    uint64_t ns = 0;
    absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
    return ns;
}

bool ItlIwx::beAwdlQueueOwned() const
{
    return beAwdlOptIn && beAwdlSupported() && !fBzFault &&
        fBeAwdlGeneration == com.sc_generation && fBeAwdlEpoch == fPc1ResetEpoch &&
        fBeAwdlQueue > 0 && fBeAwdlQueue < 32 && ((fBzDevice?fBzQueueOwned:fBeAwdlAxQueueOwned) & (1U << fBeAwdlQueue));
}

bool ItlIwx::beAwdlCurrent() const
{
    const auto *sc = &com;
    const auto *ni = sc->sc_ic.ic_bss;
    return beAwdlLifetimeCurrent(const_cast<iwx_softc *>(&com),fBeAwdlGeneration,fBeAwdlEpoch) && beAwdlQueueOwned() && fBeAwdlActive && !fBzFault && !fBeAwdlDetail[6] &&
        fBeAwdlGeneration == sc->sc_generation && fBeAwdlEpoch == fPc1ResetEpoch &&
        !(sc->sc_flags & (IWX_FLAG_SHUTDOWN | IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN)) &&
        sc->sc_ic.ic_state == IEEE80211_S_RUN && ni && ni->ni_chan &&
        ni->ni_chan != IEEE80211_CHAN_ANYC && ieee80211_chan2ieee(const_cast<ieee80211com *>(&sc->sc_ic), ni->ni_chan) == 44 &&
        fBeAwdlQueue > 0 && fBeAwdlQueue < 32 && ((fBzDevice?fBzQueueOwned:fBeAwdlAxQueueOwned) & (1U << fBeAwdlQueue));
}

int ItlIwx::beAwdlPrepare(const uint8_t address[6])
{
    if (!bzDevice()) return beAwdlAxPrepare(address);
    auto *sc = &com;
    auto *ni = sc->sc_ic.ic_bss;
    if (!beAwdlOptIn || !beAwdlSupported() || !getMainWorkLoop()->inGate()) return ENOTSUP;
    if (fBeAwdlDetail[6]) return ECANCELED; // first fatal inhibits AWDL until manual reboot
    if (!address || (address[0] & 1) || !memcmp(address, "\0\0\0\0\0\0", 6)) return EINVAL;
    if (fBeAwdlPreparing || fBeAwdlAPChanging) return EBUSY;
    if (!bzCommandReady(sc) || sc->sc_ic.ic_state != IEEE80211_S_RUN ||
        !(sc->sc_flags & IWX_FLAG_MLD_TXQ_ACTIVE) ||
        (sc->sc_flags&(IWX_FLAG_MAC_ACTIVE|IWX_FLAG_BINDING_ACTIVE|IWX_FLAG_STA_ACTIVE))!=(IWX_FLAG_MAC_ACTIVE|IWX_FLAG_BINDING_ACTIVE|IWX_FLAG_STA_ACTIVE) || !ni || !ni->ni_port_valid || !(ni->ni_associd&0x3fff) ||
        !ni->ni_chan || ni->ni_chan == IEEE80211_CHAN_ANYC || ieee80211_chan2ieee(&sc->sc_ic, ni->ni_chan) != 44 ||
        (sc->sc_flags & (IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN))) return EAGAIN;
    if (!fBeAwdlCleanRemoved && fBeAwdlGeneration == sc->sc_generation && fBeAwdlEpoch == fPc1ResetEpoch) {
        // No repeated context creation, ID reuse or retry after an ambiguous
        // firmware result. Positive device stop owns the eventual cleanup.
        if (memcmp(address, fBeAwdlAddress, 6)) return EBUSY;
        return fBeAwdlActive ? 0 : int(fBeAwdlError ? fBeAwdlError : EIO);
    }
    if (!iwx_use_mld_api(sc) ||
        iwx_lookup_cmd_ver(sc, IWX_MAC_CONF_GROUP, IWX_MAC_CONFIG_CMD) != 2 ||
        iwx_lookup_cmd_ver(sc, IWX_MAC_CONF_GROUP, IWX_LINK_CONFIG_CMD) != 2 ||
        iwx_lookup_cmd_ver(sc, IWX_DATA_PATH_GROUP, IWX_SCD_QUEUE_CONFIG_CMD) != 3)
        return ENOTSUP;
    auto *in = reinterpret_cast<iwx_node *>(ni);
    if (!in->in_phyctxt) return EAGAIN;
    const int generation = sc->sc_generation;
    const uint32_t epoch = fPc1ResetEpoch;
    const bool resumeClean=fBeAwdlCleanRemoved;
    fBeAwdlCleanRemoved=false;fBeAwdlHomeWidth=ni->ni_chw;
    fBeAwdlPhy = in->in_phyctxt->id;
    if(fBeAwdlPhy!=0)return ENOTSUP;
    memcpy(fBeAwdlApBssid,ni->ni_bssid,6);
    fBeAwdlTxAntenna=sc->sc_mgmt_last_antenna_idx;
    ieee80211_ref_node(ni);
    fBeAwdlPreparing = true; fBeAwdlActive = false;
    fBeAwdlGeneration = generation; fBeAwdlEpoch = epoch;
    fBeAwdlQueue = -1; fBeAwdlStage = 1; fBeAwdlError = 0;
    fBeAwdlCount = fBeAwdlHead = fBeAwdlTail = 0;
    fBeAwdlDataCount=fBeAwdlDataHead=fBeAwdlDataTail=0;
    fBeAwdlGp2Host = 0; fBeAwdlGp2 = 0; // never reuse a previous context clock
    fBeAwdlWindowStats[1] = 0;
    for (unsigned i=10;i<=12;++i) fBeAwdlWindowStats[i]=0;
    for (unsigned i=16;i<=26;++i) fBeAwdlWindowStats[i]=0;
    memset(fBeAwdlNativeTickets,0,sizeof(fBeAwdlNativeTickets));
    if(!resumeClean)fBeAwdlActionResults.clear(); // same-epoch clean reactivation must retain owed native CPU results
    memcpy(fBeAwdlAddress, address, 6);
    struct iwx_mac_config_cmd mac = {};
    struct iwx_link_config_cmd link = {};
    struct iwx_sta_cfg_cmd station = {};
    int err = 0, queue = -1;
    // Each command can sleep/reenter the workloop. Revalidate the lifetime
    // after every return, and never continue a partial old-generation setup.
#define AWDL_SEND(phase, group, id, value) do { \
    beAwdlEvent(1, phase, iwx_cmd_id(id, group, 0)); \
    err = iwx_send_cmd_pdu(sc, iwx_cmd_id(id, group, 0), 0, sizeof(value), &(value)); \
    if (!err && (!beAwdlLifetimeCurrent(sc, generation, epoch) || sc->sc_ic.ic_bss != ni || sc->sc_ic.ic_state != IEEE80211_S_RUN)) err = ENXIO; \
    beAwdlEvent(2, phase, iwx_cmd_id(id, group, 0), err); \
    if (err) goto done; \
} while (0)
    // Intel sf.c explicitly excludes P2P_DEVICE, including multi-MAC setup.
    // Leave the established infrastructure Smart FIFO policy untouched.
    mac.id_and_color = htole32(BeAwdlMac);
    mac.action = htole32(IWX_FW_CTXT_ACTION_ADD);
    mac.mac_type = htole32(IWX_FW_MAC_TYPE_P2P_DEVICE);
    memcpy(mac.local_mld_addr, address, 6);
    mac.filter_flags = htole32(IWX_MAC_CFG_FILTER_ACCEPT_CONTROL_AND_MGMT);
    // P2P discovery has no negotiated HE/EHT/TLC or unicast-data ACK promise.
    // Zeroed client union also leaves is_disc_extended zero, as Linux does.
    AWDL_SEND(1, IWX_MAC_CONF_GROUP, IWX_MAC_CONFIG_CMD, mac);
    fBeAwdlStage = 2;
    link.action = htole32(IWX_FW_CTXT_ACTION_ADD);
    link.link_id = htole32(BeAwdlLink); link.mac_id = htole32(BeAwdlMac);
    link.phy_id = htole32(IWX_FW_CTXT_INVALID);
    memcpy(link.local_link_addr, address, 6);
    AWDL_SEND(2, IWX_MAC_CONF_GROUP, IWX_LINK_CONFIG_CMD, link);
    fBeAwdlStage = 3;
    link.action = htole32(IWX_FW_CTXT_ACTION_MODIFY);
    link.phy_id = htole32(fBeAwdlPhy);
    // mac-cfg.h: PHY assignment is ignored when the same command activates
    // the link. Bind while inactive FIRST, as mld_roc_link does, then activate.
    AWDL_SEND(3, IWX_MAC_CONF_GROUP, IWX_LINK_CONFIG_CMD, link);
    link.modify_mask = htole32(IWX_LINK_CONTEXT_MODIFY_ACTIVE | IWX_LINK_CONTEXT_MODIFY_RATES_INFO);
    link.active = htole32(1); link.ofdm_rates = htole32(0x15); // 6/12/24 Mbps basic OFDM
    AWDL_SEND(4, IWX_MAC_CONF_GROUP, IWX_LINK_CONFIG_CMD, link);
    fBeAwdlStage = 4;
    station.sta_id = htole32(BeAwdlStation); station.link_id = htole32(BeAwdlLink);
    station.station_type = htole32(IWX_STATION_TYPE_BCAST_MGMT);
    memset(station.peer_mld_address, 0xff, 6); memset(station.peer_link_address, 0xff, 6);
    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_STA_EXP_MFP_SUPPORT)) station.mfp = htole32(1);
    AWDL_SEND(5, IWX_MAC_CONF_GROUP, IWX_STA_CONFIG_CMD, station);
    fBeAwdlStage = 5;
    beAwdlEvent(1, 6, iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD, IWX_DATA_PATH_GROUP, 0));
    queue = iwx_tvqm_enable_txq(sc, IWX_MGMT_TID, 0, beAwdlQueueSize(), BeAwdlStation);
    beAwdlEvent(2, 6, iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD, IWX_DATA_PATH_GROUP, 0), queue < 0 ? -queue : 0);
    if (queue < 0) { err = -queue; goto done; }
    if (!beAwdlLifetimeCurrent(sc, generation, epoch)) { err = ENXIO; goto done; }
    fBeAwdlQueue = queue; fBeAwdlStage = 6; fBeAwdlActive = true;
done:
    ieee80211_release_node(&sc->sc_ic, ni);
    fBeAwdlError = err; fBeAwdlPreparing = false;beAwdlControlDone();
    // No unproven queue flush, early DMA release or infrastructure reset.
    // Partial contexts/ring backing remain bounded until normal positive stop.
    return err;
#undef AWDL_SEND
}

int ItlIwx::beAwdlTransmit(mbuf_t m,uint64_t nativeTicket)
{
    if (!m) return EINVAL;
    if (!getMainWorkLoop()->inGate() || !beAwdlCurrent() ||
        !be_awdl::txWindow(fBeAwdlWindowStats[1],fBeAwdlWindowStats[10],fBeAwdlWindowStats[12],beAwdlNowNs()) ||
        awdl_p9::classify(static_cast<const uint8_t *>(mbuf_data(m)), mbuf_len(m)) != awdl_p9::Action ||
        ((static_cast<const uint8_t *>(mbuf_data(m))[4]&1) ?
          memcmp(static_cast<const uint8_t *>(mbuf_data(m))+4,"\xff\xff\xff\xff\xff\xff",6)!=0 :
          !(nativeTicket&(1ULL<<63)))) {
        mbuf_freem(m); ++fBeAwdlCounters[3]; return EAGAIN;
    }
    auto *ring = &com.txq[fBeAwdlQueue];
    if (ring->queued >= ring->ring_count / 2) { mbuf_freem(m); ++fBeAwdlCounters[3]; return EBUSY; }
    const unsigned index=ring->cur&(ring->ring_count-1);
    if(fBeAwdlNativeTickets[index]) {mbuf_freem(m);++fBeAwdlCounters[3];return EBUSY;}
    auto *ni = com.sc_ic.ic_bss;
    ieee80211_ref_node(ni);
    fBeAwdlNativeTickets[index]=nativeTicket;
    const auto evidence=fBeAwdlWire.record(be_awdl::WireEvidence::TxAction,mbuf_data(m),mbuf_len(m),
        beAwdlNowNs(),fBeAwdlGp2,44,com.sc_generation,fPc1ResetEpoch);
    int err = iwx_tx(&com, m, ni, EDCA_AC_BE, fBeAwdlQueue);
    fBeAwdlWire.complete(evidence,uint32_t(err),1); // host enqueue result, NOT RF/ACK
    if (err) fBeAwdlNativeTickets[index]=0;
    else if (nativeTicket) ++fBeAwdlNativeStatus[3];
    if (err) ieee80211_release_node(&com.sc_ic, ni); // TX consumes m on errors, not ni
    ++fBeAwdlCounters[err ? 3 : 2];
    return err; // enqueue result, NOT a positive RF/peer-ACK claim
}

bool ItlIwx::beAwdlObserve(const void *data, size_t length, uint16_t channel,
                           int32_t rssi, uint32_t gp2, bool hardwareDeaggregated)
{
    if (!beAwdlOptIn || !getMainWorkLoop()->inGate()) return false;
    const uint64_t now = beAwdlNowNs();
    const bool current = beAwdlCurrent();
    const auto *raw=static_cast<const uint8_t *>(data);
    // Native43's infra beacon field is cumulative. Count only actual valid
    // RX frames from the associated BSSID (BZ validates RX before this hook).
    // This is host-observed telemetry, not all on-air/FW-filtered beacons.
    // No synthetic increments at a window event, no infrastructure mutation.
    if (current && raw && length>=36 && (raw[0]&0xfc)==0x80 &&
        !(raw[1]&3) && com.sc_ic.ic_state==IEEE80211_S_RUN && com.sc_ic.ic_bss &&
        !memcmp(raw+16,com.sc_ic.ic_bss->ni_bssid,6) &&
        !memcmp(raw+10,com.sc_ic.ic_bss->ni_bssid,6) &&
        ((raw[4]&1) || !memcmp(raw+4,com.sc_ic.ic_myaddr,6))) ++fBeAwdlObservedInfraBeacons;
    if(current && channel==44 && raw && length>=24 && (raw[0]&0x0c)==8 && !(raw[1]&0x43)) {
        const auto e=fBeAwdlWire.record(be_awdl::WireEvidence::RxNoDs,data,length,now,gp2,channel,com.sc_generation,fPc1ResetEpoch);
        fBeAwdlWire.complete(e,be_awdl::dataHeader(raw,length,hardwareDeaggregated)?1:0,2); // raw bytes; recognition only
    }
    if (current && channel == 44) { fBeAwdlGp2 = gp2; fBeAwdlGp2Host = now; }
    if (current && channel==44 && length<=sizeof(fBeAwdlDataFrames[0].bytes) &&
        be_awdl::dataHeader(raw,length,hardwareDeaggregated)) {
        if (fBeAwdlDataCount==8) {++fBeAwdlDataOverflow;return true;}
        auto &frame=fBeAwdlDataFrames[fBeAwdlDataHead];
        frame.length=uint16_t(length);frame.channel=channel;frame.rssi=rssi;
        frame.gp2=gp2;frame.hostNs=now;
        if (!be_awdl::copyReceivedData(frame.bytes,sizeof(frame.bytes),raw,length,hardwareDeaggregated)) return false;
        fBeAwdlDataHead=(fBeAwdlDataHead+1)%8;++fBeAwdlDataCount;++fBeAwdlDataCopied;
        return true; // private AWDL domain; caller releases only the original mbuf
    }
    if (!current || channel != 44 || length > sizeof(fBeAwdlFrames[0].bytes) ||
        awdl_p9::classify(static_cast<const uint8_t *>(data), length) != awdl_p9::Action) return false;
    const auto e=fBeAwdlWire.record(be_awdl::WireEvidence::RxAction,data,length,now,gp2,channel,com.sc_generation,fPc1ResetEpoch);
    fBeAwdlWire.complete(e,0,2); // firmware-delivered CPU bytes, not native acceptance
    if (fBeAwdlCount == 4) { ++fBeAwdlCounters[1]; return true; }
    auto &frame = fBeAwdlFrames[fBeAwdlHead];
    frame.length = uint16_t(length); frame.channel = channel; frame.rssi = rssi;
    frame.gp2 = gp2; frame.hostNs = now; memcpy(frame.bytes, data, length);
    fBeAwdlHead = (fBeAwdlHead + 1) % 4; ++fBeAwdlCount; ++fBeAwdlCounters[0];
    return true;
}

bool ItlIwx::beAwdlPop(BeAwdlFrame &frame)
{
    if (!getMainWorkLoop()->inGate() || !beAwdlCurrent() || !fBeAwdlCount) return false;
    frame = fBeAwdlFrames[fBeAwdlTail];
    fBeAwdlTail = (fBeAwdlTail + 1) % 4; --fBeAwdlCount; return true;
}
bool ItlIwx::beAwdlPopData(BeAwdlFrame &frame)
{
    if (!getMainWorkLoop()->inGate() || !beAwdlCurrent() || !fBeAwdlDataCount) return false;
    frame=fBeAwdlDataFrames[fBeAwdlDataTail];
    fBeAwdlDataTail=(fBeAwdlDataTail+1)%8;--fBeAwdlDataCount;return true;
}
bool ItlIwx::beAwdlClock(uint32_t &gp2)
{
    if (!getMainWorkLoop()->inGate() || !beAwdlCurrent()) return false;
    // R1 read0xA0206C returned a nonprogressing zero through295 TXs.
    // No guessed replacement register. Use only a fresh, same-context local
    // firmware RX-descriptor GP2, plus bounded host monotonic elapsed time.
    const uint64_t now = beAwdlNowNs();
    fBeAwdlClockStats[3] = fBeAwdlGp2;
    fBeAwdlClockStats[5] = fBeAwdlGp2Host;
    fBeAwdlClockStats[6] = now;
    fBeAwdlClockStats[7] = fBeAwdlGp2Host && now >= fBeAwdlGp2Host ? now - fBeAwdlGp2Host : UINT64_MAX;
    if (!awdl_p9::estimateGp2(fBeAwdlGp2, fBeAwdlGp2Host, now, gp2)) {
        ++fBeAwdlClockStats[2]; return false;
    }
    ++fBeAwdlClockStats[1]; fBeAwdlClockStats[4] = gp2;
    return true; // estimated host-submit time, NOT measured on-air transmit time
}
void ItlIwx::beAwdlClockSnapshot(uint64_t out[8]) const
{
    memcpy(out, fBeAwdlClockStats, sizeof(fBeAwdlClockStats));
}

int ItlIwx::beAwdlWindowRequest(uint32_t durationTu)
{
    auto *sc=&com;
    if (!getMainWorkLoop()->inGate() || !beAwdlCurrent() || fBeAwdlPreparing ||
        fBeAwdlWindowCommand || fBeAwdlDataPreparing) return EBUSY;
    if (!durationTu || durationTu>128) return EINVAL;
    if (fBeAwdlWindowStats[1]) return fBeAwdlWindowStats[1]==4 ? ECANCELED : EBUSY;
    const unsigned cv=iwx_lookup_cmd_ver(sc,IWX_MAC_CONF_GROUP,IWX_SESSION_PROTECTION_CMD);
    const unsigned nv=iwx_lookup_notif_ver(sc,IWX_MAC_CONF_GROUP,IWX_SESSION_PROTECTION_NOTIF);
    fBeAwdlWindowStats[27]=cv; fBeAwdlWindowStats[28]=nv;
    // Exact GL92 TLV48 and Intel mvm API: cmdv2/link ID, notifv3/link ID.
    if (bzDevice()?(cv!=2 || nv!=3):(cv!=1 || nv!=2)) return ENOTSUP;
    uint32_t nowGp2;
    if (!beAwdlClock(nowGp2)) return EAGAIN; // no clockless native window
    struct iwx_session_prot_cmd cmd={};
    cmd.id_and_color=htole32(BeAwdlLink);
    cmd.action=htole32(IWX_FW_CTXT_ACTION_ADD);
    cmd.conf_id=htole32(IWX_SESSION_PROTECT_CONF_P2P_DEVICE_DISCOV);
    cmd.duration_tu=htole32(durationTu);
    const int generation=sc->sc_generation; const uint32_t epoch=fPc1ResetEpoch;
    fBeAwdlWindowStats[1]=1; ++fBeAwdlWindowStats[2]; ++fBeAwdlWindowStats[15];
    fBeAwdlWindowStats[30]=beAwdlNowNs();
    fBeAwdlWindowStats[12]=durationTu; fBeAwdlWindowStats[22]=nowGp2;
    fBeAwdlWindowStats[10]=fBeAwdlWindowStats[11]=0;
    fBeAwdlWindowStats[25]=fBeAwdlWindowStats[26]=0;
    fBeAwdlWindowCommand=true; // start may arrive during the synchronous send
    beAwdlWindowEvent(1,durationTu,cv,nv);
    const int err=iwx_send_cmd_pdu(sc,iwx_cmd_id(IWX_SESSION_PROTECTION_CMD,IWX_MAC_CONF_GROUP,0),0,sizeof(cmd),&cmd);
    fBeAwdlWindowCommand=false;
    beAwdlWindowEvent(2,uint32_t(err));
    // Command ownership and temporary transmit readiness are distinct. A
    // BGSCAN flag during a successful send is not an ambiguous firmware ACK.
    if (err || !beAwdlLifetimeCurrent(sc,generation,epoch) || !beAwdlQueueOwned() || fBeAwdlDetail[6]) {
        ++fBeAwdlWindowStats[4]; fBeAwdlWindowStats[1]=4;
        beAwdlWindowEvent(7,1,uint32_t(err));
        return err ? err : ENXIO; // ambiguous result: no replacement/retry in this context
    }
    ++fBeAwdlWindowStats[3]; return 0; // accepted command, NOT window-start
}

int ItlIwx::beAwdlWindowCancel()
{
    auto *sc=&com;
    if (!beAwdlOptIn || !beAwdlSupported()) return 0;
    if (!getMainWorkLoop()->inGate() || fBeAwdlWindowCommand) return EBUSY;
    if (!fBeAwdlWindowStats[1]) return 0;
    if (fBeAwdlGeneration!=sc->sc_generation || fBeAwdlEpoch!=fPc1ResetEpoch) return ENXIO;
    if (fBeAwdlWindowStats[1]==5) return 0; // accepted REMOVE, not a natural END
    if (fBeAwdlWindowStats[1]==4) return ECANCELED;
    struct iwx_session_prot_cmd cmd={};
    cmd.id_and_color=htole32(BeAwdlLink); cmd.action=htole32(IWX_FW_CTXT_ACTION_REMOVE);
    cmd.conf_id=htole32(IWX_SESSION_PROTECT_CONF_P2P_DEVICE_DISCOV);
    const int generation=sc->sc_generation; const uint32_t epoch=fPc1ResetEpoch;
    fBeAwdlWindowStats[1]=3; fBeAwdlWindowCommand=true;
    beAwdlWindowEvent(5);
    const int err=iwx_send_cmd_pdu(sc,iwx_cmd_id(IWX_SESSION_PROTECTION_CMD,IWX_MAC_CONF_GROUP,0),0,sizeof(cmd),&cmd);
    fBeAwdlWindowCommand=false; ++fBeAwdlWindowStats[29];
    beAwdlWindowEvent(6,uint32_t(err));
    if (err || !beAwdlLifetimeCurrent(sc,generation,epoch)) {
        ++fBeAwdlWindowStats[4]; fBeAwdlWindowStats[1]=4;
        beAwdlWindowEvent(7,2,uint32_t(err)); return err ? err : ENXIO;
    }
    // Intel time-event.c stop_roc/remove_time_event explicitly retires ROC
    // after REMOVE; it does not wait for another END. Keep a terminal state,
    // not idle: late replies cannot be attributed to a new same-ID session.
    // No synthetic end/start counter and no queue/credit/backing retirement.
    fBeAwdlWindowStats[1]=5;
    return 0;
}

bool ItlIwx::beAwdlWindowNotify(const void *bytes,size_t length)
{
    if (!beAwdlOptIn || !beAwdlSupported()) return false;
    if (length!=sizeof(iwx_session_prot_notif)) {
        ++fBeAwdlWindowStats[8];
        if (fBeAwdlWindowStats[1]) fBeAwdlWindowStats[1]=4;
        beAwdlWindowEvent(fBeAwdlWindowStats[1]==4 ? 7 : 11,3,length);
        return true; // reject short/mismatched payload before shared dereference
    }
    struct iwx_session_prot_notif n; memcpy(&n,bytes,sizeof(n));
    const uint32_t link=le32toh(n.mac_id), conf=le32toh(n.conf_id);
    if (link!=BeAwdlLink || conf!=IWX_SESSION_PROTECT_CONF_P2P_DEVICE_DISCOV) return false;
    fBeAwdlWindowStats[20]=link; fBeAwdlWindowStats[21]=conf;
    const uint32_t status=le32toh(n.status), start=le32toh(n.start);
    fBeAwdlWindowStats[18]=status; fBeAwdlWindowStats[19]=start;
    if (fBeAwdlWindowStats[1]==5) {
        ++fBeAwdlWindowStats[9]; beAwdlWindowEvent(9,status,start);
        return true; // real late reply, not a new session's completion
    }
    if (!getMainWorkLoop()->inGate() || fBeAwdlGeneration!=com.sc_generation ||
        fBeAwdlEpoch!=fPc1ResetEpoch || !fBeAwdlWindowStats[15]) {
        ++fBeAwdlWindowStats[9]; return true;
    }
    if (status>1 || start>1 || !fBeAwdlWindowStats[1] || fBeAwdlWindowStats[1]==4) {
        ++fBeAwdlWindowStats[8]; fBeAwdlWindowStats[1]=4;
        beAwdlWindowEvent(7,4,status,start); return true;
    }
    uint32_t gp2=0; const bool clock=beAwdlClock(gp2);
    if (start) {
        if (fBeAwdlWindowStats[16]==fBeAwdlWindowStats[15]) {
            ++fBeAwdlWindowStats[8]; fBeAwdlWindowStats[1]=4;
            beAwdlWindowEvent(7,5,status,start); return true;
        }
        // These counters describe actual notifications, not successful grants.
        ++fBeAwdlWindowStats[5]; fBeAwdlWindowStats[10]=beAwdlNowNs();
        fBeAwdlWindowStats[16]=fBeAwdlWindowStats[15];
        fBeAwdlWindowStats[23]=gp2; fBeAwdlWindowStats[25]=clock;
        if (!status) {
            ++fBeAwdlWindowStats[7];
            // Firmware contract: even an unscheduled session produces START
            // and END. Never grant TX/native41 or reuse the ID at failed START.
            // Drain its real END; missing END retains the existing timeout lock.
            if(fBeAwdlWindowStats[1]!=3)fBeAwdlWindowStats[1]=6;
            beAwdlWindowEvent(17,status,clock,gp2);return true;
        }
        if (fBeAwdlWindowStats[1]!=3) fBeAwdlWindowStats[1]=2;
        beAwdlWindowEvent(3,status,clock,gp2);
    } else {
        if(fBeAwdlWindowStats[16]!=fBeAwdlWindowStats[15]) {
            ++fBeAwdlWindowStats[8];fBeAwdlWindowStats[1]=4;
            beAwdlWindowEvent(7,6,status,start);return true;
        }
        ++fBeAwdlWindowStats[6]; fBeAwdlWindowStats[11]=beAwdlNowNs();
        fBeAwdlWindowStats[17]=fBeAwdlWindowStats[15];
        fBeAwdlWindowStats[24]=gp2; fBeAwdlWindowStats[26]=clock;
        // A real terminal END retires this finite session, including a rejected
        // or interrupted protection. It does NOT retire TX/DMA or mean packet
        // success. Only a subsequent positive START can reopen TX admission.
        if(!status){++fBeAwdlWindowStats[7];beAwdlWindowEvent(18,status,clock,gp2);}
        fBeAwdlWindowStats[1]=0;
        getMainCommandGate()->commandWakeup(&fBeAwdlWindowStats[1]);
        beAwdlWindowEvent(4,status,clock,gp2);
    }
    // Do not touch infrastructure IWX_FLAG_TE_ACTIVE, call native code from
    // completion context, or retire/free any DMA/queue backing here.
    return true;
}
void ItlIwx::beAwdlWindowPoll()
{
    if (!beAwdlOptIn || !beAwdlSupported() || !getMainWorkLoop()->inGate() ||
        fBeAwdlWindowCommand || !fBeAwdlWindowStats[1] || fBeAwdlWindowStats[1]==4 ||
        fBeAwdlWindowStats[1]==5) return;
    if(fBeAwdlGeneration!=com.sc_generation || fBeAwdlEpoch!=fPc1ResetEpoch){
        // Missing END across an invalidated radio lifetime is still adverse,
        // but must not later be mislabeled as a software admission deadline.
        fBeAwdlWindowStats[1]=4;beAwdlWindowEvent(7,12,uint32_t(fBeAwdlGeneration),fBeAwdlEpoch);return;
    }
    const uint64_t now=beAwdlNowNs();
    const bool active=fBeAwdlWindowStats[1]==2;
    const uint64_t origin=active ? fBeAwdlWindowStats[10] : fBeAwdlWindowStats[30];
    const uint64_t budget=2000000000ULL+(active ? fBeAwdlWindowStats[12]*1024000ULL : 0);
    if (origin && (now<origin || now-origin>budget)) {
        // Software admission deadline, NOT proof of a firmware timeout. Do not
        // replace an ambiguous session, free credits or reset infrastructure.
        fBeAwdlWindowStats[1]=4; ++fBeAwdlWindowStats[31];
        beAwdlWindowEvent(7,7,active,now>=origin ? now-origin : UINT64_MAX);
    }
}
void ItlIwx::beAwdlWindowEvent(uint32_t kind,uint64_t a,uint64_t b,uint64_t c)
{
    if (!beAwdlOptIn || !beAwdlSupported()) return;
    if (!getMainWorkLoop()->inGate()) {
        __atomic_fetch_add(&fBeAwdlWindowTrace[7],1ULL,__ATOMIC_RELAXED); return;
    }
    // The original203 ring remains immutable after its first fault. It may
    // belong to a previous firmware lifetime;232 keeps live transitions plus
    // the current lifetime's first adverse event, with explicit overwrite.
    auto *h=fBeAwdlWindowLife;
    if(h[5]!=uint32_t(com.sc_generation) || h[6]!=fPc1ResetEpoch) {
        memset(h+10,0,14*sizeof(*h));h[5]=uint32_t(com.sc_generation);h[6]=fPc1ResetEpoch;
    }
    const uint64_t live=++h[3];if(live>32)++h[4];
    auto *r=h+32+12*((live-1)%32);r[0]=live;r[1]=beAwdlNowNs();
    r[2]=kind|(uint64_t(uint32_t(com.sc_generation))<<32);
    r[3]=fPc1ResetEpoch|(fBeAwdlWindowStats[1]<<32);r[4]=fBeAwdlWindowStats[15];
    r[5]=a;r[6]=b;r[7]=c;r[8]=fBeAwdlWindowStats[30];r[9]=fBeAwdlWindowStats[10];
    r[10]=44;r[11]=44;
    h[24]=uint32_t(fBeAwdlGeneration);h[25]=fBeAwdlEpoch;
    if(kind==7 && !h[12]){h[10]=com.sc_generation;h[11]=fPc1ResetEpoch;h[12]=r[1];
        h[13]=r[4];h[14]=a;h[15]=b;h[16]=c;h[17]=fBeAwdlWindowStats[1];}
    if(kind==17 || kind==18) {
        ++h[kind==17?7:8];if(!h[18]){h[18]=r[1];h[19]=r[4];h[20]=kind;h[21]=a;h[22]=b;h[23]=c;}
    }
    if (fBeAwdlWindowTrace[6]) return;
    const uint64_t serial=++fBeAwdlWindowTrace[3];
    fBeAwdlWindowTrace[4]=serial<32 ? serial : 32;
    fBeAwdlWindowTrace[5]=serial>32 ? serial-32 : 0;
    fBeAwdlWindowTrace[8]=uint32_t(com.sc_generation); fBeAwdlWindowTrace[9]=fPc1ResetEpoch;
    uint64_t *e=fBeAwdlWindowTrace+16+((serial-1)%32)*8;
    e[0]=serial; e[1]=beAwdlNowNs(); e[2]=kind|(uint64_t(uint32_t(com.sc_generation))<<32);
    e[3]=fPc1ResetEpoch|(fBeAwdlWindowStats[1]<<32); e[4]=fBeAwdlWindowStats[15];
    e[5]=a; e[6]=b; e[7]=c;
    if (kind==7) fBeAwdlWindowTrace[6]=1; // retain first failure's preceding32events
}
void ItlIwx::beAwdlWindowTraceSnapshot(uint64_t out[272]) const
{
    // Header7 alone can be incremented by an out-of-gate rejected writer.
    // Do not mix its atomic writes with a non-atomic memcpy read.
    memcpy(out,fBeAwdlWindowTrace,7*sizeof(uint64_t));
    out[7]=__atomic_load_n(&fBeAwdlWindowTrace[7],__ATOMIC_RELAXED);
    memcpy(out+8,fBeAwdlWindowTrace+8,(272-8)*sizeof(uint64_t));
}
void ItlIwx::beAwdlWindowSnapshot(uint64_t out[32]) const
{
    memcpy(out,fBeAwdlWindowStats,sizeof(fBeAwdlWindowStats));
    out[13]=uint32_t(fBeAwdlGeneration); out[14]=fBeAwdlEpoch;
}

void ItlIwx::beAwdlSnapshot(uint64_t out[16]) const
{
    memset(out, 0, 16 * sizeof(uint64_t));
    out[0] = 1; out[1] = beAwdlOptIn; out[2] = fBeAwdlStage; out[3] = fBeAwdlError;
    out[4] = fBeAwdlGeneration; out[5] = fBeAwdlEpoch; out[6] = uint32_t(fBeAwdlQueue);
    out[7] = beAwdlCurrent();
    for (unsigned i = 0; i < 8; ++i) out[8+i] = fBeAwdlCounters[i];
}

int ItlIwx::beAwdlTeardown()
{
    if(!bzDevice())return beAwdlAxStop(false);
    if (beAwdlOptIn) {const int dataError=beAwdlDataTeardown();if(dataError)return dataError;}
    auto *sc = &com;
    if (!beAwdlOptIn || fBeAwdlGeneration != sc->sc_generation ||
        fBeAwdlEpoch != fPc1ResetEpoch || fBeAwdlStage == 7 || fBeAwdlStage == 0) return 0;
    if (!getMainWorkLoop()->inGate() || fBeAwdlPreparing) return EBUSY;
    if (fBeAwdlWindowCommand) return EBUSY;
    beAwdlEvent(5, 0, 0);
    fBeAwdlActive = false; fBeAwdlPreparing = true; // close new submissions first
    const int generation = sc->sc_generation;
    const uint32_t epoch = fPc1ResetEpoch;
    int err = fBeAwdlError;
    struct iwx_link_config_cmd link = {};
    struct iwx_mac_config_cmd mac = {};
    struct iwx_sta_remove_cmd station = {};
    if (err) goto done; // unknown prior command outcome requires normal reset
    err=beAwdlWindowCancel();
    if (err) goto done; // normal positive device stop owns ambiguous backing
#define AWDL_REMOVE(phase, group, id, value) do { \
    beAwdlEvent(1, phase, iwx_cmd_id(id, group, 0)); \
    err = iwx_send_cmd_pdu(sc, iwx_cmd_id(id, group, 0), 0, sizeof(value), &(value)); \
    if (!err && !beAwdlLifetimeCurrent(sc, generation, epoch)) err = ENXIO; \
    beAwdlEvent(2, phase, iwx_cmd_id(id, group, 0), err); \
    if (err) goto done; \
} while (0)
    if (fBeAwdlStage >= 6) {
        beAwdlEvent(1, 7, 0);
        err = iwx_wait_txq_empty(sc, fBeAwdlQueue);
        beAwdlEvent(2, 7, 0, err);
        if (err) goto done;
        beAwdlEvent(1, 8, iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD, IWX_DATA_PATH_GROUP, 0));
        err = iwx_mld_disable_txq(sc, BeAwdlStation, IWX_MGMT_TID);
        beAwdlEvent(2, 8, iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD, IWX_DATA_PATH_GROUP, 0), err);
        if (!err && !beAwdlLifetimeCurrent(sc, generation, epoch)) err = ENXIO;
        if (err) goto done;
    }
    if (fBeAwdlStage >= 5) {
        station.sta_id = htole32(BeAwdlStation);
        AWDL_REMOVE(9, IWX_MAC_CONF_GROUP, IWX_STA_REMOVE_CMD, station);
    }
    if (fBeAwdlStage >= 3) {
        link.link_id = htole32(BeAwdlLink); link.mac_id = htole32(BeAwdlMac);
        link.phy_id = htole32(fBeAwdlPhy);
        link.action = htole32(IWX_FW_CTXT_ACTION_MODIFY);
        link.modify_mask = htole32(IWX_LINK_CONTEXT_MODIFY_ACTIVE);
        AWDL_REMOVE(10, IWX_MAC_CONF_GROUP, IWX_LINK_CONFIG_CMD, link);
        link.action = htole32(IWX_FW_CTXT_ACTION_REMOVE);
        AWDL_REMOVE(11, IWX_MAC_CONF_GROUP, IWX_LINK_CONFIG_CMD, link);
    }
    if (fBeAwdlStage >= 2) {
        mac.id_and_color = htole32(BeAwdlMac); mac.action = htole32(IWX_FW_CTXT_ACTION_REMOVE);
        AWDL_REMOVE(12, IWX_MAC_CONF_GROUP, IWX_MAC_CONFIG_CMD, mac);
    }
    err=beAwdlRetireDiscoveryQueue();if(err)goto done;
    fBeAwdlStage = 7; fBeAwdlCount = 0;fBeAwdlCleanRemoved=true;
    // Queue memory remains in the existing ownership ledger until positive
    // device stop. Do not recycle IDs/backing merely because a remove ACK arrived.
done:
    beAwdlEvent(6, 0, 0, err);
    fBeAwdlError = err; fBeAwdlPreparing = false;beAwdlControlDone(); return err;
#undef AWDL_REMOVE
}

// This is not a packet trace. All access is serialized by the pre-existing
// controller workloop (including bzInterrupt); no filter-side allocation/read.
void ItlIwx::beAwdlEvent(uint32_t kind, uint32_t phase, uint32_t code, int error)
{
    if (!beAwdlOptIn || !beAwdlSupported() || !getMainWorkLoop()->inGate() || fBeAwdlDetail[7]) return;
    const uint64_t ticket = ++fBeAwdlDetail[3];
    auto *event = &fBeAwdlDetail[32 + ((ticket - 1) % 32) * 8];
    event[0] = ticket; event[1] = beAwdlNowNs();
    event[2] = com.sc_generation; event[3] = fPc1ResetEpoch;
    event[4] = (uint64_t(phase) << 32) | kind;
    event[5] = code; event[6] = uint32_t(error); event[7] = fBeAwdlStage;
    fBeAwdlDetail[4] = ticket < 32 ? ticket : 32;
    fBeAwdlDetail[5] = ticket > 32 ? ticket - 32 : 0;
    if (kind == 1) { fBeAwdlDetail[28] = phase; fBeAwdlDetail[29] = code; }
}

void ItlIwx::beAwdlHostGate(uint64_t bits)
{
    if (!beAwdlOptIn || !beAwdlSupported() || !getMainWorkLoop()->inGate()) return;
    if (fBeAwdlDetail[30] != bits) beAwdlEvent(3, 0, uint32_t(bits));
    fBeAwdlDetail[30] = bits;
}

void ItlIwx::beAwdlFatal(uint32_t fh, uint32_t hw)
{
    if (!beAwdlOptIn || !beAwdlSupported() || !getMainWorkLoop()->inGate() ||
        fBeAwdlDetail[7] || !fBeAwdlStage || fBeAwdlStage >= 7 ||
        fBeAwdlGeneration != com.sc_generation || fBeAwdlEpoch != fPc1ResetEpoch) return;
    beAwdlEvent(4, uint32_t(fBeAwdlDetail[28]), uint32_t(fBeAwdlDetail[29]), EIO);
    fBeAwdlDetail[7] = beAwdlNowNs();
    if(!fAwdlR42Commands[6]){fAwdlR42Commands[6]=fBeAwdlDetail[7];
        fAwdlR42Commands[7]=fh;fAwdlR42Commands[8]=hw;
        fAwdlR42Commands[9]=com.sc_generation;fAwdlR42Commands[10]=fPc1ResetEpoch;
        const int q=com.sc_tid_data[0].qid>0 && com.sc_tid_data[0].qid<32?com.sc_tid_data[0].qid:com.first_data_qid;
        fAwdlR42Commands[11]=q>0 && q<32?com.txq[q].queued:UINT32_MAX;}

    fBeAwdlDetail[8] = com.sc_generation; fBeAwdlDetail[9] = fPc1ResetEpoch;
    fBeAwdlDetail[10] = fBeAwdlStage;
    fBeAwdlDetail[11] = fBeAwdlDetail[28]; fBeAwdlDetail[12] = fBeAwdlDetail[29];
    fBeAwdlDetail[13] = fh; fBeAwdlDetail[14] = hw;
    fBeAwdlDetail[15] = com.sc_fh_mask; fBeAwdlDetail[16] = com.sc_hw_mask;
    fBeAwdlDetail[17] = com.sc_ic.ic_state; fBeAwdlDetail[18] = com.sc_newstate_pending;
    fBeAwdlDetail[19] = com.sc_flags;
    // One failed experimental context must not provoke an endless retry/reset
    // loop. Preserve normal infra recovery; require manual reboot to retry AWDL.
    // Do not clear queue ownership here: positive device stop remains its fence.
    fBeAwdlDetail[6] = 1;
}

void ItlIwx::beAwdlCaptureBeforeStop()
{
    if (!beAwdlOptIn || !beAwdlSupported() || !getMainWorkLoop()->inGate() ||
        !fBeAwdlDetail[7] || fBeAwdlDetail[20]) return;
    // iwx_stop increments generation before stop_device; reset epoch has NOT
    // advanced here. Never read a later firmware lifetime as the old error.
    if (fBeAwdlDetail[9] != fPc1ResetEpoch ||
        (fBeAwdlDetail[8] != uint32_t(com.sc_generation) &&
         fBeAwdlDetail[8] + 1 != uint32_t(com.sc_generation))) return;
    fBeAwdlDetail[20] = 1;
    fBeAwdlDetail[27] = beAwdlNowNs();
    const uint32_t bases[2] = {
        com.sc_uc.uc_lmac_error_event_table[0] & ~IWX_FW_ADDR_CACHE_CONTROL,
        com.sc_uc.uc_umac_error_event_table & ~IWX_FW_ADDR_CACHE_CONTROL };
    const size_t sizes[2] = { sizeof(iwx_error_event_table), sizeof(iwx_umac_error_event_table) };
    const uint32_t flags[2] = { IWX_ERROR_EVENT_TABLE_LMAC1, IWX_ERROR_EVENT_TABLE_UMAC };
    static_assert(sizeof(iwx_error_event_table) <= 48 * sizeof(uint32_t), "bounded LMAC table");
    static_assert(sizeof(iwx_umac_error_event_table) <= 48 * sizeof(uint32_t), "bounded UMAC table");
    for (unsigned core = 0; core < 2; ++core) {
        uint32_t raw[48] = {};
        const int err = iwx_read_error_table(&com, bases[core], raw, sizes[core],
            (com.sc_uc.error_event_table_tlv_status & flags[core]) != 0);
        fBeAwdlDetail[21 + core] = uint32_t(err);
        fBeAwdlDetail[23 + core] = bases[core];
        fBeAwdlDetail[25 + core] = sizes[core];
        // Raw valid/count and error-id retain firmware endianness as decoded
        // dwords. Failed/inaccessible SRAM is NEVER reported as a valid assert.
        if (!err) for (unsigned j = 0; j < sizes[core] / sizeof(uint32_t); ++j)
            fBeAwdlDetail[288 + core * 24 + j / 2] |= uint64_t(raw[j]) << ((j % 2) * 32);
    }
}

void ItlIwx::beAwdlDetailSnapshot(uint64_t out[336]) const
{
    memcpy(out, fBeAwdlDetail, sizeof(fBeAwdlDetail));
    out[0] = 202; out[1] = 1; out[2] = sizeof(fBeAwdlDetail);
}

// Qualification inputs, not proof of an AWDL-capable data MAC or on-air ACK.
void ItlIwx::beAwdlDataPrerequisites(uint64_t out[16]) const
{
    memset(out,0,16*sizeof(*out));out[0]=205;out[1]=2;out[2]=128;
    out[3]=fBeAwdlRegGeneration>=0;out[4]=fBeAwdlReg44;
    out[5]=uint32_t(fBeAwdlRegGeneration);out[6]=uint32_t(com.sc_generation);
    out[7]=fBeAwdlRegGeneration==com.sc_generation;
    out[8]=IWX_FW_MAC_TYPE_P2P_DEVICE;out[9]=BeAwdlLink;out[10]=BeAwdlStation;
    out[11]=uint32_t(fBeAwdlQueue);
    // Discovery still owns BCAST_MGMT separately. Count only positively
    // installed dedicated data peers, never host-observed addresses.
    out[12]=0;for(const auto &p:fBeAwdlDataPeers)if(p.state==2)++out[12];
    out[13]=beAwdlDataReady();
    out[14]=be_awdl::concurrentGoAllowed(fBeAwdlReg44,out[3]&&out[7],com.sc_ic.ic_state==IEEE80211_S_RUN,beAwdlCurrent());
    out[15]=fBeAwdlEpoch;
}

// A scheduler boundary can retire several entries, including entries without
// an individually retained result. Release EVERY retired CPU reservation.
// This does not retire DMA and never turns scheduler progress into a peer ACK.
void ItlIwx::beAwdlActionRetire(int q,unsigned index)
{
    if(!getMainWorkLoop()->inGate() || !beAwdlQueueOwned() || q!=fBeAwdlQueue ||
       index>=com.txq[q].ring_count || index>=IWX_MIN_256_BA_QUEUE_SIZE_GEN3)return;
    const uint64_t ticket=fBeAwdlNativeTickets[index];
    if(!ticket)return; // an identified status already consumed this ticket
    fBeAwdlNativeTickets[index]=0;
    ++fBeAwdlTxResources[61];
    if(ticket&(1ULL<<63)) {
        // -2 is terminal unknown/failed, distinct from native pending -1.
        const be_awdl::ActionResults::Result r={ticket,UINT32_MAX-1U,
            uint32_t(com.sc_generation),fPc1ResetEpoch};
        if(!fBeAwdlActionResults.push(r))++fBeAwdlNativeStatus[15];
    }
}

// Main-gate CPU snapshot at the actual rejecting branch. Shared DMA admission
// and errno values remain unchanged. Old R16 KGP_TX_2I was an allocation-time
// record and cannot identify which of these branches caused its errno12.
void ItlIwx::beAwdlTxReject(unsigned reason,int q,int index,int error)
{
    if(!beAwdlOptIn || !getMainWorkLoop()->inGate() || reason>=16 ||
       !((beAwdlQueueOwned() && q==fBeAwdlQueue)||beAwdlDataQueueOwned(q)))return;
    auto &ring=com.txq[q];
    if(index<0 || unsigned(index)>=ring.ring_count)return;
    auto &data=ring.data[index];auto *s=fBeAwdlTxResources;
    ++s[3];++s[4+reason];
    uint64_t v[20]={reason,uint32_t(error),beAwdlNowNs(),uint32_t(q),uint32_t(index),
        uint32_t(ring.queued),uint32_t(ring.cur),uint32_t(ring.tail),
        uint64_t(data.m!=nullptr)|(uint64_t(data.tx2I_token!=0)<<1),
        __atomic_load_n(&fTx2IReady,__ATOMIC_ACQUIRE),__atomic_load_n(&fTx2IBlocked,__ATOMIC_ACQUIRE),
        __atomic_load_n(&fTx2JAdmission,__ATOMIC_ACQUIRE),fTx5FreeCount,
        __atomic_load_n(&fTx5Telemetry[0],__ATOMIC_RELAXED),
        __atomic_load_n(&fTx5Telemetry[2],__ATOMIC_RELAXED),__atomic_load_n(&fTx5Telemetry[4],__ATOMIC_RELAXED),
        uint32_t(com.sc_generation),fPc1ResetEpoch,uint32_t(com.sc_ic.ic_if.if_flags),uint32_t(com.sc_flags)};
    memcpy(s+20,v,sizeof(v));if(s[3]==1)memcpy(s+40,v,sizeof(v));
}

// Periodic passive CPU state, not a per-packet operation or firmware query.
// R30 ON exhausted the shared 1024-entry reserve, but first/last rejection
// records cannot identify all owners or the first queue that stopped draining.
// No DMA contents, pointers, payload, scheduling or admission policy is changed.
void ItlIwx::beAwdlQueueSnapshot(uint64_t out[272])
{
    memset(out,0,272*sizeof(*out));out[0]=229;out[1]=1;out[2]=272*sizeof(*out);
    if(!getMainWorkLoop()->inGate())return;
    out[3]=beAwdlNowNs();out[4]=uint32_t(com.sc_generation);out[5]=fPc1ResetEpoch;
    out[6]=fTx5FreeCount;out[7]=__atomic_load_n(&fTx5Telemetry[4],__ATOMIC_RELAXED);
    out[8]=__atomic_load_n(&fTx5Telemetry[0],__ATOMIC_RELAXED);
    out[9]=__atomic_load_n(&fTx5Telemetry[2],__ATOMIC_RELAXED);
    out[10]=__atomic_load_n(&fTx2IBlocked,__ATOMIC_ACQUIRE);out[11]=fBzQueueOwned;
    out[12]=com.qfullmsk;out[13]=dma5Leased();out[14]=kTx2ICount;
    for(unsigned q=0;q<32 && q<nitems(com.txq);++q) {
        const auto &r=com.txq[q];auto *v=out+16+q*8;
        v[0]=q;v[1]=r.ring_count;v[2]=r.queued;v[3]=r.cur;v[4]=r.tail;
        unsigned kind=q==IWX_DQA_CMD_QUEUE?1:0,station=UINT32_MAX,tid=UINT32_MAX;
        if(q>IWX_DQA_CMD_QUEUE && int(q)==com.first_data_qid){kind=2;station=IWX_STATION_ID;tid=IWX_MGMT_TID;}
        for(unsigned t=0;t<IWX_MAX_TID_COUNT;++t)if(q>IWX_DQA_CMD_QUEUE && com.sc_tid_data[t].qid==int(q)){
            kind=2;station=IWX_STATION_ID;tid=t;
        }
        if(int(q)==fBeAwdlQueue){kind=3;station=BeAwdlStation;tid=IWX_MGMT_TID;}
        if(int(q)==fBeAwdlDataQueue){kind=4;station=BeAwdlMcastStation;tid=0;}
        if(int(q)==fBeAwdlGoMgmtQueue){kind=5;station=BeAwdlGoMgmtStation;tid=IWX_MGMT_TID;}
        for(unsigned p=0;p<BeAwdlPeerCount;++p)if(fBeAwdlDataPeers[p].state && fBeAwdlDataPeers[p].queue==int(q)){
            kind=6;station=BeAwdlPeerFirst+p;tid=BeAwdlPeerTid;
        }
        v[5]=kind;v[6]=station;v[7]=tid;
    }
}

// Diagnostic association to the first descriptor actually identified by a real
// single-TX notification. Scheduler SSN/BA reclamation is NOT one result per
// packet, and broadcast management success is NOT a peer-ACK statement.
void ItlIwx::beAwdlTxStatus(struct iwx_tx_resp *response,struct iwx_tx_data *data)
{
    if (!getMainWorkLoop()->inGate() || !beAwdlQueueOwned() ||
        le16toh(response->tx_queue)!=fBeAwdlQueue) return;
    auto *ring=&com.txq[fBeAwdlQueue];
    const uintptr_t address=reinterpret_cast<uintptr_t>(data);
    const uintptr_t first=reinterpret_cast<uintptr_t>(&ring->data[0]);
    if (address<first || address-first>=ring->ring_count*sizeof(*data) ||
        (address-first)%sizeof(*data)) return;
    const unsigned index=unsigned((address-first)/sizeof(*data));
    const uint64_t ticket=fBeAwdlNativeTickets[index];
    if (!ticket) return;
    fBeAwdlNativeTickets[index]=0; // CPU correlation only; no descriptor/free
    auto *s=fBeAwdlNativeStatus;const uint32_t status=le16toh(response->status.status)&IWX_TX_STATUS_MSK;
    ++s[4];if (status!=IWX_TX_STATUS_SUCCESS) ++s[5];
    s[6]=ticket;s[7]=status;s[8]=fBeAwdlQueue;s[9]=index;
    s[10]=com.sc_generation;s[11]=fPc1ResetEpoch;
    s[12]=le32toh(response->initial_rate);s[13]=response->failure_frame;s[14]=beAwdlNowNs();
    if(ticket&(1ULL<<63)) {
        const be_awdl::ActionResults::Result r={ticket,status,uint32_t(com.sc_generation),fPc1ResetEpoch};
        if(!fBeAwdlActionResults.push(r))++s[15]; // no invented ACK on overflow
    }
}

// Bounded CPU observations only. Neither a wait record nor a snapshot changes
// a grant, firmware result, packet ownership or a channel decision.
void ItlIwx::beAwdlTxAdmissionEvent(unsigned stage,uint64_t ticket,int error,const void *frame,size_t bytes)
{
    if(!beAwdlOptIn || !getMainWorkLoop()->inGate())return;
    const uint64_t state[16]={uint32_t(com.sc_generation),fPc1ResetEpoch,stage,ticket,uint32_t(error),
        fBeAwdlWindowStats[10],fBeAwdlWindowStats[10],fBeAwdlWindowStats[12],
        fBeAwdlWindowStats[1],fBeAwdlWindowStats[15],44,
        44,fBeAwdlError,fBeAwdlPreparing,
        fBeAwdlWindowStats[15],uint64_t(fBeAwdlDataStage)};
    fBeAwdlTxAdmission.record(beAwdlNowNs(),state,static_cast<const uint8_t *>(frame),bytes);
}
void ItlIwx::beAwdlInfraRateSnapshot(uint64_t out[32]) const
{
    memcpy(out,fBeAwdlInfraRate,sizeof(fBeAwdlInfraRate));
    ItlHeTxReport report={};const bool owns=const_cast<ItlIwx *>(this)->getHeTxReport(report);
    out[15]=owns;out[16]=report.raw;out[17]=uint64_t(report.kbps)|(uint64_t(report.mcs)<<32)|(uint64_t(report.nss)<<40)|(uint64_t(report.bw)<<48)|(uint64_t(report.gi)<<56);
    out[18]=uint64_t(report.valid)|(uint64_t(report.fresh)<<1)|(uint64_t(report.source)<<8);out[19]=report.age_ms;
    out[20]=beAwdlNowNs();out[21]=uint32_t(com.sc_generation);out[22]=fPc1ResetEpoch;
    out[23]=com.sc_ic.ic_state;
    const auto *n=com.sc_ic.ic_bss;
    if(n){out[24]=n->ni_txmcs;out[25]=n->ni_txrate;out[26]=n->ni_chw;out[27]=com.sc_ic.ic_curmode;
        out[28]=n->ni_port_valid;out[29]=n->ni_associd&0x3fff;
        if(n->ni_chan && n->ni_chan!=IEEE80211_CHAN_ANYC)out[30]=ieee80211_chan2ieee(const_cast<ieee80211com *>(&com.sc_ic),n->ni_chan);}
}
void ItlIwx::beAwdlInfraCoexSnapshot(uint64_t out[64]) const
{
    memcpy(out,fBeAwdlInfraCoex,sizeof(fBeAwdlInfraCoex));
    const auto *n=com.sc_ic.ic_bss;
    if(n && n->ni_chan && n->ni_chan!=IEEE80211_CHAN_ANYC){
        out[8]=n->ni_chan->ic_freq;out[9]=n->ni_chan->ic_center_freq1;out[10]=n->ni_chw;
    }
    out[12]=fBeAwdlDataFilter;out[13]=fBeAwdlDataFilterKnown;
    out[28]=fPc1ResetEpoch;out[29]=uint32_t(com.sc_generation);out[30]=fBeAwdlDataStage;out[31]=fBeAwdlError;
    out[32]=com.sc_ic.ic_state;out[33]=com.ns_nstate;out[34]=com.sc_flags;out[35]=fBzFault;
    for(unsigned i=0;i<8;++i)out[40+i]=fBzStats[24+i];
    out[48]=44;out[49]=44;out[50]=fBeAwdlPhy;
    out[51]=fBeAwdlDataEpoch;out[52]=uint32_t(fBeAwdlDataGeneration);out[53]=fBeAwdlSfHeld;
    out[54]=fBeAwdlWindowStats[1];out[55]=beAwdlNowNs();
    out[57]=com.sc_tx_timer;out[58]=uint32_t(com.qfullmsk);
    out[59]=com.sc_ic.ic_if.if_snd.ifq_oactive;out[60]=com.sc_rx_ba_sessions;
}

void ItlIwx::beAwdlInfraRxOwnerNote(unsigned scope,const uint8_t *frame,size_t bytes,
                                   uint32_t status,uint8_t macPhy,uint8_t channel,unsigned reason)
{
    // Bounded CPU headers only. No allocation, native call, firmware command,
    // per-frame registry publication or management body/credential retention.
    auto *h=fBeAwdlInfraRxOwner;++h[3];
    const bool scan=scope==be_awdl::InfraIsolation::ScanOnly;
    if(scan)++h[4];else if((frame[0]&0x0c)==4)++h[6];else ++h[5];
    if(scan)++fBeAwdlInfraCoex[61];
    else if((frame[0]&0x0c)==4)++fBeAwdlInfraCoex[62];else ++fBeAwdlInfraCoex[27];
    auto record=[&](uint64_t *v) {
        memset(v,0,16*sizeof(*v));v[0]=beAwdlNowNs();v[1]=uint32_t(com.sc_generation);v[2]=fPc1ResetEpoch;
        v[3]=scope;v[4]=channel;v[5]=status;v[6]=macPhy;v[7]=bytes;
        memcpy(v+8,frame,MIN(bytes,size_t(24)));
        memcpy(v+11,com.sc_ic.ic_myaddr,6);memcpy(reinterpret_cast<uint8_t *>(v+11)+6,com.sc_ic.ic_bss->ni_bssid,6);
        v[13]=uint32_t(com.sc_ic.ic_state)|(uint64_t(uint32_t(com.ns_nstate))<<32);
        v[14]=com.sc_flags;v[15]=reason;
    };
    if(scan)record(h+48);
    else {if(!h[16])record(h+16);record(h+32);} // first reject stays boot/epoch-qualified
}
