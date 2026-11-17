/* SPDX-License-Identifier: GPL-2.0-or-later
 * R41 R30-private-PHY fixed-channel dispatch and pre-MLD AX backend.
 * Adapted from laobamac PR53 ff8db434cace0bf71d3b2991d07db8473378bbbd
 * P2P MAC/BINDING/ADD_STA. R49 replaces the AX LISTENER with a GO using
 * Intel/Linux v6.12 mvm-mac-ctxt/start_ap/sta contracts. R44 GL92 is unchanged.
 * This is functional adaptation, not byte-for-byte reuse.
 * R30 native frontend/transport remains shared. No Apple ABI in this backend.
 */
#include <Airport/BeAwdlAvailability.hpp>
#include <Airport/BeAwdlInfraIsolation.hpp>
#include <Airport/BeAwdlDataTransport.hpp>
static uint64_t beAwdlNowNs();

bool ItlIwx::beAwdlSupported() const
{
    // Native registration precedes FW command-table availability. Eligibility
    // does not claim readiness; the full contract is checked at Prepare.
    return fBzDevice?dma5Mapped():com.sc_device_family>=IWX_DEVICE_FAMILY_22000;
}
bool ItlIwx::beAwdlAxContract() const
{
    if(fBzDevice)return false;
    if(com.sc_device_family<IWX_DEVICE_FAMILY_22000)return false;
    auto *self=const_cast<ItlIwx *>(this);auto *sc=const_cast<iwx_softc *>(&com);
    const auto version=[&](unsigned group,unsigned cmd){return self->iwx_lookup_cmd_ver(sc,group,cmd);};
    const unsigned tx=version(IWX_LONG_GROUP,IWX_TX_CMD);
    // AX201/22000 uses TXv9 Gen2; AX210/211 use TXv10 Gen3. Both have
    // independently inspected embedded command tables; unknown layouts fail AWDL only.
    return version(IWX_LONG_GROUP,IWX_ADD_STA)==12 &&
        version(IWX_LONG_GROUP,IWX_PHY_CONTEXT_CMD)==3 &&
        version(IWX_LONG_GROUP,IWX_MAC_CONTEXT_CMD)==5 &&
        version(IWX_LONG_GROUP,IWX_BINDING_CONTEXT_CMD)==2 &&
        version(IWX_LONG_GROUP,IWX_SCD_QUEUE_CFG)==2 &&
        version(IWX_MAC_CONF_GROUP,IWX_SESSION_PROTECTION_CMD)==1 &&
        version(IWX_LONG_GROUP,IWX_TXPATH_FLUSH)==2 &&
        (version(IWX_LONG_GROUP,IWX_BEACON_TEMPLATE_CMD)==11 ||
         version(IWX_LONG_GROUP,IWX_BEACON_TEMPLATE_CMD)==12) &&
        isset(sc->sc_ucode_api,IWX_UCODE_TLV_API_STA_TYPE) &&
        (com.sc_device_family<IWX_DEVICE_FAMILY_AX210?tx==9:tx==10);
}
bool ItlIwx::beAwdlLifetimeCurrent(iwx_softc *sc,int generation,uint32_t epoch) const
{
    const auto *n=sc->sc_ic.ic_bss;
    const auto *ap=reinterpret_cast<const iwx_node *>(n);
    const bool epochCurrent=beAwdlOptIn && beAwdlSupported() && generation==sc->sc_generation && epoch==fPc1ResetEpoch &&
        !fPc1Fault && !fBzFault && !fBeAwdlDetail[6] && !(sc->sc_flags&(IWX_FLAG_SHUTDOWN|IWX_FLAG_HW_ERR|IWX_FLAG_RFKILL));
    if(fBeAwdlRemoving)return epochCurrent; // cleanup owns IDs, not the AP's newly selected host BSSID/chandef
    if(fBeAwdlAPChanging)return false;
    return epochCurrent &&
        !fPc1Fault && !fBzFault && !fBeAwdlDetail[6] &&
        !(sc->sc_flags&(IWX_FLAG_SHUTDOWN|IWX_FLAG_HW_ERR|IWX_FLAG_RFKILL)) &&
        (sc->sc_flags&(IWX_FLAG_MAC_ACTIVE|IWX_FLAG_BINDING_ACTIVE|IWX_FLAG_STA_ACTIVE))==(IWX_FLAG_MAC_ACTIVE|IWX_FLAG_BINDING_ACTIVE|IWX_FLAG_STA_ACTIVE) && n &&
        !memcmp(fBeAwdlApBssid,n->ni_bssid,6) && ap->in_phyctxt && ap->in_phyctxt->id==0 &&
        ap->in_phyctxt->channel && ap->in_phyctxt->channel!=IEEE80211_CHAN_ANYC &&
        ieee80211_chan2ieee(const_cast<ieee80211com *>(&sc->sc_ic),ap->in_phyctxt->channel)==44;
}
bool ItlIwx::beAwdlDataControlCurrent() const
{
    return com.sc_ic.ic_state==IEEE80211_S_RUN && beAwdlQueueOwned() && fBeAwdlActive && beAwdlLifetimeCurrent(
        const_cast<iwx_softc *>(&com),fBeAwdlGeneration,fBeAwdlEpoch);
}
bool ItlIwx::beAwdlRadioTxAllowed() const
{
    return beAwdlCurrent() && be_awdl::txWindow(fBeAwdlWindowStats[1],
        fBeAwdlWindowStats[10],fBeAwdlWindowStats[12],beAwdlNowNs());
}
int ItlIwx::beAwdlAxMac(unsigned id,uint32_t action)
{
    if(bzDevice() || (id!=1 && id!=2) || !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))return ENXIO;
    iwx_mac_ctx_cmd c={};c.id_and_color=htole32(IWX_FW_CMD_ID_AND_COLOR(id,0));c.action=htole32(action);
    if(action!=IWX_FW_CTXT_ACTION_REMOVE){
        c.mac_type=htole32(id==1?IWX_FW_MAC_TYPE_P2P_DEVICE:IWX_FW_MAC_TYPE_GO);
        c.tsf_id=htole32(id==1?IWX_TSF_ID_B:IWX_TSF_ID_C);memcpy(c.node_addr,fBeAwdlAddress,6);
        if(id==1)c.node_addr[5]^=0x80; // existing separate discovery broker
        static const uint8_t bssid[]={0,0x25,0,0xff,0x94,0x73};memcpy(c.bssid_addr,bssid,6);
        c.ofdm_rates=htole32(0x15);
        if(id==1)c.filter_flags=htole32(IWX_MAC_FILTER_ACCEPT_GRP|IWX_MAC_FILTER_IN_CONTROL_AND_MGMT);
        else {
            // Intel mac_tsf_id_iter: a GO follows the infrastructure TSF if
            // their beacon intervals divide. The P2P device keeps its own B.
            const auto *ap=com.sc_ic.ic_bss;const unsigned bi=be_awdl::DataBeaconIntervalTu;
            if(!ap || !ap->ni_intval)return EAGAIN;
            const unsigned smaller=MIN(unsigned(ap->ni_intval),bi);
            if((unsigned(ap->ni_intval)>bi?unsigned(ap->ni_intval)-bi:bi-unsigned(ap->ni_intval))%smaller==0)
                c.tsf_id=htole32(IWX_TSF_ID_A);
            memcpy(c.bssid_addr,fBeAwdlAddress,6); // AP identity matches its actual beacon
            c.filter_flags=htole32(IWX_MAC_FILTER_ACCEPT_GRP|IWX_MAC_FILTER_IN_BEACON|IWX_MAC_FILTER_IN_PROBE_REQUEST);
            c.short_slot=htole32(IWX_MAC_FLG_SHORT_SLOT);
            for(unsigned ac=0;ac<IWX_AC_NUM;++ac){c.ac[ac].cw_min=htole16(15);c.ac[ac].cw_max=htole16(1023);
                c.ac[ac].aifsn=2;c.ac[ac].fifos_mask=1U<<(ac+1);}
            c.ac[IWX_AC_VO].fifos_mask|=1U<<IWX_TX_FIFO_MCAST;
            c.go.ap.beacon_time=htole32(fBeAwdlAxGoBeaconTime);
            c.go.ap.bi=htole32(bi);
            c.go.ap.dtim_interval=htole32(bi*be_awdl::DataDtimPeriod);
            // These old header names are reserved1/reserved2 in the selected
            // Intel MAC_CONTEXT v5 AP layout; leave both zero (Linux v6.12).
            c.go.ap.beacon_template=htole32(id); // STA_TYPE API: mcast_qid is obsolete
            // No WMM/HT, opportunistic power save or CT window is advertised.
        }
    }
    beAwdlEvent(1,60,IWX_MAC_CONTEXT_CMD);
    int e=iwx_send_cmd_pdu(&com,IWX_MAC_CONTEXT_CMD,0,sizeof(c),&c);
    if(!e && !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))e=ENXIO;
    if(!e){if(action==IWX_FW_CTXT_ACTION_REMOVE)fBeAwdlAxMacMask&=~(1U<<id);else fBeAwdlAxMacMask|=1U<<id;}
    beAwdlEvent(2,60,IWX_MAC_CONTEXT_CMD,e);return e;
}
int ItlIwx::beAwdlAxGoTiming()
{
    const auto *ap=com.sc_ic.ic_bss;const uint64_t now=beAwdlNowNs();
    if(bzDevice() || !ap || !ap->ni_intval || !beAwdlCurrent() ||
       fBeAwdlAxGoClockGeneration!=com.sc_generation || fBeAwdlAxGoClockEpoch!=fPc1ResetEpoch ||
       memcmp(fBeAwdlAxGoClockAp,ap->ni_bssid,6) || !fBeAwdlAxGoClockHost || now<fBeAwdlAxGoClockHost ||
       now-fBeAwdlAxGoClockHost>2000000000ULL)return EAGAIN;
    // RX TSF is synchronized to this AP. Place initial GO TBTT halfway
    // between its next two TBTTs (Linux chooses 36..63%); preserve on MODIFY.
    const uint64_t elapsed=(now-fBeAwdlAxGoClockHost)/1000ULL;
    const uint32_t interval=uint32_t(ap->ni_intval)*1024U;
    const uint32_t delta=interval-uint32_t((fBeAwdlAxGoClockTsf+elapsed)%interval)+interval/2;
    fBeAwdlAxGoBeaconTime=fBeAwdlAxGoClockGp2+uint32_t(elapsed)+delta;
    return 0;
}
void ItlIwx::beAwdlAxGoObserve(const uint8_t *f,size_t n,unsigned channel,uint32_t gp2,uint64_t tsf,uint16_t phy)
{
    const auto *ap=com.sc_ic.ic_bss;
    // Only the current infrastructure AP supplies this synchronized clock.
    // This receipt never changes native association/rate/duplicate state.
    if(bzDevice() || fBeAwdlAPChanging || com.sc_ic.ic_state!=IEEE80211_S_RUN || !ap || !ap->ni_port_valid ||
       !f || n<24 || channel!=44 || ((f[0]&0x0c)!=0 && (f[0]&0x0c)!=8) ||
       memcmp(f+10,ap->ni_bssid,6) || ((f[1]&3)==0 && memcmp(f+16,ap->ni_bssid,6)) ||
       (phy&IWX_RX_MPDU_PHY_TSF_OVERLOAD))return;
    fBeAwdlAxGoClockGp2=gp2;fBeAwdlAxGoClockTsf=tsf;fBeAwdlAxGoClockHost=beAwdlNowNs();
    fBeAwdlAxGoClockGeneration=com.sc_generation;fBeAwdlAxGoClockEpoch=fPc1ResetEpoch;
    memcpy(fBeAwdlAxGoClockAp,ap->ni_bssid,6);
}
int ItlIwx::beAwdlAxGoBeacon()
{
    // Intel BEACON_TEMPLATE v11/v12: same header size as v13, but this
    // field is a MAC/template ID, NOT a GL92 link ID. New-rate 6Mbps = 0.
    struct {be_awdl::Beacon13 header;uint8_t frame[54];} __packed b={};
    b.header.bytes=htole16(be_awdl::dataBeacon(b.frame,sizeof(b.frame),fBeAwdlAddress));
    b.header.link=htole32(2);b.header.timIndex=htole32(48);b.header.timSize=htole32(4);
    return beAwdlDataCommand(63,IWX_BEACON_TEMPLATE_CMD,sizeof(b),&b);
}
bool ItlIwx::beAwdlAxGoBeaconReply(const iwx_softc *sc,unsigned qid,unsigned idx) const
{
    return beAwdlOptIn && !bzDevice() && fBeAwdlDataPreparing &&
        fBeAwdlDataEpoch==fPc1ResetEpoch && fBeAwdlDataGeneration==sc->sc_generation &&
        qid==IWX_DQA_CMD_QUEUE && idx<sc->txq[IWX_DQA_CMD_QUEUE].ring_count;
}
int ItlIwx::beAwdlAxBinding(unsigned privateMask)
{
    if(bzDevice() || (privateMask&~6U) || !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))return ENXIO;
    auto *phy=&com.sc_phyctxt[1];
    if(phy->id!=1 || fBeAwdlDataPhyStage!=4)return EBUSY;
    if(privateMask==fBeAwdlAxBindingMask)return 0;
    struct iwx_binding_cmd c={};c.id_and_color=htole32(IWX_FW_CMD_ID_AND_COLOR(1,phy->color));
    c.phy=c.id_and_color;c.action=htole32(!privateMask?IWX_FW_CTXT_ACTION_REMOVE:
        fBeAwdlAxBindingMask?IWX_FW_CTXT_ACTION_MODIFY:IWX_FW_CTXT_ACTION_ADD);
    for(auto &m:c.macs)m=htole32(IWX_FW_CTXT_INVALID);
    unsigned slot=0;for(unsigned id=1;id<=2;++id)if(privateMask&(1U<<id))c.macs[slot++]=htole32(id);
    c.lmac_id=htole32(iwx_lmac_id(&com,phy->channel));uint32_t status=0;
    beAwdlEvent(1,61,IWX_BINDING_CONTEXT_CMD);
    int e=iwx_send_cmd_pdu_status(&com,IWX_BINDING_CONTEXT_CMD,sizeof(c),&c,&status);
    if(!e && status)e=EIO;
    if(!e && !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))e=ENXIO;
    if(!e)fBeAwdlAxBindingMask=privateMask;
    beAwdlEvent(2,61,IWX_BINDING_CONTEXT_CMD,e);return e;
}
int ItlIwx::beAwdlAxStation(unsigned id,unsigned mac,const uint8_t *address,bool remove)
{
    if(bzDevice() || id<3 || id>9 || (mac!=1 && mac!=2) ||
       !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))return ENXIO;
    int e=0;beAwdlEvent(1,62,remove?IWX_REMOVE_STA:IWX_ADD_STA);
    if(remove){struct iwx_rm_sta_cmd c={};c.sta_id=id;e=iwx_send_cmd_pdu(&com,IWX_REMOVE_STA,0,sizeof(c),&c);}
    else {struct iwx_add_sta_cmd c={};c.sta_id=id;c.mac_id_n_color=htole32(mac);
        c.station_type=address?IWX_STA_LINK:(mac==2 && id==4?IWX_STA_MULTICAST:IWX_STA_GENERAL_PURPOSE);
        c.tid_disable_tx=htole16(0xffff);
        if(address)memcpy(c.addr,address,6);else if(mac==2 && id==4)c.addr[0]=3;else memset(c.addr,255,6);
        uint32_t status=IWX_ADD_STA_SUCCESS;e=iwx_send_cmd_pdu_status(&com,IWX_ADD_STA,sizeof(c),&c,&status);
        if(!e && (status&IWX_ADD_STA_STATUS_MASK)!=IWX_ADD_STA_SUCCESS)e=EIO;}
    if(!e && !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))e=ENXIO;
    if(!e){if(remove)fBeAwdlAxStationMask&=~(1U<<id);else fBeAwdlAxStationMask|=1U<<id;}
    beAwdlEvent(2,62,remove?IWX_REMOVE_STA:IWX_ADD_STA,e);return e;
}
int ItlIwx::beAwdlAxQueueDisable(unsigned station,unsigned tid)
{
    if(bzDevice() || station<3 || station>9 || !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))return ENXIO;
    iwx_tx_queue_cfg_cmd c={};c.sta_id=station;c.tid=tid; // ENABLE flag cleared, old API disable
    int e=iwx_send_cmd_pdu(&com,IWX_SCD_QUEUE_CFG,0,sizeof(c),&c);
    return e?e:beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch)?0:ENXIO;
}
int ItlIwx::beAwdlAxPrepare(const uint8_t address[6])
{
    auto *sc=&com;auto *ni=sc->sc_ic.ic_bss;
    if(!beAwdlOptIn || !beAwdlSupported() || !beAwdlAxContract() || !getMainWorkLoop()->inGate())return ENOTSUP;
    if(!address || (address[0]&1) || !memcmp(address,"\0\0\0\0\0\0",6) || !memcmp(address,sc->sc_ic.ic_myaddr,6))return EINVAL;
    if(fBeAwdlPreparing || fBeAwdlAPChanging)return EBUSY;if(fBeAwdlDetail[6])return ECANCELED;
    if(!ni || !ni->ni_port_valid || !(ni->ni_associd&0x3fff) || sc->sc_ic.ic_state!=IEEE80211_S_RUN ||
       (sc->sc_flags&(IWX_FLAG_MAC_ACTIVE|IWX_FLAG_BINDING_ACTIVE|IWX_FLAG_STA_ACTIVE))!=(IWX_FLAG_MAC_ACTIVE|IWX_FLAG_BINDING_ACTIVE|IWX_FLAG_STA_ACTIVE) ||
       !ni->ni_chan || ni->ni_chan==IEEE80211_CHAN_ANYC || ieee80211_chan2ieee(&sc->sc_ic,ni->ni_chan)!=44)return EAGAIN;
    if(!fBeAwdlCleanRemoved && fBeAwdlEpoch==fPc1ResetEpoch && fBeAwdlGeneration==sc->sc_generation){
        if(memcmp(address,fBeAwdlAddress,6))return EBUSY;
        return fBeAwdlActive?0:int(fBeAwdlError?fBeAwdlError:EIO);}
    if(!fBeAwdlCleanRemoved && (fBeAwdlAxMacMask || fBeAwdlAxStationMask || fBeAwdlAxQueueOwned) && !fBeAwdlAxTerminal)return EIO;
    fBeAwdlWindowStats[1]=0;memset(fBeAwdlNativeTickets,0,sizeof(fBeAwdlNativeTickets));if(!fBeAwdlCleanRemoved)fBeAwdlActionResults.clear();
    fBeAwdlCleanRemoved=false;fBeAwdlHomeWidth=ni->ni_chw;fBeAwdlAxTerminal=false;
    fBeAwdlPreparing=true;fBeAwdlActive=false;fBeAwdlGeneration=sc->sc_generation;fBeAwdlEpoch=fPc1ResetEpoch;
    fBeAwdlAxQueueOwned=fBeAwdlAxStationMask=fBeAwdlAxMacMask=fBeAwdlAxBindingMask=0;
    fBeAwdlPhy=1;fBeAwdlStage=1;fBeAwdlQueue=-1;fBeAwdlGp2Host=0;fBeAwdlGp2=0;
    memcpy(fBeAwdlAddress,address,6);memcpy(fBeAwdlApBssid,ni->ni_bssid,6);ieee80211_ref_node(ni);
    int e=beAwdlAxPhyPrepare();if(!e)e=beAwdlAxMac(1,IWX_FW_CTXT_ACTION_ADD);
    if(!e){fBeAwdlStage=2;e=beAwdlAxBinding(2);}
    if(!e){fBeAwdlStage=4;e=beAwdlAxStation(3,1,nullptr,false);}
    if(!e){fBeAwdlStage=5;const int q=iwx_tvqm_enable_txq(sc,IWX_MGMT_TID,0,beAwdlQueueSize(),3);
        if(q<0)e=-q;else fBeAwdlQueue=q;}
    if(!e && !beAwdlLifetimeCurrent(sc,fBeAwdlGeneration,fBeAwdlEpoch))e=ENXIO;
    if(!e){fBeAwdlActive=true;fBeAwdlStage=6;}
    ieee80211_release_node(&sc->sc_ic,ni);fBeAwdlError=e;fBeAwdlPreparing=false;beAwdlControlDone();return e;
}
int ItlIwx::beAwdlAxDataPrepare()
{
    const auto *ap=com.sc_ic.ic_bss;
    if(!beAwdlDataOptIn || !beAwdlCurrent() || !ap || !ap->ni_port_valid || !(ap->ni_associd&0x3fff))return EAGAIN;if(fBeAwdlDataPreparing)return EBUSY;
    if(!beAwdlDataRemovalComplete() && fBeAwdlDataGeneration==com.sc_generation && fBeAwdlDataEpoch==fPc1ResetEpoch)
        return beAwdlDataReady()?0:int(fBeAwdlDataError?fBeAwdlDataError:EIO);
    if(fBeAwdlWindowCommand || fBeAwdlWindowStats[1])return EAGAIN;
    if(!be_awdl::dataChannelAllowed(fBeAwdlReg44,fBeAwdlRegGeneration==com.sc_generation,true,true))return EACCES;
    const int timing=beAwdlAxGoTiming();if(timing)return timing; // no GO owners before a genuine clock anchor
    fBeAwdlDataPreparing=true;fBeAwdlDataGeneration=com.sc_generation;fBeAwdlDataEpoch=fPc1ResetEpoch;
    fBeAwdlDataQueueRemoved=fBeAwdlDataStationRemoved=0;fBeAwdlDataQueue=fBeAwdlGoMgmtQueue=-1;
    memset(fBeAwdlDataSlots,0,sizeof(fBeAwdlDataSlots));fBeAwdlResultHead=fBeAwdlResultTail=fBeAwdlResultCount=0;
    fBeAwdlDataError=0;fBeAwdlDataStage=1;for(auto &p:fBeAwdlDataPeers)p=BeAwdlDataPeer{};
    int e=beAwdlDataPhyPrepare();
    if(!e && !isset(com.sc_ucode_api,IWX_UCODE_TLV_API_SMART_FIFO_OFFLOAD)){
        struct iwx_sf_cfg_cmd sf={};sf.state=htole32(IWX_SF_UNINIT);iwx_fill_sf_command(&com,&sf,nullptr);
        e=beAwdlDataCommand(20,IWX_REPLY_SF_CFG_CMD,sizeof(sf),&sf);}
    // Intel start_ap: 22000 template first; AX210+ MAC first. Never copy
    // GL92 LINK activation order into the pre-MLD MAC_CONTEXT backend.
    if(!e && com.sc_device_family<=IWX_DEVICE_FAMILY_22000)e=beAwdlAxGoBeacon();
    if(!e)e=beAwdlAxMac(2,IWX_FW_CTXT_ACTION_ADD);
    if(!e && com.sc_device_family>IWX_DEVICE_FAMILY_22000)e=beAwdlAxGoBeacon();
    if(!e){fBeAwdlDataStage=2;e=beAwdlAxBinding(6);}
    if(!e){fBeAwdlDataStage=5;e=beAwdlAxStation(4,2,nullptr,false);}
    if(!e){fBeAwdlDataStage=6;fBeAwdlDataAllocStation=4;fBeAwdlDataAllocTid=0;
        const int q=iwx_tvqm_enable_txq(&com,0,0,beAwdlQueueSize(),4);if(q<0)e=-q;else fBeAwdlDataQueue=q;}
    if(!e)e=beAwdlAxStation(9,2,nullptr,false);
    if(!e){fBeAwdlDataAllocStation=9;fBeAwdlDataAllocTid=IWX_MGMT_TID;
        const int q=iwx_tvqm_enable_txq(&com,IWX_MGMT_TID,0,beAwdlQueueSize(),9);if(q<0)e=-q;else fBeAwdlGoMgmtQueue=q;}
    if(!e && !beAwdlLifetimeCurrent(&com,fBeAwdlDataGeneration,fBeAwdlDataEpoch))e=ENXIO;
    if(!e){fBeAwdlDataStage=7;fBeAwdlDataFilter=IWX_MAC_FILTER_ACCEPT_GRP|IWX_MAC_FILTER_IN_BEACON|IWX_MAC_FILTER_IN_PROBE_REQUEST;
        fBeAwdlDataFilterKnown=true;fBeAwdlDataStats[58]=be_awdl::DataBeaconIntervalTu;fBeAwdlDataStats[59]=be_awdl::DataDtimPeriod;}
    // Same bounded, once-per-setup CPU receipt as GL92. Preserve original
    // MAC/TSF/binding bytes before recurring window commands replace them.
    memcpy(fAwdlR42Activation,fAwdlR42Commands,sizeof(fAwdlR42Activation));
    fBeAwdlDataAllocStation=fBeAwdlDataAllocTid=-1;fBeAwdlDataPreparing=false;beAwdlControlDone();fBeAwdlDataError=e;return e;
}
int ItlIwx::beAwdlAxStop(bool dataOnly)
{
    if(!beAwdlOptIn || (!fBeAwdlAxMacMask && !fBeAwdlAxStationMask))return 0;
    if(!beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch) || fBeAwdlDataPreparing || fBeAwdlPreparing)return EBUSY;
    if(fBeAwdlWindowCommand || fBeAwdlWindowStats[1])return EBUSY; // genuine END required
    if(fBeAwdlDataStage && fBeAwdlDataStage!=7 && fBeAwdlDataStage!=8)return EBUSY; // partial ownership retained
    int e=0;fBeAwdlDataPreparing=true;
    for(unsigned i=0;i<BeAwdlPeerCount;++i)if(fBeAwdlDataPeers[i].state){
        if(fBeAwdlDataPeers[i].state!=2 && fBeAwdlDataPeers[i].state!=4){e=EBUSY;break;}
        if(fBeAwdlDataPeers[i].state==2){e=beAwdlDataRemoveStation(5+i,fBeAwdlDataPeers[i].queue,BeAwdlPeerTid,65);if(e)break;fBeAwdlDataPeers[i].state=4;}}
    if(!e && (fBeAwdlAxStationMask&(1U<<9)))e=beAwdlDataRemoveStation(9,fBeAwdlGoMgmtQueue,IWX_MGMT_TID,65);
    if(!e && (fBeAwdlAxStationMask&(1U<<4)))e=beAwdlDataRemoveStation(4,fBeAwdlDataQueue,0,65);
    if(!e && (fBeAwdlAxMacMask&4)){e=beAwdlAxBinding(fBeAwdlAxBindingMask&~4U);if(!e)e=beAwdlAxMac(2,IWX_FW_CTXT_ACTION_REMOVE);}
    if(!e && fBeAwdlDataStage){fBeAwdlDataLinkRemoved=true;fBeAwdlDataStage=8;fBeAwdlDataError=0;}
    if(!e && fBeAwdlSfHeld){struct iwx_sf_cfg_cmd sf={};sf.state=htole32(IWX_SF_FULL_ON);iwx_fill_sf_command(&com,&sf,com.sc_ic.ic_bss);
        e=beAwdlDataCommand(46,IWX_REPLY_SF_CFG_CMD,sizeof(sf),&sf,false);if(!e)fBeAwdlSfHeld=false;}
    fBeAwdlDataPreparing=false;beAwdlControlDone();if(e || dataOnly)return e;
    fBeAwdlPreparing=true;fBeAwdlActive=false;
    if(fBeAwdlQueue>=0){e=iwx_wait_txq_empty(&com,fBeAwdlQueue);if(!e)e=beAwdlAxQueueDisable(3,IWX_MGMT_TID);}
    if(!e && (fBeAwdlAxStationMask&8))e=beAwdlAxStation(3,1,nullptr,true);
    if(!e && fBeAwdlAxBindingMask)e=beAwdlAxBinding(0);
    if(!e && (fBeAwdlAxMacMask&2))e=beAwdlAxMac(1,IWX_FW_CTXT_ACTION_REMOVE);
    if(!e)e=beAwdlRetireDiscoveryQueue();
    if(!e){fBeAwdlStage=7;fBeAwdlDataPhyStage=2;fBeAwdlCleanRemoved=true;}fBeAwdlError=e;fBeAwdlPreparing=false;beAwdlControlDone();return e;
}

