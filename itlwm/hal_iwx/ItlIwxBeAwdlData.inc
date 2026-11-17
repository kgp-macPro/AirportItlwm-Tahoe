/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 KGP.
 * GL92 data role: Intel Linux v6.12 adc218676eef mld-mac/link/mld-sta/tx,
 * SPDX GPL-2.0 OR BSD-3-Clause, Intel Corporation. Native framing/beacon
 * mechanism comparison: Bananz0 400019bacbc6, Glen Muthoka, GPL-2.0-or-later.
 * Distinct from the existing management MAC1/link1 and infrastructure MAC0.
 */
#include <Airport/BeAwdlDataTransport.hpp>

bool ItlIwx::beAwdlDataReady() const
{
    const auto *ni=com.sc_ic.ic_bss;
    return ni && ni->ni_port_valid && (ni->ni_associd&0x3fff) && beAwdlDataOptIn && beAwdlCurrent() && fBeAwdlDataStage==7 && !fBeAwdlDataError && !fBeAwdlDataPreparing &&
        fBeAwdlDataGeneration==com.sc_generation && fBeAwdlDataEpoch==fPc1ResetEpoch &&
        fBeAwdlRegGeneration==com.sc_generation && fBeAwdlDataPhyStage==4 && fBeAwdlDataFilterKnown &&
        (!bzDevice() || fBeAwdlDataFilter==(IWX_MAC_CFG_FILTER_ACCEPT_GRP|IWX_MAC_CFG_FILTER_ACCEPT_BEACON|IWX_MAC_CFG_FILTER_ACCEPT_PROBE_REQ)) &&
        fBeAwdlDataPhyEpoch==fPc1ResetEpoch && fBeAwdlDataPhyGeneration==uint32_t(com.sc_generation) && be_awdl::dataChannelAllowed(fBeAwdlReg44,true,true,true);
}
// Acceptance owns a bounded CPU request, not a firmware removal or TX grant.
// Exact Legacy deletePeerFromFWCache0x9f554 sets delete-pending BEFORE the
// IOCTL and does not roll it back on NotReady. Retunes/background scans may
// temporarily close dataReady while this generation still owns the peer.
// Keep that cleanup request until the existing safe service/fences can run.
bool ItlIwx::beAwdlDataCanQueueRemoval() const
{
    if(!beAwdlDataOptIn || !beAwdlQueueOwned() || fBeAwdlDetail[6] ||
       fBeAwdlError || fBeAwdlDataError ||
       fBeAwdlDataGeneration!=com.sc_generation || fBeAwdlDataEpoch!=fPc1ResetEpoch)
        return false;
    return fBeAwdlDataStage==7 || beAwdlDataRemovalComplete();
}
// Hold new discovery leases until the separately opted-in data transaction is
// ready. This does not declare a native gate, peer or firmware resource ready.
bool ItlIwx::beAwdlDataSetupPending() const
{
    return beAwdlDataOptIn && beAwdlCurrent() && !beAwdlDataReady();
}
bool ItlIwx::beAwdlDataQueueReserved(int q) const
{
    if(!beAwdlOptIn || !beAwdlSupported() || fBeAwdlDataEpoch!=fPc1ResetEpoch)return false;
    if(q>=0 && (q==fBeAwdlDataQueue || q==fBeAwdlGoMgmtQueue))return true;
    for(const auto &p:fBeAwdlDataPeers)if(q>=0 && q==p.queue)return true;
    return false;
}
bool ItlIwx::beAwdlDataQueueOwned(int q,int station,int tid) const
{
    if(!beAwdlQueueOwned() || !beAwdlDataQueueReserved(q) || q<=0 || q>=32 ||
       !((fBzDevice?fBzQueueOwned:fBeAwdlAxQueueOwned)&(1U<<q)) || fBeAwdlDataGeneration!=com.sc_generation)return false;
    if(q==fBeAwdlGoMgmtQueue)return (station<0 || station==BeAwdlGoMgmtStation) && (tid<0 || tid==IWX_MGMT_TID);
    if(q==fBeAwdlDataQueue)return (station<0 || station==BeAwdlMcastStation) && (tid<0 || tid==0);
    for(unsigned i=0;i<BeAwdlPeerCount;++i)if(q==fBeAwdlDataPeers[i].queue)
        return (station<0 || station==int(BeAwdlPeerFirst+i)) && (tid<0 || tid==BeAwdlPeerTid);
    return false;
}
bool ItlIwx::beAwdlDataAllocates(int station,int tid) const
{
    return beAwdlDataControlCurrent() && fBeAwdlDataPreparing && fBeAwdlDataEpoch==fPc1ResetEpoch &&
        fBeAwdlDataGeneration==com.sc_generation && station==fBeAwdlDataAllocStation &&
        tid==fBeAwdlDataAllocTid && station>=BeAwdlMcastStation && (station<BeAwdlPeerFirst+BeAwdlPeerCount || station==BeAwdlGoMgmtStation);
}
// Firmware command submission for privately owned PHY1/MAC2/LINK2 resources.
int ItlIwx::beAwdlDataCommand(uint32_t phase,uint32_t command,size_t bytes,const void *payload,bool requirePolicy,struct iwx_host_cmd *response)
{
    auto *sc=&com;const auto *ni=sc->sc_ic.ic_bss;
    const bool run=sc->sc_ic.ic_state==IEEE80211_S_RUN && ni && ni->ni_port_valid && (ni->ni_associd&0x3fff);
    const bool permission=be_awdl::dataChannelAllowed(fBeAwdlReg44,
        fBeAwdlRegGeneration==sc->sc_generation,run,beAwdlCurrent());
    int pre=0;
    if(!getMainWorkLoop()->inGate() || !fBeAwdlDataPreparing || !beAwdlDataOptIn ||
       !beAwdlSupported() || !beAwdlLifetimeCurrent(sc,fBeAwdlDataGeneration,fBeAwdlDataEpoch))pre=ENXIO;
    else if(bytes>256 || !payload)pre=EINVAL;
    else if(requirePolicy && !permission)pre=EACCES;
    struct Hold {ieee80211com *ic;ieee80211_node *n;~Hold(){if(n)ieee80211_release_node(ic,n);}} hold{&sc->sc_ic,ni?ieee80211_ref_node(const_cast<ieee80211_node *>(ni)):nullptr};
    const bool record=!fBeAwdlDataProofFrozen;
    if(record){const uint64_t serial=fBeAwdlDataProof[3]+1;memset(fBeAwdlDataProof,0,sizeof(fBeAwdlDataProof));
        auto *v=fBeAwdlDataProof;v[0]=209;v[1]=1;v[2]=512;v[3]=serial;v[4]=phase;v[5]=command;v[6]=bytes;
        v[7]=beAwdlNowNs();v[11]=sc->sc_generation;v[12]=fPc1ResetEpoch;v[15]=fBeAwdlReg44;v[16]=fBeAwdlRegGeneration;
        v[17]=fBeAwdlPhy;v[18]=0;v[19]=fBeAwdlWindowStats[1];v[20]=fBeAwdlWindowStats[15];
        v[21]=iwx_lookup_cmd_ver(sc,command==IWX_TXPATH_FLUSH?IWX_LONG_GROUP:(command>>8)&255,command&255);v[22]=!pre;v[23]=permission;
        v[24]=fBeAwdlPhy;v[25]=ni?ni->ni_chw:0;v[26]=ni?(ni->ni_associd&0x3fff):0;v[27]=ni&&ni->ni_port_valid;
        v[28]=fBeAwdlDataPhyStage;v[29]=fBeAwdlDataLinkRemoved;v[30]=requirePolicy;
        if(payload && bytes<=256)memcpy(v+32,payload,bytes);
    }
    int raw=0,validation=pre;
    if(!pre){
        // A submitted SF command may have changed firmware policy even when
        // its completion is ambiguous. Hold it until a positively qualified stop.
        if(phase==20)fBeAwdlSfHeld=true;
        beAwdlEvent(1,phase,command);
        raw=response?iwx_send_cmd(sc,response):iwx_send_cmd_pdu(sc,command,0,bytes,payload);
        if(!beAwdlLifetimeCurrent(sc,fBeAwdlDataGeneration,fBeAwdlDataEpoch) ||
           (requirePolicy && (!beAwdlDataControlCurrent() || sc->sc_ic.ic_bss!=ni ||
            !be_awdl::dataChannelAllowed(fBeAwdlReg44,fBeAwdlRegGeneration==sc->sc_generation,
                sc->sc_ic.ic_state==IEEE80211_S_RUN && ni && ni->ni_port_valid && (ni->ni_associd&0x3fff),true))))validation=ENXIO;
        beAwdlEvent(2,phase,command,raw?raw:validation);
    }
    fBeAwdlDataStats[61]=bytes;fBeAwdlDataStats[5]=phase;fBeAwdlDataStats[6]=command;fBeAwdlDataStats[52]=uint32_t(raw);
    fBeAwdlDataStats[53]=uint32_t(validation);fBeAwdlDataStats[54]=beAwdlNowNs();
    fBeAwdlDataStats[56]=fBeAwdlWindowStats[1];fBeAwdlDataStats[57]=fBeAwdlWindowStats[15];
    if(record){auto *v=fBeAwdlDataProof;v[8]=beAwdlNowNs();v[9]=uint32_t(raw);v[10]=uint32_t(validation);
        v[13]=sc->sc_generation;v[14]=fPc1ResetEpoch;
        if(raw || validation){fBeAwdlDataProofFrozen=true;v[31]=1;}}
    return raw?raw:validation;
}
void ItlIwx::beAwdlDataConfigSnapshot(uint64_t out[64]) const
{
    memcpy(out,fBeAwdlDataProof,sizeof(fBeAwdlDataProof));
}
int ItlIwx::beAwdlDataPhyPrepare()
{
    if(!bzDevice())return beAwdlAxPhyPrepare();
    auto *sc=&com;auto &slot=sc->sc_phyctxt[1];
    if(iwx_lookup_cmd_ver(sc,IWX_LONG_GROUP,IWX_PHY_CONTEXT_CMD)!=5 ||
       iwx_lookup_cmd_ver(sc,IWX_DATA_PATH_GROUP,IWX_RLC_CONFIG_CMD)!=2)return ENOTSUP;
    if(fBeAwdlDataPhyStage && fBeAwdlDataPhyEpoch!=fPc1ResetEpoch){
        // A new positive-stop epoch means firmware no longer owns the old PHY.
        fBeAwdlDataPhyStage=0;
    }
    if(fBeAwdlPhy!=0 || slot.id!=1 || slot.ref || slot.channel!=&sc->sc_ic.ic_channels[1])return EBUSY;
    if(fBeAwdlDataPhyStage && (fBeAwdlDataPhyGeneration!=uint32_t(sc->sc_generation) ||
       fBeAwdlDataPhyEpoch!=fPc1ResetEpoch || fBeAwdlDataPhyStage!=2))return EBUSY;
    static_assert(sizeof(struct iwx_phy_context_cmd)==32,"GL92 PHYv5 wire");
    static_assert(sizeof(struct iwx_rlc_config_cmd)==32,"GL92 RLCv2 wire");
    struct iwx_phy_context_cmd cmd={};int err;
    cmd.id_and_color=htole32(IWX_FW_CMD_ID_AND_COLOR(slot.id,slot.color));
    if(!fBeAwdlDataPhyStage){
        fBeAwdlDataPhyGeneration=sc->sc_generation;fBeAwdlDataPhyEpoch=fPc1ResetEpoch;
        fBeAwdlDataPhyStage=1; // reserve before a sleeping/ambiguous command
        cmd.action=htole32(IWX_FW_CTXT_ACTION_REMOVE);
        cmd.ci.channel=htole32(1);cmd.ci.band=IWX_PHY_BAND_24;
        cmd.lmac_id=htole32(iwx_lmac_id(sc,slot.channel));
        err=beAwdlDataCommand(40,IWX_PHY_CONTEXT_CMD,sizeof(cmd),&cmd);if(err)return err;
        fBeAwdlDataPhyStage=2;
    }
    memset(&cmd,0,sizeof(cmd));cmd.id_and_color=htole32(IWX_FW_CMD_ID_AND_COLOR(slot.id,slot.color));
    cmd.action=htole32(IWX_FW_CTXT_ACTION_ADD);cmd.ci.channel=htole32(44);cmd.ci.band=IWX_PHY_BAND_5;
    cmd.ci.width=0;cmd.ci.ctrl_pos=IWX_PHY_VHT_CTRL_POS_1_BELOW;
    cmd.lmac_id=htole32(iwx_lmac_id(sc,&sc->sc_ic.ic_channels[44]));
    // PHYv5's union is SBB bandwidth/ctrl/puncture. No AP-derived SBB and
    // no rx-chain bits here; explicit RLCv2 carries receive chains below.
    fBeAwdlDataPhyStage=1;
    err=beAwdlDataCommand(41,IWX_PHY_CONTEXT_CMD,sizeof(cmd),&cmd);if(err)return err;
    fBeAwdlDataPhyStage=3;
    struct iwx_rlc_config_cmd rlc={};rlc.phy_id=htole32(1);
    const uint32_t antennas=iwx_fw_valid_rx_ant(sc);const unsigned count=iwx_num_of_ant(antennas)>1?2:1;
    rlc.rlc.rx_chain_info=htole32((antennas<<IWX_PHY_RX_CHAIN_VALID_POS)|
        (count<<IWX_PHY_RX_CHAIN_CNT_POS)|(count<<IWX_PHY_RX_CHAIN_MIMO_CNT_POS));
    err=beAwdlDataCommand(42,iwx_cmd_id(IWX_RLC_CONFIG_CMD,IWX_DATA_PATH_GROUP,2),sizeof(rlc),&rlc);
    if(!err)fBeAwdlDataPhyStage=4;return err; // DO NOT suppress an RLC error
}
int ItlIwx::beAwdlDataPhyRemove()
{
    if(!bzDevice())return 0; // AX PHY release follows removal of both private MACs
    if(fBeAwdlDataPhyStage!=4 || fBeAwdlDataPhyEpoch!=fPc1ResetEpoch ||
       fBeAwdlDataPhyGeneration!=uint32_t(com.sc_generation))return EBUSY;
    struct iwx_phy_context_cmd cmd={};auto &slot=com.sc_phyctxt[1];
    cmd.id_and_color=htole32(IWX_FW_CMD_ID_AND_COLOR(slot.id,slot.color));cmd.action=htole32(IWX_FW_CTXT_ACTION_REMOVE);
    cmd.ci.channel=htole32(44);cmd.ci.band=IWX_PHY_BAND_5;cmd.ci.width=0;
    cmd.lmac_id=htole32(iwx_lmac_id(&com,&com.sc_ic.ic_channels[44]));
    const int err=beAwdlDataCommand(43,IWX_PHY_CONTEXT_CMD,sizeof(cmd),&cmd,false);
    if(!err)fBeAwdlDataPhyStage=2; // known removed, privately reserved until positive stop
    return err;
}
// Link MODIFY composition is role-local for bind, activation AND deactivation.
// Linux link_changed always fills the complete role definition, not a zeroed
// subset selected only by modify_mask. Do not reintroduce the R5 omission on stop.
void ItlIwx::beAwdlDataLinkParameters(struct iwx_link_config_cmd *link) const
{
    link->link_id=htole32(BeAwdlDataLink);link->mac_id=htole32(BeAwdlDataMac);link->phy_id=htole32(1);
    memcpy(link->local_link_addr,fBeAwdlAddress,6);
    link->ofdm_rates=htole32(0x15);link->short_slot=htole32(1);
    link->bi=htole32(be_awdl::DataBeaconIntervalTu);
    link->dtim_interval=htole32(be_awdl::DataBeaconIntervalTu*be_awdl::DataDtimPeriod);
    // Own legacy/no-WMM role: Linux mac80211 defaults are DCF on every
    // AC (15/1023, AIFS2, TXOP0), not the associated AP's WMM parameters.
    for(unsigned i=0;i<EDCA_NUM_AC;++i){auto &b=link->ac[iwx_mvm_mac80211_ac_to_ucode_ac((ieee80211_edca_ac)i)];
        b.cw_min=htole16(15);b.cw_max=htole16(1023);b.aifsn=2;
        b.fifos_mask=1U<<iwx_ac_to_tx_fifo[i];}
    link->ac[iwx_mvm_mac80211_ac_to_ucode_ac((ieee80211_edca_ac)EDCA_AC_VO)].fifos_mask|=1U<<IWX_TX_FIFO_MCAST;
    link->qos_flags=0; // no WMM IE and no HT: no UPDATE_EDCA or TGN claim
}
int ItlIwx::beAwdlDataPrepare()
{
    if(!bzDevice())return beAwdlAxDataPrepare();
    if(!beAwdlDataOptIn)return ENOTSUP;
    auto *sc=&com;if(!getMainWorkLoop()->inGate() || !beAwdlCurrent())return EAGAIN;
    if(fBeAwdlDataPreparing)return EBUSY;
    const bool cleanRemoved=fBeAwdlDataStage==8 && !fBeAwdlResultCount && !fBeAwdlSfHeld && !fBeAwdlDataStats[26];
    if(!cleanRemoved && fBeAwdlDataGeneration==sc->sc_generation && fBeAwdlDataEpoch==fPc1ResetEpoch)
        return beAwdlDataReady()?0:int(fBeAwdlDataError?fBeAwdlDataError:EIO);
    if(!cleanRemoved && fBeAwdlDataStage && fBeAwdlDataEpoch==fPc1ResetEpoch)return ESTALE; // no same-firmware ID reuse after generation change
    if(fBeAwdlWindowCommand || fBeAwdlWindowStats[1]){++fBeAwdlDataStats[55];return EAGAIN;}
    if(!isset(sc->sc_enabled_capa,IWX_UCODE_TLV_CAPA_DYNAMIC_QUOTA))return ENOTSUP;
    if(!be_awdl::dataChannelAllowed(fBeAwdlReg44,fBeAwdlRegGeneration==sc->sc_generation,true,true))return EACCES;
    if(!memcmp(fBeAwdlAddress,sc->sc_ic.ic_myaddr,6) || !memcmp(fBeAwdlAddress,sc->sc_ic.ic_bss->ni_bssid,6))return EADDRINUSE;
    if(!sc->sc_ic.ic_bss->ni_port_valid || !(sc->sc_ic.ic_bss->ni_associd&0x3fff))return EAGAIN;
    if(iwx_lookup_cmd_ver(sc,IWX_LONG_GROUP,IWX_BEACON_TEMPLATE_CMD)!=13 ||
       iwx_lookup_cmd_ver(sc,IWX_DATA_PATH_GROUP,IWX_SCD_QUEUE_CONFIG_CMD)!=3 ||
       iwx_lookup_cmd_ver(sc,IWX_LONG_GROUP,IWX_TXPATH_FLUSH)!=2 ||
       iwx_lookup_notif_ver(sc,IWX_LONG_GROUP,IWX_TXPATH_FLUSH)!=1)return ENOTSUP;
    const auto staVersion=iwx_lookup_cmd_ver(sc,IWX_MAC_CONF_GROUP,IWX_STA_CONFIG_CMD);
    if(staVersion!=1 && staVersion!=IWX_FW_CMD_VER_UNKNOWN)return ENOTSUP;
    if(iwx_lookup_cmd_ver(sc,IWX_LONG_GROUP,IWX_PHY_CONTEXT_CMD)!=5 ||
       iwx_lookup_cmd_ver(sc,IWX_DATA_PATH_GROUP,IWX_RLC_CONFIG_CMD)!=2 || fBeAwdlPhy!=0)return ENOTSUP;
    fBeAwdlDataPreparing=true;fBeAwdlDataGeneration=sc->sc_generation;fBeAwdlDataEpoch=fPc1ResetEpoch;
    fBeAwdlDataQueueRemoved=fBeAwdlDataStationRemoved=0;
    fBeAwdlDataStage=1;fBeAwdlDataError=0;fBeAwdlDataQuiesceTried=false;fBeAwdlDataQueue=fBeAwdlGoMgmtQueue=-1;
    for(auto &p:fBeAwdlDataPeers)p=BeAwdlDataPeer{};
    memset(fBeAwdlDataSlots,0,sizeof(fBeAwdlDataSlots));
    fBeAwdlResultHead=fBeAwdlResultTail=fBeAwdlResultCount=0;
    struct iwx_sf_cfg_cmd sf={};struct iwx_mac_config_cmd mac={};struct iwx_link_config_cmd link={};struct iwx_sta_cfg_cmd sta={};
    struct {be_awdl::Beacon13 header;uint8_t frame[54];} __packed beacon={};
    int err=0,q=-1;const int generation=sc->sc_generation;const uint32_t epoch=fPc1ResetEpoch;
#define DATA_SEND(phase,group,id,value) do {     err=beAwdlDataCommand(phase,iwx_cmd_id(id,group,0),sizeof(value),&(value));     if(err)goto done; }while(0)
    // Synchronous command completion before any data MAC can be bound.
    sf.state=htole32(IWX_SF_UNINIT);iwx_fill_sf_command(sc,&sf,nullptr);
    if(!isset(sc->sc_ucode_api,IWX_UCODE_TLV_API_SMART_FIFO_OFFLOAD)) { DATA_SEND(20,0,IWX_REPLY_SF_CFG_CMD,sf); }
    err=beAwdlDataPhyPrepare();if(err)goto done;
    fBeAwdlDataLinkRemoved=false;
    mac.id_and_color=htole32(BeAwdlDataMac);mac.action=htole32(IWX_FW_CTXT_ACTION_ADD);mac.mac_type=htole32(IWX_FW_MAC_TYPE_GO);
    memcpy(mac.local_mld_addr,fBeAwdlAddress,6);
    // R42: this is a GO, not Intel's LISTENER filter. Keep addressed/group
    // reception and GO probe/beacon handling; Discovery MAC1 owns broad
    // management admission. No same-channel promiscuous/control admission.
    // NoDS/ACK acceptance without PROMISC remains a physical qualification.
    mac.filter_flags=htole32(IWX_MAC_CFG_FILTER_ACCEPT_GRP|IWX_MAC_CFG_FILTER_ACCEPT_BEACON|
        IWX_MAC_CFG_FILTER_ACCEPT_PROBE_REQ);
    fBeAwdlDataFilter=le32toh(mac.filter_flags);fBeAwdlDataFilterKnown=false;
    DATA_SEND(21,IWX_MAC_CONF_GROUP,IWX_MAC_CONFIG_CMD,mac);fBeAwdlDataFilterKnown=true;fBeAwdlDataStage=2;
    link.action=htole32(IWX_FW_CTXT_ACTION_ADD);link.link_id=htole32(BeAwdlDataLink);link.mac_id=htole32(BeAwdlDataMac);
    link.phy_id=htole32(IWX_FW_CTXT_INVALID);memcpy(link.local_link_addr,fBeAwdlAddress,6);
    DATA_SEND(22,IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,link);fBeAwdlDataStage=3;
    // Linux link_changed always composes the full payload, even when the
    // modify mask applies only ACTIVE/RATES. In R5 BI/DTIM/short-slot/EDCA were
    // still zero at activation and were populated only after the beacon.
    link.action=htole32(IWX_FW_CTXT_ACTION_MODIFY);beAwdlDataLinkParameters(&link);
    fBeAwdlDataStats[58]=be_awdl::DataBeaconIntervalTu;fBeAwdlDataStats[59]=be_awdl::DataDtimPeriod;
    DATA_SEND(23,IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,link);
    link.modify_mask=htole32(IWX_LINK_CONTEXT_MODIFY_ACTIVE|IWX_LINK_CONTEXT_MODIFY_RATES_INFO);
    link.active=htole32(1);fBeAwdlDataStats[60]=le32toh(link.modify_mask);
    fBeAwdlDataStats[62]=le32toh(link.bi);fBeAwdlDataStats[63]=le32toh(link.dtim_interval);
    DATA_SEND(24,IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,link);
    // Linux mld_start_ap explicitly activates BEFORE the beacon. Preserve
    // that ordering; complete payload is the correction, not a guessed reorder.
    beacon.header.bytes=htole16(be_awdl::dataBeacon(beacon.frame,sizeof(beacon.frame),fBeAwdlAddress));
    beacon.header.link=htole32(BeAwdlDataLink);beacon.header.timIndex=htole32(48);beacon.header.timSize=htole32(4);
    DATA_SEND(25,0,IWX_BEACON_TEMPLATE_CMD,beacon);fBeAwdlDataStage=4;
    link.modify_mask=htole32(IWX_LINK_CONTEXT_MODIFY_BEACON_TIMING|IWX_LINK_CONTEXT_MODIFY_PROTECT_FLAGS|IWX_LINK_CONTEXT_MODIFY_QOS_PARAMS);
    DATA_SEND(26,IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,link);fBeAwdlDataStage=5;
    sta.sta_id=htole32(BeAwdlMcastStation);sta.link_id=htole32(BeAwdlDataLink);sta.station_type=htole32(IWX_STATION_TYPE_MCAST);
    sta.peer_mld_address[0]=sta.peer_link_address[0]=3;
    DATA_SEND(27,IWX_MAC_CONF_GROUP,IWX_STA_CONFIG_CMD,sta);fBeAwdlDataStage=6;
    fBeAwdlDataAllocStation=BeAwdlMcastStation;fBeAwdlDataAllocTid=0;
    q=iwx_tvqm_enable_txq(sc,0,0,beAwdlQueueSize(),BeAwdlMcastStation);
    if(q<0){err=-q;goto done;}if(!beAwdlLifetimeCurrent(sc,generation,epoch)){err=ENXIO;goto done;}
    fBeAwdlDataQueue=q;
    // Linux start_ap: the GO owns its own BCAST_MGMT station as well as MCAST.
    memset(&sta,0,sizeof(sta));sta.sta_id=htole32(BeAwdlGoMgmtStation);sta.link_id=htole32(BeAwdlDataLink);
    sta.station_type=htole32(IWX_STATION_TYPE_BCAST_MGMT);memset(sta.peer_mld_address,255,6);memset(sta.peer_link_address,255,6);
    if(isset(sc->sc_enabled_capa,IWX_UCODE_TLV_CAPA_STA_EXP_MFP_SUPPORT))sta.mfp=htole32(1);
    DATA_SEND(28,IWX_MAC_CONF_GROUP,IWX_STA_CONFIG_CMD,sta);
    fBeAwdlDataAllocStation=BeAwdlGoMgmtStation;fBeAwdlDataAllocTid=IWX_MGMT_TID;
    q=iwx_tvqm_enable_txq(sc,IWX_MGMT_TID,0,beAwdlQueueSize(),BeAwdlGoMgmtStation);
    if(q<0){err=-q;goto done;}if(!beAwdlLifetimeCurrent(sc,generation,epoch)){err=ENXIO;goto done;}
    fBeAwdlGoMgmtQueue=q;fBeAwdlDataStage=7;
done:
    // One bounded CPU copy per GO setup, including partial failure. Window
    // command traffic must not overwrite the last activation's real bytes.
    // Do not clear this forensic copy on recovery; its generation stays explicit.
    memcpy(fAwdlR42Activation,fAwdlR42Commands,sizeof(fAwdlR42Activation));
    fBeAwdlDataStats[7]=uint32_t(err);fBeAwdlDataError=err;fBeAwdlDataPreparing=false;
    fBeAwdlDataAllocStation=fBeAwdlDataAllocTid=-1;beAwdlControlDone();
    // Ambiguous or partial setup is not retried, nor freed on a timer.
    return err;
#undef DATA_SEND
}
bool ItlIwx::beAwdlDataRemovalComplete() const
{
    // R28: stage8 alone precedes the synchronous SF restore and is NOT a
    // completion fence. Require the actual whole-role stop postconditions.
    // Retired backing remains owned by the existing DMA pool; this only
    // qualifies an owed native deletion, never storage reuse or an add.
    if(!beAwdlCurrent() || fBeAwdlDataPreparing || fBeAwdlDataStage!=8 ||
       fBeAwdlDataGeneration!=com.sc_generation || fBeAwdlDataEpoch!=fPc1ResetEpoch ||
       fBeAwdlSfHeld || fBeAwdlDataError || fBeAwdlDataStats[26] || fBeAwdlResultCount ||
       !fBeAwdlDataLinkRemoved || fBeAwdlDataPhyStage!=(bzDevice()?2U:4U) ||
       fBeAwdlDataPhyGeneration!=uint32_t(com.sc_generation) || fBeAwdlDataPhyEpoch!=fPc1ResetEpoch ||
       fBeAwdlDataQueue>=0 || fBeAwdlGoMgmtQueue>=0)return false;
    uint32_t removed=(1U<<BeAwdlMcastStation)|(1U<<BeAwdlGoMgmtStation);
    for(unsigned i=0;i<BeAwdlPeerCount;++i){const auto &p=fBeAwdlDataPeers[i];
        if(p.queue>=0 || (p.state && p.state!=4))return false;
        if(p.state==4)removed|=1U<<(BeAwdlPeerFirst+i);
    }
    return (fBeAwdlDataQueueRemoved&removed)==removed &&
        (fBeAwdlDataStationRemoved&removed)==removed;
}
int ItlIwx::beAwdlDataPeer(const uint8_t address[6],bool add)
{
    if(!getMainWorkLoop()->inGate()||fBeAwdlDataPreparing)return EAGAIN;
    if(!address || address[0]&1 || !memcmp(address,"\0\0\0\0\0\0",6)||!memcmp(address,fBeAwdlAddress,6))return EINVAL;
    if(!add && beAwdlDataRemovalComplete())return 0; // real global removal already satisfied this delete
    if(add?!beAwdlDataReady():!beAwdlDataCanQueueRemoval())return EAGAIN;
    unsigned slot=BeAwdlPeerCount;
    for(unsigned i=0;i<BeAwdlPeerCount;++i)if(fBeAwdlDataPeers[i].state && !memcmp(fBeAwdlDataPeers[i].address,address,6)){slot=i;break;}
    if(add && slot!=BeAwdlPeerCount)return fBeAwdlDataPeers[slot].state==2?0:EBUSY;
    // A successful whole-role fence or successful individual removal can
    // precede the native delete request. A zero slot is only published after
    // those fences (or before any add); ambiguous/partial peers stay nonzero.
    // Complete the already-absent state, not an invented firmware command.
    if(!add && slot==BeAwdlPeerCount)return 0;
    if(add)for(unsigned i=0;i<BeAwdlPeerCount;++i)if(!fBeAwdlDataPeers[i].state){slot=i;break;}
    if(slot==BeAwdlPeerCount)return ENOSPC;
    auto &p=fBeAwdlDataPeers[slot];if(!add && p.state!=2)return EBUSY;int err=0;const int generation=com.sc_generation;const uint32_t epoch=fPc1ResetEpoch;
    fBeAwdlDataPreparing=true;
    uint32_t phase=30,command=iwx_cmd_id(IWX_STA_CONFIG_CMD,IWX_MAC_CONF_GROUP,0);
    if(add){fBeAwdlDataQueueRemoved&=~(1U<<(BeAwdlPeerFirst+slot));fBeAwdlDataStationRemoved&=~(1U<<(BeAwdlPeerFirst+slot));p.state=1;memcpy(p.address,address,6);struct iwx_sta_cfg_cmd cmd={};
        cmd.sta_id=htole32(BeAwdlPeerFirst+slot);cmd.link_id=htole32(BeAwdlDataLink);cmd.station_type=htole32(IWX_STATION_TYPE_PEER);
        memcpy(cmd.peer_mld_address,address,6);memcpy(cmd.peer_link_address,address,6);
        err=bzDevice()?beAwdlDataCommand(phase,command,sizeof(cmd),&cmd):beAwdlAxStation(BeAwdlPeerFirst+slot,2,address,false);
        if(!err&&!beAwdlLifetimeCurrent(&com,generation,epoch))err=ENXIO;
        if(!err){fBeAwdlDataAllocStation=BeAwdlPeerFirst+slot;fBeAwdlDataAllocTid=BeAwdlPeerTid;
            phase=31;command=iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD,IWX_DATA_PATH_GROUP,0);beAwdlEvent(1,phase,command);
            int q=iwx_tvqm_enable_txq(&com,BeAwdlPeerTid,0,beAwdlQueueSize(),BeAwdlPeerFirst+slot);beAwdlEvent(2,phase,command,q<0?-q:0);
            if(q<0)err=-q;else p.queue=q;}
        if(!err && beAwdlLifetimeCurrent(&com,generation,epoch)){p.state=2;++fBeAwdlDataStats[12];}else p.state=3;
    }else{
        // Use the same firmware fence/order as whole-role removal. Do not
        // use sc_tid_data: these are private AWDL queues, not the BSS TIDs.
        phase=32;command=IWX_TXPATH_FLUSH;
        err=beAwdlDataRemoveStation(BeAwdlPeerFirst+slot,p.queue,BeAwdlPeerTid,32);
        if(!err&&beAwdlLifetimeCurrent(&com,generation,epoch)){p=BeAwdlDataPeer{};++fBeAwdlDataStats[13];}
    }
    fBeAwdlDataPreparing=false;fBeAwdlDataAllocStation=fBeAwdlDataAllocTid=-1;beAwdlControlDone();
    if(!beAwdlLifetimeCurrent(&com,generation,epoch))err=ENXIO;
    if(err && err!=EBUSY)p.state=3; // removal/install outcome no longer admits data
    fBeAwdlDataStats[14]=uint32_t(err);
    if(err && err!=EBUSY && !fBeAwdlDataStats[44]){fBeAwdlDataStats[44]=uint32_t(err);fBeAwdlDataStats[45]=phase;fBeAwdlDataStats[46]=command;fBeAwdlDataStats[47]=beAwdlNowNs();}
    return err;
}
bool ItlIwx::beAwdlDataPeerMatches(const uint8_t address[6],bool present) const
{
    if(!address)return false;
    if(!present && beAwdlDataRemovalComplete())return true;
    if(!beAwdlDataReady())return false;
    for(const auto &p:fBeAwdlDataPeers)
        if(p.state && !memcmp(p.address,address,6))return present && p.state==2;
    return !present; // zero slots, never an ambiguous/partially removed peer
}
int ItlIwx::beAwdlDataTransmit(mbuf_t m,uint64_t ticket)
{
    if(!m)return EINVAL;const auto *wh=static_cast<const uint8_t *>(mbuf_data(m));int q=-1;
    if(!getMainWorkLoop()->inGate()||!beAwdlDataReady()||!beAwdlRadioTxAllowed()||!be_awdl::txWindow(fBeAwdlWindowStats[1],fBeAwdlWindowStats[10],fBeAwdlWindowStats[12],beAwdlNowNs())||!ticket||mbuf_len(m)<80){beAwdlTxAdmissionEvent(3,ticket,EAGAIN,mbuf_data(m),mbuf_len(m));mbuf_freem(m);return EAGAIN;}
    if(wh[4]&1)q=fBeAwdlDataQueue;
    else for(const auto &p:fBeAwdlDataPeers)if(p.state==2&&!memcmp(wh+4,p.address,6)){q=p.queue;break;}
    if(!beAwdlDataQueueOwned(q)){beAwdlTxAdmissionEvent(4,ticket,ENOENT,mbuf_data(m),mbuf_len(m));mbuf_freem(m);return ENOENT;}
    auto &ring=com.txq[q];unsigned index=ring.cur&(ring.ring_count-1);
    if(ring.queued>=8||fBeAwdlDataSlots[q][index].ticket){beAwdlTxAdmissionEvent(5,ticket,EBUSY,mbuf_data(m),mbuf_len(m));mbuf_freem(m);return EBUSY;}
    auto &slot=fBeAwdlDataSlots[q][index];slot={ticket,-1,0};
    const uint32_t diagnosticGeneration=uint32_t(com.sc_generation),diagnosticEpoch=fPc1ResetEpoch;
    fBeAwdlTxDetail.begin(ticket,diagnosticGeneration,diagnosticEpoch,beAwdlNowNs(),q,index,
        wh,mbuf_len(m),fBeAwdlWindowStats[15],fBeAwdlWindowStats[12],fBeAwdlWindowStats[10],fBeAwdlGp2);
    const auto evidence=fBeAwdlWire.record(be_awdl::WireEvidence::TxData,mbuf_data(m),mbuf_len(m),
        beAwdlNowNs(),fBeAwdlGp2,44,com.sc_generation,fPc1ResetEpoch);
    auto *ni=com.sc_ic.ic_bss;ieee80211_ref_node(ni);const int err=iwx_tx(&com,m,ni,EDCA_AC_BE,q);
    fBeAwdlTxDetail.returned(ticket,diagnosticGeneration,diagnosticEpoch,beAwdlNowNs(),err);
    fBeAwdlWire.complete(evidence,uint32_t(err),1); // host enqueue only; real status stays separate
    if(err){slot.ticket=0;ieee80211_release_node(&com.sc_ic,ni);}else ++fBeAwdlDataStats[15];return err;
}
void ItlIwx::beAwdlDataDiagnosticCommand(unsigned q,unsigned index,const void *command,size_t bytes)
{
    static_assert(sizeof(iwx_cmd_header)+sizeof(iwx_tx_cmd_gen3)+sizeof(ieee80211_frame)==56,"reviewed gen3 prefix");
    if(!beAwdlDataQueueOwned(q)||index>=IWX_MIN_256_BA_QUEUE_SIZE_GEN3)return;
    const auto ticket=fBeAwdlDataSlots[q][index].ticket;if(!ticket)return;
    fBeAwdlTxDetail.command(ticket,uint32_t(com.sc_generation),fPc1ResetEpoch,beAwdlNowNs(),command,bytes);
}
void ItlIwx::beAwdlDataDiagnosticDoorbell(unsigned q,unsigned index)
{
    if(!beAwdlDataQueueOwned(q)||index>=IWX_MIN_256_BA_QUEUE_SIZE_GEN3)return;
    const auto ticket=fBeAwdlDataSlots[q][index].ticket;if(!ticket)return;
    fBeAwdlTxDetail.doorbell(ticket,uint32_t(com.sc_generation),fPc1ResetEpoch,beAwdlNowNs());
}
void ItlIwx::beAwdlDataDiagnosticResponse(struct iwx_rx_packet *pkt,struct iwx_tx_data *data)
{
    static_assert(sizeof(iwx_tx_resp)==44 && offsetof(iwx_tx_resp,status)==40,"reviewed v6 single response");
    static_assert(offsetof(iwx_rx_packet,data)==8,"reviewed RX envelope");
    // Called only from the real single-response path, BEFORE any status masking.
    const auto *r=reinterpret_cast<const iwx_tx_resp *>(pkt->data);
    const int q=le16toh(r->tx_queue);if(!beAwdlDataQueueOwned(q))return;
    const uintptr_t first=reinterpret_cast<uintptr_t>(&com.txq[q].data[0]),a=reinterpret_cast<uintptr_t>(data);
    if(a<first || a-first>=com.txq[q].ring_count*sizeof(*data)||(a-first)%sizeof(*data)) {
        fBeAwdlTxDetail.orphanResponse();return;
    }
    const unsigned index=(a-first)/sizeof(*data);
    const auto ticket=fBeAwdlDataSlots[q][index].ticket;
    if(!ticket){fBeAwdlTxDetail.orphanResponse();return;}
    fBeAwdlTxDetail.response(ticket,uint32_t(com.sc_generation),fPc1ResetEpoch,beAwdlNowNs(),
        pkt,sizeof(pkt->len_n_flags)+iwx_rx_packet_len(pkt));
}
void ItlIwx::beAwdlDataStatus(struct iwx_tx_resp *r,struct iwx_tx_data *data)
{
    const int q=le16toh(r->tx_queue);if(!beAwdlDataQueueOwned(q))return;
    const uintptr_t first=reinterpret_cast<uintptr_t>(&com.txq[q].data[0]),a=reinterpret_cast<uintptr_t>(data);
    if(a<first || a-first>=com.txq[q].ring_count*sizeof(*data)||(a-first)%sizeof(*data))return;
    const unsigned index=(a-first)/sizeof(*data);
    auto &s=fBeAwdlDataSlots[q][index];if(!s.ticket)return;
    s.status=le16toh(r->status.status)&IWX_TX_STATUS_MSK;
    s.firmwareDetail=uint32_t(q)|(uint32_t(index)<<8)|(uint32_t(r->failure_frame)<<16)|
        (uint32_t(r->ra_tid)<<24);
    fBeAwdlDataStats[51]=le32toh(r->initial_rate);
    ++fBeAwdlDataStats[16];fBeAwdlDataStats[17]=s.status;fBeAwdlDataStats[18]=r->failure_frame;
}
void ItlIwx::beAwdlDataRetire(int q,unsigned index,bool positive)
{
    if(!beAwdlDataQueueOwned(q)||index>=IWX_MIN_256_BA_QUEUE_SIZE_GEN3)return;
    auto &s=fBeAwdlDataSlots[q][index];if(!s.ticket)return;
    if(fBeAwdlResultCount>=16){++fBeAwdlDataStats[22];return;} // CPU original stays retained, never fake completion
    auto &out=fBeAwdlDataResults[fBeAwdlResultHead];
    out={s.ticket,s.status,s.firmwareDetail}; // -1 unless an actual TX response was recorded
    (void)positive;
    fBeAwdlTxDetail.retired(s.ticket,uint32_t(com.sc_generation),fPc1ResetEpoch,beAwdlNowNs(),s.status);
    fBeAwdlResultHead=(fBeAwdlResultHead+1)%16;++fBeAwdlResultCount;s.ticket=0;++fBeAwdlDataStats[19];
    if(out.status<0)++fBeAwdlDataStats[20];
}
bool ItlIwx::beAwdlDataPopResult(BeAwdlDataResult &out)
{
    if(!getMainWorkLoop()->inGate()||!fBeAwdlResultCount)return false;
    out=fBeAwdlDataResults[fBeAwdlResultTail];fBeAwdlResultTail=(fBeAwdlResultTail+1)%16;--fBeAwdlResultCount;return true;
}
void ItlIwx::beAwdlDataSnapshot(uint64_t out[64]) const
{
    memcpy(out,fBeAwdlDataStats,sizeof(fBeAwdlDataStats));out[3]=fBeAwdlDataStage;out[4]=fBeAwdlDataError;
    out[8]=fBeAwdlDataGeneration;out[9]=fBeAwdlDataEpoch;out[10]=BeAwdlDataMac;out[11]=uint32_t(fBeAwdlDataQueue);
    out[23]=beAwdlDataReady();out[24]=fBeAwdlSfHeld;out[25]=fBeAwdlResultCount;out[27]=uint32_t(fBeAwdlGoMgmtQueue);out[28]=fBeAwdlDataStage>=2?IWX_FW_MAC_TYPE_GO:0;out[29]=BeAwdlDataLink;
    out[50]=fBeAwdlDataPhyStage;
    for(unsigned i=0;i<BeAwdlPeerCount;++i){out[32+i*3]=fBeAwdlDataPeers[i].state;out[33+i*3]=uint32_t(fBeAwdlDataPeers[i].queue);out[34+i*3]=BeAwdlPeerFirst+i;}
}
// Linux v6.12 mld-sta::rm_int_sta -> flush_sta -> flush_sta_tids, then
// disable_txq (SCDv3 REMOVE) -> STA_REMOVE. Deliberately drain REAL completions
// first: shared tx2IRetire(false) quarantines mapped backing and closes global
// admission. A flush is NOT an ACK and must not be passed as a normal TX status.
// If real completions cannot drain, retain the role/backing and fail closed.
int ItlIwx::beAwdlDataFlush(unsigned station,int queue,unsigned tid)
{
    auto *sc=&com;auto *v=fBeAwdlDataLife;
    ++v[16];v[17]=station;v[18]=uint32_t(queue);v[19]=tid;v[20]=beAwdlNowNs();v[21]=v[22]=v[23]=0;
    auto finish=[&](int error){v[21]=uint32_t(error);v[24]=beAwdlNowNs();if(!error)++v[25];
        else if(!v[28]){v[28]=uint32_t(error);v[29]=v[24];v[30]=station;v[31]=uint32_t(queue);}return error;};
    if(!beAwdlDataQueueOwned(queue,station,tid))return finish(ENXIO);
    const int generation=fBeAwdlDataGeneration;const uint32_t epoch=fBeAwdlDataEpoch;
    int err=iwx_wait_txq_empty(sc,queue);
    if(err)return finish(err);
    if(!beAwdlLifetimeCurrent(sc,generation,epoch) || !beAwdlDataQueueOwned(queue,station,tid))return finish(ENXIO);
    if(!bzDevice()) {
        struct iwx_tx_path_flush_cmd payload={};payload.sta_id=htole32(station);payload.tid_mask=htole16(1U<<tid);
        return finish(beAwdlDataCommand(47,IWX_TXPATH_FLUSH,sizeof(payload),&payload,false));
    }
    if(iwx_lookup_cmd_ver(sc,IWX_LONG_GROUP,IWX_TXPATH_FLUSH)!=2 ||
       iwx_lookup_notif_ver(sc,IWX_LONG_GROUP,IWX_TXPATH_FLUSH)!=1)return finish(ENOTSUP);
    struct iwx_tx_path_flush_cmd payload={};payload.sta_id=htole32(station);payload.tid_mask=htole16(0xffff);
    struct iwx_host_cmd cmd={};cmd.id=IWX_TXPATH_FLUSH;cmd.len[0]=sizeof(payload);cmd.data[0]=&payload;
    cmd.flags=IWX_CMD_WANT_RESP;cmd.resp_pkt_len=sizeof(struct iwx_rx_packet)+sizeof(struct iwx_tx_path_flush_cmd_rsp);
    err=beAwdlDataCommand(47,cmd.id,sizeof(payload),&payload,false,&cmd);
    if(!err){
        auto *pkt=cmd.resp_pkt;
        if(!pkt || (pkt->hdr.group_id&IWX_CMD_FAILED_MSK) || iwx_rx_packet_payload_len(pkt)!=sizeof(struct iwx_tx_path_flush_cmd_rsp))err=EPROTO;
        else {
            auto *rsp=reinterpret_cast<const struct iwx_tx_path_flush_cmd_rsp *>(pkt->data);
            const unsigned n=le16toh(rsp->num_flushed_queues);v[22]=n;
            if(le16toh(rsp->sta_id)!=station || n>1)err=EPROTO; // exactly one TID/queue in this role
            for(unsigned i=0;!err && i<n;++i){const auto &r=rsp->queues[i];
                v[23]=uint64_t(le16toh(r.read_before_flush))|(uint64_t(le16toh(r.read_after_flush))<<32);
                auto &ring=sc->txq[queue];
                if(le16toh(r.tid)!=tid || le16toh(r.queue_num)!=queue || !ring.ring_count || ring.queued ||
                   IWX_AGG_SSN_TO_TXQ_IDX(le16toh(r.read_after_flush),ring.ring_count)!=ring.tail ||
                   IWX_AGG_SSN_TO_TXQ_IDX(le16toh(r.read_before_flush),ring.ring_count)!=ring.tail)err=EPROTO;
            }
        }
    }
    iwx_free_resp(sc,&cmd);
    // Response validation errors are distinct from send/lifetime errors.
    if(err && !fBeAwdlDataProofFrozen){fBeAwdlDataProof[10]=uint32_t(err);fBeAwdlDataProof[31]=1;fBeAwdlDataProofFrozen=true;}
    return finish(err);
}
// A global hardware stop is NOT required by GL92/Linux to deactivate one
// queue. It IS still required by our generic DMA-free quarantine. Preserve
// that policy: move proven-quiescent AWDL storage into a private bounded pool
// and reuse its existing DMA objects, rather than freeing/overwriting aliases.
int ItlIwx::beAwdlDataRetireQueue(unsigned station,int queue,unsigned tid)
{
    auto *v=fBeAwdlDataLife;++v[39];v[40]=station;v[41]=uint32_t(queue);
    auto finish=[&](int error){v[42]=uint32_t(error);v[43]=beAwdlNowNs();return error;};
    if(!getMainWorkLoop()->inGate() || !beAwdlDataOptIn || !fBeAwdlDataPreparing ||
       !beAwdlLifetimeCurrent(&com,fBeAwdlDataGeneration,fBeAwdlDataEpoch))return finish(ENXIO);
    if(queue<=IWX_DQA_CMD_QUEUE || queue>=32 || station>=32 ||
       !beAwdlDataQueueOwned(queue,station,tid))return finish(EINVAL);
    const uint32_t bit=1U<<station;
    if(!(fBeAwdlDataQueueRemoved&bit) || !(fBeAwdlDataStationRemoved&bit))return finish(EBUSY);
    auto &ring=com.txq[queue];
    if(!ring.ring_count || ring.ring_count>IWX_MIN_256_BA_QUEUE_SIZE_GEN3 || ring.queued || ring.qid!=queue ||
       !ring.desc_dma.cmd || !ring.cmd_dma.cmd || !ring.bc_tbl.cmd)return finish(EBUSY);
    // Drain completion and CPU-original ownership are distinct. Results may
    // remain for native delivery, but no descriptor/payload/ticket may remain.
    for(unsigned i=0;i<ring.ring_count;++i)
        if(ring.data[i].m || ring.data[i].in || ring.data[i].tx2I_token || fBeAwdlDataSlots[queue][i].ticket)return finish(EBUSY);
    for(unsigned t=0;t<IWX_MAX_TID_COUNT;++t)if(com.sc_tid_data[t].qid==queue)return finish(EBUSY);
    BeAwdlRetiredRing *slot=nullptr;for(auto &r:fBeAwdlRetired)if(!r.held){slot=&r;break;}
    if(!slot)return finish(ENOSPC);
    // No sleeping calls between proof and move; main gate owns both ledgers.
    memcpy(&slot->ring,&ring,sizeof(ring));slot->ring.qid=IWX_INVALID_QUEUE;
    slot->generation=fBeAwdlDataGeneration;slot->epoch=fBeAwdlDataEpoch;slot->held=true;
    memset(&ring,0,sizeof(ring));ring.qid=IWX_INVALID_QUEUE;
    com.qfullmsk&=~(1U<<queue);if(bzDevice())fBzQueueOwned&=~(1U<<queue);else fBeAwdlAxQueueOwned&=~(1U<<queue);
    if(queue==fBeAwdlDataQueue)fBeAwdlDataQueue=-1;
    if(queue==fBeAwdlGoMgmtQueue)fBeAwdlGoMgmtQueue=-1;
    // A removed peer is no longer a live QID reservation, even before its
    // caller clears/reuses the separate native-cache CPU record.
    for(auto &p:fBeAwdlDataPeers)if(p.queue==queue)p.queue=-1;
    ++v[44];++v[45];return finish(0);
}
int ItlIwx::beAwdlDataTakeRing(struct iwx_tx_ring *destination,uint32_t size)
{
    const bool discovery=fBeAwdlPreparing && fBeAwdlStage==5 && fBeAwdlGeneration==com.sc_generation && fBeAwdlEpoch==fPc1ResetEpoch;
    const int generation=discovery?fBeAwdlGeneration:fBeAwdlDataGeneration;
    const uint32_t epoch=discovery?fBeAwdlEpoch:fBeAwdlDataEpoch;
    if(destination!=&com.sc_tvqm_ring || (!discovery && !beAwdlDataAllocates(fBeAwdlDataAllocStation,fBeAwdlDataAllocTid)) ||
       !getMainWorkLoop()->inGate())return -EINVAL;
    if(destination->desc_dma.cmd || destination->cmd_dma.cmd || destination->bc_tbl.cmd)return -EBUSY;
    for(auto &slot:fBeAwdlRetired)if(slot.held){
        // A generation change without positive reset is not permission to
        // repurpose old storage. Actual positive stop uses Cleanup below.
        if(slot.generation!=uint32_t(generation) || slot.epoch!=epoch)return -ESTALE;
        if(slot.ring.ring_count!=size)continue;
        auto &old=slot.ring;
        if(old.queued || !old.desc_dma.cmd || !old.cmd_dma.cmd || !old.bc_tbl.cmd ||
           !old.desc_dma.vaddr || !old.cmd_dma.vaddr || !old.bc_tbl.vaddr)return -EBUSY;
        for(unsigned i=0;i<old.ring_count;++i)if(old.data[i].m || old.data[i].in || old.data[i].tx2I_token)return -EBUSY;
        memcpy(destination,&old,sizeof(old));memset(&old,0,sizeof(old));old.qid=IWX_INVALID_QUEUE;slot.held=false;
        // Existing maps/command addresses retain exactly one owner. Reinitialize
        // only fenced control bytes; shared allocator still synchronizes them,
        // applies the publication barrier and uses the real returned WRPTR.
        memset(destination->desc_dma.vaddr,0,destination->desc_dma.size);
        memset(destination->cmd_dma.vaddr,0,destination->cmd_dma.size);
        memset(destination->bc_tbl.vaddr,0,destination->bc_tbl.size);
        for(unsigned i=0;i<destination->ring_count;++i){destination->data[i].flags=0;destination->data[i].type=0;}
        destination->qid=IWX_INVALID_QUEUE;destination->cur=destination->tail=destination->queued=0;
        --fBeAwdlDataLife[45];fBeAwdlDataLife[58]=1;++fBeAwdlDataLife[63];return 1;
    }
    return 0; // No qualified pool entry: existing allocation path, no relaxed guard.
}
void ItlIwx::beAwdlDataRetiredCleanup()
{
    // Called only inside the existing positive global stop boundary.
    if(fBzHardwareOwned || !fPc1LastMaster || !fPc1Suspended ||
       fPc1TerminalEpoch!=fPc1ResetEpoch || !getMainWorkLoop()->inGate())return;
    for(auto &r:fBeAwdlRetired)if(r.held && tx5RingReleasable(&r.ring)){
        iwx_free_tx_ring(&com,&r.ring);
        if(!r.ring.desc_dma.cmd && !r.ring.cmd_dma.cmd && !r.ring.bc_tbl.cmd){r.held=false;if(fBeAwdlDataLife[45])--fBeAwdlDataLife[45];}
    }
}
void ItlIwx::beAwdlDataQueueNote(unsigned reason,int station,int tid,int queue,int error,uint32_t writePointer)
{
    auto *v=fBeAwdlDataLife;
    if(!reason){++v[46];v[56]=beAwdlNowNs();v[58]=0;}
    v[47]=uint32_t(station);v[48]=uint32_t(tid);v[49]=uint32_t(queue);v[50]=reason;v[51]=uint32_t(error);
    auto &temp=com.sc_tvqm_ring;v[52]=(temp.desc_dma.cmd?1U:0U)|(temp.cmd_dma.cmd?2U:0U)|(temp.bc_tbl.cmd?4U:0U);
    v[53]=fBzQueueOwned;v[54]=v[55]=0;
    if(queue>=0 && queue<32){const auto &r=com.txq[queue];v[54]=r.queued;
        for(unsigned i=0;i<r.ring_count && i<IWX_MIN_256_BA_QUEUE_SIZE_GEN3;++i)if(r.data[i].tx2I_token)++v[55];}
    v[57]=beAwdlNowNs();v[61]=writePointer;
    if(error && !v[59]){v[59]=reason;v[60]=v[57];v[62]=uint32_t(queue);}
}
int ItlIwx::beAwdlDataRemoveStation(unsigned station,int queue,unsigned tid,uint32_t phase)
{
    const uint32_t bit=1U<<station;
    if(fBeAwdlDataStationRemoved&bit)return queue<0?0:beAwdlDataRetireQueue(station,queue,tid);
    int err=0;
    if(!(fBeAwdlDataQueueRemoved&bit)){
        err=beAwdlDataFlush(station,queue,tid);if(err)return err;
        struct iwx_scd_queue_cfg_cmd q={};q.operation=htole32(IWX_SCD_QUEUE_REMOVE);q.u.remove.sta_mask=htole32(bit);q.u.remove.tid=htole32(tid);
        err=bzDevice()?beAwdlDataCommand(phase,iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD,IWX_DATA_PATH_GROUP,0),sizeof(q),&q,false):beAwdlAxQueueDisable(station,tid);
        if(err)return err;fBeAwdlDataQueueRemoved|=bit;fBeAwdlDataLife[26]=fBeAwdlDataQueueRemoved;
    }
    struct iwx_sta_remove_cmd c={htole32(station)};
    err=bzDevice()?beAwdlDataCommand(phase,iwx_cmd_id(IWX_STA_REMOVE_CMD,IWX_MAC_CONF_GROUP,0),sizeof(c),&c,false):beAwdlAxStation(station,2,nullptr,true);
    if(!err){fBeAwdlDataStationRemoved|=bit;fBeAwdlDataLife[27]=fBeAwdlDataStationRemoved;}
    return err?err:beAwdlDataRetireQueue(station,queue,tid);
}
int ItlIwx::beAwdlDataTeardown()
{
    if(!bzDevice())return beAwdlAxStop(true);
    if(!beAwdlOptIn || !fBeAwdlDataStage || fBeAwdlDataEpoch!=fPc1ResetEpoch)return 0;
    if(fBeAwdlDataStage==8)return fBeAwdlSfHeld?int(fBeAwdlDataStats[26]?fBeAwdlDataStats[26]:EBUSY):0;
    if(!getMainWorkLoop()->inGate() || fBeAwdlDataPreparing ||
       !beAwdlLifetimeCurrent(&com,fBeAwdlDataGeneration,fBeAwdlDataEpoch))return EBUSY;
    if(fBeAwdlWindowCommand || fBeAwdlWindowStats[1])return EBUSY; // actual END/terminal lifetime required, never synthesize one
    bool ambiguousPeer=false;for(const auto &p:fBeAwdlDataPeers)if(p.state && p.state!=2 && p.state!=4)ambiguousPeer=true;
    if(fBeAwdlDataStage!=7 || fBeAwdlDataError || ambiguousPeer){
        if(!fBeAwdlDataError)fBeAwdlDataError=ECANCELED;
        // If a known link exists, quiesce its offloaded beacon once even after
        // a later partial failure. Do not free/reuse ambiguous resources.
        if(fBeAwdlDataStage>=3 && !fBeAwdlDataLinkRemoved && !fBeAwdlDataQuiesceTried && beAwdlLifetimeCurrent(&com,fBeAwdlDataGeneration,fBeAwdlDataEpoch)){
            fBeAwdlDataQuiesceTried=true;struct iwx_link_config_cmd link={};link.action=htole32(IWX_FW_CTXT_ACTION_MODIFY);
            beAwdlDataLinkParameters(&link);link.modify_mask=htole32(IWX_LINK_CONTEXT_MODIFY_ACTIVE);
            fBeAwdlDataPreparing=true;
            fBeAwdlDataStats[48]=beAwdlDataCommand(44,iwx_cmd_id(IWX_LINK_CONFIG_CMD,IWX_MAC_CONF_GROUP,0),sizeof(link),&link,false);
            fBeAwdlDataPreparing=false;beAwdlControlDone();fBeAwdlDataStats[49]=beAwdlNowNs();
        }
        return EBUSY;
    }
    for(const auto &p:fBeAwdlDataPeers)if(p.state && p.state!=2)return EBUSY;
    fBeAwdlDataPreparing=true;fBeAwdlDataError=ECANCELED;
    const int generation=com.sc_generation;const uint32_t epoch=fPc1ResetEpoch;int err=0;
#define DATA_REMOVE(group,id,value) do {err=beAwdlDataCommand(45,iwx_cmd_id(id,group,0),sizeof(value),&(value),false);if(err)goto done;}while(0)
    // Native END has already closed the lease. Remove only MAC2's receive-
    // all-data admission before regular queue/station/link teardown. The
    // complete role definition is preserved; no global/AP filter operation.
    {struct iwx_mac_config_cmd m={};m.id_and_color=htole32(BeAwdlDataMac);m.action=htole32(IWX_FW_CTXT_ACTION_MODIFY);
     m.mac_type=htole32(IWX_FW_MAC_TYPE_GO);memcpy(m.local_mld_addr,fBeAwdlAddress,6);
     m.filter_flags=htole32(fBeAwdlDataFilter&~IWX_MAC_CFG_FILTER_PROMISC);
     err=beAwdlDataCommand(48,iwx_cmd_id(IWX_MAC_CONFIG_CMD,IWX_MAC_CONF_GROUP,0),sizeof(m),&m,false);
     if(err)goto done;fBeAwdlDataFilter=le32toh(m.filter_flags);}
    // Stop peers first, then Linux stop_ap's BCAST_MGMT before MCAST.
    // Removing MCAST while beacon/TBTT resources remain active was R7's
    // observed failing order. Assert causality still requires hardware proof.
    for(unsigned i=0;i<BeAwdlPeerCount;++i)if(fBeAwdlDataPeers[i].state==2){
        err=beAwdlDataRemoveStation(BeAwdlPeerFirst+i,fBeAwdlDataPeers[i].queue,BeAwdlPeerTid,45);
        if(err)goto done;fBeAwdlDataPeers[i].state=4;
    }
    err=beAwdlDataRemoveStation(BeAwdlGoMgmtStation,fBeAwdlGoMgmtQueue,IWX_MGMT_TID,45);if(err)goto done;
    err=beAwdlDataRemoveStation(BeAwdlMcastStation,fBeAwdlDataQueue,0,45);if(err)goto done;
    if(!beAwdlLifetimeCurrent(&com,generation,epoch)){err=ENXIO;goto done;}
    {struct iwx_link_config_cmd link={};beAwdlDataLinkParameters(&link);
    link.action=htole32(IWX_FW_CTXT_ACTION_MODIFY);link.modify_mask=htole32(IWX_LINK_CONTEXT_MODIFY_ACTIVE);
    DATA_REMOVE(IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,link);link.action=htole32(IWX_FW_CTXT_ACTION_REMOVE);DATA_REMOVE(IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,link);fBeAwdlDataLinkRemoved=true;}
    {struct iwx_mac_config_cmd mac={};mac.id_and_color=htole32(BeAwdlDataMac);mac.action=htole32(IWX_FW_CTXT_ACTION_REMOVE);DATA_REMOVE(IWX_MAC_CONF_GROUP,IWX_MAC_CONFIG_CMD,mac);}
    fBeAwdlDataStage=9; // MAC/link removed; PHY release still must complete
    err=beAwdlDataPhyRemove();if(err)goto done;
    fBeAwdlDataStage=8;
    // Restore only after actual removal, never after partial/ambiguous setup.
    // The normal helper clamps to UNINIT while held and is ASYNC; its0
    // would prove neither the requested state nor command completion. Restore
    // explicitly/synchronously only after every owned data resource is removed.
    {struct iwx_sf_cfg_cmd sf={};const bool run=com.sc_ic.ic_state==IEEE80211_S_RUN;
    sf.state=htole32(run?IWX_SF_FULL_ON:IWX_SF_INIT_OFF);
    iwx_fill_sf_command(&com,&sf,run?com.sc_ic.ic_bss:nullptr);
    err=fBeAwdlSfHeld?beAwdlDataCommand(46,IWX_REPLY_SF_CFG_CMD,sizeof(sf),&sf,false):0;}
    if(!err){fBeAwdlSfHeld=false;fBeAwdlDataError=0;}
done:
    fBeAwdlDataPreparing=false;beAwdlControlDone();fBeAwdlDataStats[26]=uint32_t(err);return err;
#undef DATA_REMOVE
}