bool ItlIwx::beAwdlAxRx(iwx_softc *sc,mbuf_t m,iwx_rx_mpdu_desc *desc,ieee80211_rxinfo *rxi)
{
    auto *ic=&sc->sc_ic;const auto *f=static_cast<const uint8_t *>(mbuf_data(m));const size_t n=mbuf_len(m);
    const bool gen3=sc->sc_device_family>=IWX_DEVICE_FAMILY_AX210;
    const unsigned channel=gen3?desc->v3.channel:desc->v1.channel;
    const uint32_t gp2=le32toh(gen3?desc->v3.gp2_on_air_rise:desc->v1.gp2_on_air_rise);
    beAwdlAxGoObserve(f,n,channel,gp2,le64toh(gen3?desc->v3.tsf_on_air_rise:desc->v1.tsf_on_air_rise),le16toh(desc->phy_info));
    if(beAwdlObserve(f,n,channel,iwx_rxmq_get_signal_strength(sc,desc),gp2,
        (desc->mac_flags2&IWX_RX_MPDU_MFLG2_AMSDU)!=0))return true;
    if(ic->ic_opmode!=IEEE80211_M_STA || !ic->ic_bss || !n)return false;
    const auto scope=be_awdl::InfraIsolation::managementScope(f,n,ic->ic_myaddr,ic->ic_bss->ni_bssid);
    if(scope==be_awdl::InfraIsolation::Foreign){beAwdlInfraRxOwnerNote(scope,f,n,le32toh(desc->status),desc->mac_phy_idx,channel,1);return true;}
    if(scope==be_awdl::InfraIsolation::ScanOnly){rxi->rxi_flags|=IEEE80211_RXI_SCAN_ONLY;return false;}
    if(be_awdl::InfraIsolation::foreignData(f,n,ic->ic_myaddr,ic->ic_bss->ni_bssid))return true;
    const uint32_t status=le32toh(desc->status);
    if((status&IWX_RX_MPDU_RES_STATUS_SRC_STA_FOUND) &&
       ((status&IWX_RX_MPDU_RES_STATUS_STA_ID_MSK)>>24)!=IWX_STATION_ID)return true;
    return false;
}

uint64_t ItlIwx::beAwdlCommandBegin(const iwx_host_cmd *c)
{
    if(!beAwdlOptIn || !getMainWorkLoop()->inGate() || fAwdlR42Commands[6] ||
       !(fBeAwdlPreparing || fBeAwdlDataPreparing || fBeAwdlWindowCommand))return 0;
    auto *h=fAwdlR42Commands;const uint64_t serial=++h[3];if(serial>32)++h[4];
    auto *r=h+16+((serial-1)%32)*56;memset(r,0,56*8);r[0]=serial;r[1]=beAwdlNowNs();
    r[3]=c->id;r[4]=iwx_lookup_cmd_ver(&com,(c->id>>8)&255?(c->id>>8)&255:IWX_LONG_GROUP,c->id&255);
    r[5]=c->flags;r[7]=com.sc_generation;r[8]=fPc1ResetEpoch;r[9]=UINT32_MAX;
    r[10]=com.txq[IWX_DQA_CMD_QUEUE].cur;r[11]=uint64_t(fBeAwdlStage)|(uint64_t(fBeAwdlDataStage)<<32);
    r[12]=fBeAwdlPhy;r[13]=com.sc_flags;
    size_t n=0;for(unsigned i=0;i<nitems(c->len);++i){if(c->data[i] && n<256){const size_t k=MIN(size_t(c->len[i]),256-n);memcpy(reinterpret_cast<uint8_t *>(r+16)+n,c->data[i],k);n+=k;}r[6]+=c->len[i];}
    h[5]=serial;return serial;
}
void ItlIwx::beAwdlCommandEnd(uint64_t serial,const iwx_host_cmd *c,int error)
{
    if(!serial)return;auto *r=fAwdlR42Commands+16+((serial-1)%32)*56;
    if(r[0]!=serial)return;r[2]=beAwdlNowNs();r[9]=uint32_t(error);
    r[14]=com.sc_generation;r[15]=fPc1ResetEpoch;
    if(c->resp_pkt){uint32_t header=0;memcpy(&header,&c->resp_pkt->hdr,4);r[5]|=uint64_t(header)<<32;const size_t bytes=MIN(size_t(iwx_rx_packet_payload_len(c->resp_pkt)),size_t(56));
        r[48]=bytes;memcpy(r+49,c->resp_pkt->data,bytes);}
}

int ItlIwx::beAwdlAxPhyPrepare()
{
    if(bzDevice() || !beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch))return EAGAIN;
    if(fBeAwdlDataPhyStage==4 && fBeAwdlDataPhyEpoch==fPc1ResetEpoch &&
       fBeAwdlDataPhyGeneration==uint32_t(com.sc_generation))return 0;
    auto &phy=com.sc_phyctxt[1];auto *target=&com.sc_ic.ic_channels[44];
    if(phy.id!=1 || phy.ref || fBeAwdlAxBindingMask || !target->ic_freq)return EBUSY;
    fBeAwdlDataPhyStage=1;fBeAwdlDataPhyEpoch=fPc1ResetEpoch;fBeAwdlDataPhyGeneration=com.sc_generation;
    // Intel v6.12 phy-ctxt.c: CDB band change requires REMOVE/ADD, unbound.
    const bool recreate=phy.channel && IEEE80211_IS_CHAN_2GHZ(phy.channel)!=IEEE80211_IS_CHAN_2GHZ(target) &&
        isset(com.sc_enabled_capa,IWX_UCODE_TLV_CAPA_BINDING_CDB_SUPPORT);
    auto send=[&](uint32_t action,ieee80211_channel *channel)->int{
        struct iwx_phy_context_cmd c={};c.id_and_color=htole32(IWX_FW_CMD_ID_AND_COLOR(1,phy.color));c.action=htole32(action);
        c.lmac_id=htole32(iwx_lmac_id(&com,channel));c.ci.band=IEEE80211_IS_CHAN_2GHZ(channel)?IWX_PHY_BAND_24:IWX_PHY_BAND_5;
        c.ci.channel=htole32(ieee80211_chan2ieee(&com.sc_ic,channel));c.ci.width=IWX_PHY_VHT_CHANNEL_MODE20;
        const unsigned chains=iwx_num_of_ant(iwx_fw_valid_rx_ant(&com))>1?2:1;
        c.rxchain_info=htole32((iwx_fw_valid_rx_ant(&com)<<IWX_PHY_RX_CHAIN_VALID_POS)|
            (chains<<IWX_PHY_RX_CHAIN_CNT_POS)|(chains<<IWX_PHY_RX_CHAIN_MIMO_CNT_POS));
        int e=iwx_send_cmd_pdu(&com,IWX_PHY_CONTEXT_CMD,0,sizeof(c),&c);
        return e?e:beAwdlLifetimeCurrent(&com,fBeAwdlGeneration,fBeAwdlEpoch)?0:ENXIO;};
    int e=recreate?send(IWX_FW_CTXT_ACTION_REMOVE,phy.channel):0;if(e)return e;
    e=send(recreate?IWX_FW_CTXT_ACTION_ADD:IWX_FW_CTXT_ACTION_MODIFY,target);if(e)return e;
    phy.channel=target;fBeAwdlDataPhyStage=4;return 0;
}

bool ItlIwx::beAwdlHasPrivateOwners() const
{
    if(!beAwdlOptIn)return false;
    return fBeAwdlPreparing || fBeAwdlDataPreparing || fBeAwdlAPCommands ||
        (fBeAwdlStage && fBeAwdlStage!=7) || (fBeAwdlDataStage && fBeAwdlDataStage!=8) ||
        fBeAwdlAxMacMask || fBeAwdlAxStationMask || fBeAwdlAxBindingMask;
}
bool ItlIwx::beAwdlAPMutation(const iwx_host_cmd *c) const
{
    if(!beAwdlOptIn || !c || !c->data[0] || !c->len[0])return false;
    const unsigned id=c->id&255,group=(c->id>>8)&255;
    uint32_t first=UINT32_MAX;if(c->len[0]>=4)memcpy(&first,c->data[0],4);first=le32toh(first);
    if((group==0 || group==IWX_LONG_GROUP) && id==IWX_PHY_CONTEXT_CMD)return (first&255)==0;
    if(group==IWX_DATA_PATH_GROUP && id==IWX_RLC_CONFIG_CMD)return first==0;
    if(group==IWX_MAC_CONF_GROUP && id==IWX_MAC_CONFIG_CMD)return (first&255)==0;
    if(group==IWX_MAC_CONF_GROUP && id==IWX_STA_REMOVE_CMD)return first==0;
    if(group==IWX_MAC_CONF_GROUP && id==IWX_LINK_CONFIG_CMD && c->len[0]>=sizeof(iwx_link_config_cmd)){
        const auto *p=static_cast<const iwx_link_config_cmd *>(c->data[0]);
        return le32toh(p->link_id)==0 && (le32toh(p->action)!=IWX_FW_CTXT_ACTION_MODIFY ||
            (le32toh(p->modify_mask)&IWX_LINK_CONTEXT_MODIFY_ACTIVE) || le32toh(p->phy_id)==IWX_FW_CTXT_INVALID);
    }
    if(group==0 || group==IWX_LONG_GROUP){
        if(id==IWX_BINDING_CONTEXT_CMD)return (first&255)==0; // private binding is ID1, never AP0
        if(id==IWX_REMOVE_STA)return *static_cast<const uint8_t *>(c->data[0])==0;
        if(id==IWX_MAC_CONTEXT_CMD && (first&255)==0 && c->len[0]>=sizeof(iwx_mac_ctx_cmd)){
            const auto *p=static_cast<const iwx_mac_ctx_cmd *>(c->data[0]);
            return le32toh(p->action)!=IWX_FW_CTXT_ACTION_MODIFY || !le32toh(p->sta.is_assoc);
        }
    }
    return false;
}
void ItlIwx::beAwdlControlDone()
{
    if(fBeAwdlPreparing || fBeAwdlDataPreparing)return;
    fBeAwdlControlThread=0;
    getMainCommandGate()->commandWakeup(&fBeAwdlPreparing);
}
int ItlIwx::beAwdlInfrastructureWillChange()
{
    if(!getMainWorkLoop()->inGate())return EWOULDBLOCK;
    fBeAwdlAPChanging=true;
    if(!beAwdlHasPrivateOwners())return 0;
    // closes TX and startup BEFORE inspecting any positive owner mask
    if(fBeAwdlAPCommands)return 0; // same AP transaction, private admission remains closed
    const int generation=com.sc_generation;const uint32_t epoch=fPc1ResetEpoch;
    uint32_t budget=1000; // existing firmware command-wait budget, not an AirDrop retry extension
    while(fBeAwdlPreparing || fBeAwdlDataPreparing){
        if(getMainWorkLoop()->onThread() || fBeAwdlControlThread==reinterpret_cast<uintptr_t>(IOThreadSelf()))return EBUSY;
        const int e=iwx_gate_sleep(&com,&fBeAwdlPreparing,&budget,IWX_GATE_WAIT_NOTIF);if(e)return e;
    }
    if(generation!=com.sc_generation || epoch!=fPc1ResetEpoch)return ENXIO;
    while(fBeAwdlWindowCommand || fBeAwdlWindowStats[1]){
        if(fBeAwdlWindowStats[1]>=4 && fBeAwdlWindowStats[1]!=6)return EBUSY;
        if(getMainWorkLoop()->onThread())return EBUSY;
        const int e=iwx_gate_sleep(&com,&fBeAwdlWindowStats[1],&budget,IWX_GATE_WAIT_NOTIF);if(e)return e;
    }
    if(generation!=com.sc_generation || epoch!=fPc1ResetEpoch ||
       fBeAwdlGeneration!=generation || fBeAwdlEpoch!=epoch)return ENXIO;
    const bool old=fBeAwdlRemoving;fBeAwdlRemoving=true;
    const int e=beAwdlTeardown();fBeAwdlRemoving=old;
    beAwdlEvent(2,70,0,e);return e;
}
int ItlIwx::beAwdlRetireDiscoveryQueue()
{
    const int q=fBeAwdlQueue;if(q<0)return 0;
    if(!fBeAwdlPreparing || !getMainWorkLoop()->inGate() || q<=0 || q>=32 ||
       fBeAwdlGeneration!=com.sc_generation || fBeAwdlEpoch!=fPc1ResetEpoch)return ENXIO;
    auto &r=com.txq[q];if(r.queued || !r.ring_count || r.ring_count>IWX_MIN_256_BA_QUEUE_SIZE_GEN3)return EBUSY;
    for(unsigned i=0;i<r.ring_count;++i)if(r.data[i].m || r.data[i].in || r.data[i].tx2I_token || fBeAwdlNativeTickets[i])return EBUSY;
    for(unsigned t=0;t<IWX_MAX_TID_COUNT;++t)if(com.sc_tid_data[t].qid==q)return EBUSY;
    BeAwdlRetiredRing *slot=nullptr;for(auto &p:fBeAwdlRetired)if(!p.held){slot=&p;break;}if(!slot)return ENOSPC;
    memcpy(&slot->ring,&r,sizeof(r));slot->ring.qid=IWX_INVALID_QUEUE;slot->generation=fBeAwdlGeneration;slot->epoch=fBeAwdlEpoch;slot->held=true;
    memset(&r,0,sizeof(r));r.qid=IWX_INVALID_QUEUE;
    if(bzDevice())fBzQueueOwned&=~(1U<<q);else fBeAwdlAxQueueOwned&=~(1U<<q);
    com.qfullmsk&=~(1U<<q);fBeAwdlQueue=-1;++fBeAwdlDataLife[45];return 0;
}

void ItlIwx::beAwdlAxStoppedCleanup()
{
    // Only called after actual bus-master stop + processor reset on the main gate.
    fBeAwdlAxTerminal=true;
    fBeAwdlAxGoClockHost=0;fBeAwdlAxGoClockGeneration=-1;fBeAwdlAxGoBeaconTime=0;
    for(unsigned q=1;q<32;++q)if(fBeAwdlAxQueueOwned&(1U<<q))iwx_reset_tx_ring(&com,&com.txq[q]);
    if(com.sc_tvqm_ring.desc_dma.cmd || com.sc_tvqm_ring.cmd_dma.cmd || com.sc_tvqm_ring.bc_tbl.cmd){
        iwx_reset_tx_ring(&com,&com.sc_tvqm_ring);iwx_free_tx_ring(&com,&com.sc_tvqm_ring);}
    for(auto &r:fBeAwdlRetired)if(r.held && tx5RingReleasable(&r.ring)){
        iwx_reset_tx_ring(&com,&r.ring);iwx_free_tx_ring(&com,&r.ring);r=BeAwdlRetiredRing{};}
    fBeAwdlDataLife[45]=0;fBeAwdlAxQueueOwned=fBeAwdlAxMacMask=fBeAwdlAxStationMask=fBeAwdlAxBindingMask=0;
    fBeAwdlAPChanging=false;fBeAwdlAPCommands=0;fBeAwdlControlThread=0;fBeAwdlCleanRemoved=false;
    fBeAwdlActive=false;fBeAwdlStage=fBeAwdlDataStage=0; // real global stop invalidates old firmware owners
}
