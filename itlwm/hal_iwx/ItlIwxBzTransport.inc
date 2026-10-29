/* GL/BE200 ownership integration. KGP 2026, GPL-2.0-or-later.
 * Hardware ABI: pinned AirportBZ/Intel references; ZC28 owns backing/lifetimes.
 * See docs/BE200_PROVENANCE.md. No native WCL or allocator import. */
void ItlIwx::bzConnectionEvent(unsigned event,uint64_t a,uint64_t b,uint64_t c)
{
    if(!getMainWorkLoop() || !getMainWorkLoop()->inGate())return;
    fBzConnection.add(time2GNow(),com.sc_generation,fPc1ResetEpoch,event,a,b,c);
}
kgp_bz::ReplyRoute ItlIwx::bzRouteReply(struct iwx_softc *sc,unsigned qid,unsigned idx,int *code,unsigned length)
{
    using R=kgp_bz::ReplyRoute;
    // TX_CMD replies describe packet queues, even without the unsolicited bit.
    // Linux v6.9 mvm/ops.c no_reclaim_cmds + pcie/rx.c: never command-reclaim TX.
    if(*code==IWX_TX_CMD)return R::PacketTx;
    if(qid&0x80U)return R::Notification;
    if(qid!=IWX_DQA_CMD_QUEUE || idx>=IWX_CMD_QUEUE_SIZE_GEN3) {
        bzConnectionEvent(10,(qid<<16)|(idx<<24),length,*code);
        bzFail(3,EPROTO);return R::Invalid;
    }
    if(iwx_cmd_groupid(*code)==IWX_LONG_GROUP &&
       (sc->txq[qid].data[idx].flags&IWX_TXDATA_FLAG_CMD_IS_NARROW))
        *code=iwx_cmd_opcode(*code);
    if(!fBzCommands.expected(idx,*code,sc->sc_generation,fPc1ResetEpoch)) {
        bzConnectionEvent(10,(qid<<16)|(idx<<24),length,*code);
        bzFail(31,EPROTO);return R::Invalid;
    }
    return R::Command;
}
void ItlIwx::bzObserveMgmt(const void *bytes,size_t length)
{
    // CPU-owned, already synchronized/validated host frame. No addresses/keys.
    if(!bytes || length<24)return;
    const auto *p=static_cast<const uint8_t *>(bytes);
    if((p[0]&0x0c)!=0)return;
    const unsigned subtype=p[0]&0xf0;
    if(subtype==0x80) {++fBzScanBeacons;return;}
    if(subtype==0x50) {++fBzScanProbes;return;}
    if(p[1]&0x40)return; // Do not interpret protected management bodies.
    if(subtype==0xb0 && length>=30)
        bzConnectionEvent(6,uint32_t(kgp_bz::le16(p+24))|(uint32_t(kgp_bz::le16(p+26))<<16),kgp_bz::le16(p+28),subtype);
    else if((subtype==0x10 || subtype==0x30) && length>=30)
        bzConnectionEvent(7,kgp_bz::le16(p+26),kgp_bz::le16(p+28),subtype);
    else if((subtype==0xc0 || subtype==0xa0) && length>=26)
        bzConnectionEvent(8,kgp_bz::le16(p+24),subtype,0);
}
bool ItlIwx::bzAdmit()
{
    auto *s=&com;
    fBzAdmitted=kgp_bz::deviceAllowed(pci.pa_tag->configRead16(kIOPCIConfigVendorID),
        pci.pa_tag->configRead16(kIOPCIConfigDeviceID),CSR_HW_REV_TYPE(s->sc_hw_rev),
        IWX_CSR_HW_REV_STEP(s->sc_hw_rev),CSR_HW_RFID_TYPE(s->sc_hw_rf_id),
        CSR_HW_RFID_STEP(s->sc_hw_rf_id)) && s->sc_cfg &&
        s->sc_device_family==IWX_DEVICE_FAMILY_BZ && !s->sc_integrated;
    if(!fBzAdmitted) bzFail(6,ENODEV);
    return fBzAdmitted;
}
void ItlIwx::bzFail(uint32_t why,int error)
{
    uint32_t zero=0;
    if(__atomic_compare_exchange_n(&fBzFault,&zero,why,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))
    {
        fBzError=error;
        bzConnectionEvent(1,why,uint32_t(error),com.sc_ic.ic_state);
    }
    fBzRx.close();
    __atomic_store_n(&fTx2IBlocked,1U,__ATOMIC_RELEASE);
    com.sc_boot_diag.error=error;
    // Wake waiters as failed, never synthesize command completion or DMA stop.
    if(getMainWorkLoop() && getMainWorkLoop()->inGate()) {
        getMainCommandGate()->commandWakeup(&com.sc_uc.uc_intr);
        getMainCommandGate()->commandWakeup(&com.sc_init_complete);
        if(com.txq[0].desc) for(unsigned i=0;i<IWX_CMD_QUEUE_SIZE_GEN3;++i)
            getMainCommandGate()->commandWakeup(&com.txq[0].desc[i]);
    }
}
// Called under the command gate, including after a sleep/poll which can reenter.
// A completed flag must never override a fatal fault or a changed DMA lifetime.
int ItlIwx::bzWaitFailure(struct iwx_softc *sc,int generation,uint32_t epoch)
{
    if(generation!=sc->sc_generation || epoch!=fPc1ResetEpoch)return ENXIO;
    if(fBzFault)return fBzError ? int(fBzError) : EIO;
    return 0;
}
void ItlIwx::bzPnvmComplete(struct iwx_softc *sc,const void *payload,size_t length)
{
    if(!getMainWorkLoop()->inGate() || fBzFault)return;
    if(!payload || length!=sizeof(iwl_pnvm_init_complete_ntfy) ||
       !fBzPnvm.complete(sc->sc_generation,fPc1ResetEpoch,kgp_bz::le32(payload))) {
        bzFail(25,EPROTO);return;
    }
    // GL92 notification v1 is checked before boot. The upstream callback accepts
    // the matched completion and logs the status; no zero-only contract exists.
    sc->sc_pnvm_status=fBzPnvm.status;
    sc->sc_init_complete|=IWX_PNVM_COMPLETE;
    ++fBzStats[12];getMainCommandGate()->commandWakeup(&sc->sc_init_complete);
}
void ItlIwx::bzPublishInitCounters(struct iwx_softc *sc)
{
    // Boot-thread/gate snapshots only: no export/allocation in ISR or RX dispatch.
    if(!getMainWorkLoop()->inGate())return;
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_FILTER_COUNT",
        __atomic_load_n(&fBzStats[0],__ATOMIC_RELAXED),64);
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_ACTION_COUNT",fBzStats[1],64);
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_RX_CLAIMS",fBzStats[7],64);
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_ALIVE_COUNT",fBzStats[8],64);
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_PNVM_COUNT",fBzStats[12],64);
    sc->sc_pcitag->setProperty("KGP_BE200_ALIVE_SKU0",sc->sku_id[0],32);
    sc->sc_pcitag->setProperty("KGP_BE200_ALIVE_SKU1",sc->sku_id[1],32);
    sc->sc_pcitag->setProperty("KGP_BE200_ALIVE_SKU2",sc->sku_id[2],32);
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_GENERATION",sc->sc_generation,32);
    sc->sc_pcitag->setProperty("KGP_BE200_INIT_RESET_EPOCH",fPc1ResetEpoch,32);
}
bool ItlIwx::bzMasterStop(struct iwx_softc *sc)
{
    IWX_SETBITS(sc,IWX_CSR_GP_CNTRL,IWX_CSR_GP_CNTRL_REG_FLAG_BUS_MASTER_DISABLE_REQ);
    for(unsigned i=0;i<10000;++i) {
        const uint32_t value=IWX_READ(sc,IWX_CSR_GP_CNTRL);
        if(kgp_bz::masterStopped(value)) {++fBzStats[2];return true;}
        if(!kgp_bz::ioValid(value)) break;
        DELAY(10);
    }
    ++fBzStats[3];bzFail(7,ETIMEDOUT);
    return false; // Timeout/delay/invalid MMIO NEVER authorize reuse.
}
void ItlIwx::bzStoppedCleanup(struct iwx_softc *sc)
{
    if(fBzHardwareOwned || !fPc1LastMaster || !fPc1Suspended ||
       fPc1TerminalEpoch!=fPc1ResetEpoch || !getMainWorkLoop()->inGate()) return;
    iwx_ctxt_info_free_fw_img(sc);iwx_ctxt_info_free_paging(sc);
    if(sc->sc_tvqm_ring.desc_dma.cmd || sc->sc_tvqm_ring.cmd_dma.cmd || sc->sc_tvqm_ring.bc_tbl.cmd) {
        iwx_reset_tx_ring(sc,&sc->sc_tvqm_ring);iwx_free_tx_ring(sc,&sc->sc_tvqm_ring);
    }
    fBzStats[15]=fBzFault;fBzStats[16]=fBzError;
    fBzQueueOwned=0;fBzCommands.reset(true);fBzPnvm.reset(true);
    for(unsigned i=0;i<IWX_CMD_QUEUE_SIZE_GEN3;++i) sc->txq[0].data[i].flags &= ~IWX_TXDATA_FLAG_CMD_PENDING;
    fBzFault=fBzError=0; // ONLY positive stop under gate permits a new BZ generation.

    for(unsigned i=0;i<IWX_MAX_DRAM_ENTRY;++i) iwx_dma_contig_free(&sc->pnvm_frag_dma[i]);
    iwx_dma_contig_free(&sc->pnvm_desc_dma);
    // Context/prph/IML/PNVM common extents are freed by the ordinary stop path.
}
void ItlIwx::bzResetRx(struct iwx_softc *sc)
{
    const bool terminal=!fBzHardwareOwned && (!fBzRx.issued ||
        (fPc1LastMaster && fPc1Suspended && getMainWorkLoop()->inGate()));
    if(!terminal || !fBzRx.reset(sc->sc_generation,fPc1ResetEpoch,terminal)) {
        bzFail(8,EBUSY);return;
    }
    memset(sc->rxq.free_desc_dma.vaddr,0,sc->rxq.free_desc_dma.size);
    memset(sc->rxq.used_desc_dma.vaddr,0,sc->rxq.used_desc_dma.size);
    memset(sc->rxq.stat_dma.vaddr,0,sc->rxq.stat_dma.size);
    sc->rxq.cur=sc->rxq.write=0;
}
void ItlIwx::bzUpdateRxDesc(struct iwx_softc *sc,struct iwx_rx_ring *r,unsigned slot,unsigned dataIndex)
{
    if(slot!=fBzRx.write || dataIndex>=kgp_bz::RxCount || !fRxZcPool ||
        fRxZcSlot[dataIndex]>=kRxZcCount) {bzFail(9,EINVAL);return;}
    auto &entry=fRxZcPool->entry[fRxZcSlot[dataIndex]];
    if(entry.state!=KgpRxZcPool::Posted || !entry.command ||
        entry.command->getIOMemoryDescriptor()!=entry.memory ||
        !kgp_bz::dmaFits(entry.address,IWX_RBUF_SIZE,256) ||
        entry.command->synchronize(kIODirectionOut)!=kIOReturnSuccess ||
        !fBzRx.prepare(dataIndex,sc->sc_generation,fPc1ResetEpoch)) {bzFail(10,EIO);return;}
    auto *d=static_cast<iwx_rx_transfer_desc *>(r->free_desc_dma.vaddr);
    memset(&d[slot],0,sizeof(d[slot]));d[slot].addr=htole64(entry.address);
    d[slot].rbid=htole16(dataIndex+1);r->write=fBzRx.write;
}
void ItlIwx::bzPublishRx(struct iwx_softc *sc)
{
    if(fBzFault || !fBzHardwareOwned || !fRxZcRunning ||
       fRxZcGeneration!=unsigned(sc->sc_generation) || fRxZcEpoch!=fPc1ResetEpoch) return;
    unsigned credits=0;
    if(!sc->rxq.free_desc_dma.cmd ||
        sc->rxq.free_desc_dma.cmd->synchronize(kIODirectionOut)!=kIOReturnSuccess ||
        !fBzRx.publish(sc->sc_generation,fPc1ResetEpoch,&credits)) {bzFail(11,EIO);return;}
    iwx_dma_queue_publish_barrier();
    IWX_WRITE(sc,IWX_HBUS_TARG_WRPTR,IWX_HBUS_TARG_WRPTR_RX_Q(0)|credits);
    ++fBzStats[4];
}
void ItlIwx::bzNotif(struct iwx_softc *sc)
{
    if(!getMainWorkLoop()->inGate() || fBzFault || !fRxZcRunning ||
       __atomic_load_n(&fPc1Fault,__ATOMIC_ACQUIRE)) return;
    const unsigned generation=sc->sc_generation,epoch=fPc1ResetEpoch;
    const uint32_t cookie=fBzRx.begin(generation,epoch);
    if(!cookie) {++fBzStats[5];return;}
    struct mbuf_list ml=MBUF_LIST_INITIALIZER();
    auto *r=&sc->rxq;
    if(!r->stat_dma.cmd || r->stat_dma.cmd->synchronize(kIODirectionIn)!=kIOReturnSuccess ||
       !r->used_desc_dma.cmd || r->used_desc_dma.cmd->synchronize(kIODirectionIn)!=kIOReturnSuccess) {
        bzFail(12,EIO);fBzRx.end(cookie);return;
    }
    const unsigned hw=le16toh(*static_cast<uint16_t *>(r->stat)) & (kgp_bz::RxCount-1);
    const auto *cq=static_cast<const kgp_bz::Completion *>(r->used_desc_dma.vaddr);
    unsigned processed=0;
    while(r->cur!=hw && processed++<kgp_bz::RxCount) {
        // Copy only a completion descriptor after synchronizing; never log DMA pointers.
        const auto c=cq[r->cur];unsigned dataIndex=0;
        if(c.reserved || !kgp_bz::completeMessage(c.flags) ||
           !fBzRx.claim(le16toh(c.rbid),generation,epoch,&dataIndex)) {
            ++fBzStats[6];bzFail(13,EPROTO);break;
        }
        ++fBzStats[7];
        iwx_rx_pkt(sc,&r->data[dataIndex],&ml);
        if(fBzFault || !fBzRx.serviceCurrent(cookie,generation,epoch) || generation!=unsigned(sc->sc_generation) || epoch!=fPc1ResetEpoch) break;
        if(!rxZcRepost(dataIndex)) {bzFail(14,EIO);break;}
        bzUpdateRxDesc(sc,r,fBzRx.write,dataIndex);
        if(fBzFault) break;
        r->cur=(r->cur+1)&(kgp_bz::RxCount-1);
    }
    // Upper stack can reenter/reset: recheck lifetime before publishing credits.
    if_input(&sc->sc_ic.ic_if,&ml);
    if(!fBzFault && generation==unsigned(sc->sc_generation) && epoch==fPc1ResetEpoch &&
        fBzRx.serviceCurrent(cookie,generation,epoch)) bzPublishRx(sc);
    fBzRx.end(cookie);
}
void ItlIwx::bzCapture()
{
    if(pci.pa_tag) {
        pci.pa_tag->setProperty("KGP_BE200_TRANSPORT",fBzWire,sizeof(fBzWire));
        pci.pa_tag->setProperty("KGP_BE200_CONNECTION",fBzConnectionWire,sizeof(fBzConnectionWire));
    }
}
int ItlIwx::bzValidateFirmware()
{
    auto *s=&com;
    struct V {uint8_t group,id,version;bool notification,implicit;};
    const V versions[]={
        {IWX_LEGACY_GROUP,IWX_ALIVE,6,true,false},
        {IWX_LONG_GROUP,IWX_SCAN_REQ_UMAC,17,false,false},
        {IWX_MAC_CONF_GROUP,IWX_MAC_CONFIG_CMD,2,false,false},
        {IWX_MAC_CONF_GROUP,IWX_LINK_CONFIG_CMD,2,false,false},
        {IWX_MAC_CONF_GROUP,IWX_STA_CONFIG_CMD,1,false,true},
        {IWX_MAC_CONF_GROUP,IWX_STA_REMOVE_CMD,1,false,true},
        {IWX_DATA_PATH_GROUP,IWX_SEC_KEY_CMD,1,false,false},
        {IWX_DATA_PATH_GROUP,IWX_SCD_QUEUE_CONFIG_CMD,3,false,false},
        {IWX_MAC_CONF_GROUP,IWX_SESSION_PROTECTION_CMD,2,false,false},
        {IWX_LONG_GROUP,IWX_PHY_CONTEXT_CMD,5,false,false},
        {IWX_DATA_PATH_GROUP,IWX_RLC_CONFIG_CMD,2,false,false},
        {IWX_DATA_PATH_GROUP,IWX_TLC_MNG_CONFIG_CMD,4,false,false},
        {IWX_DATA_PATH_GROUP,IWX_TLC_MNG_UPDATE_NOTIF,3,true,false},
        {IWX_LONG_GROUP,IWX_TX_CMD,10,false,false},
        {IWX_LONG_GROUP,IWX_TX_CMD,7,true,false},
        {IWX_DATA_PATH_GROUP,IWX_RX_BAID_ALLOCATION_CONFIG_CMD,2,false,false},
        {IWX_REGULATORY_AND_NVM_GROUP,IWX_NVM_GET_INFO,4,true,false},
        {IWX_REGULATORY_AND_NVM_GROUP,IWX_PNVM_INIT_COMPLETE_NTFY,1,true,false},
    };
    if(!isset(s->sc_enabled_capa,IWX_UCODE_TLV_CAPA_MLD_API_SUPPORT)) return EPROTONOSUPPORT;
    for(const auto &v:versions) {
        const unsigned found=v.notification?iwx_lookup_notif_ver(s,v.group,v.id):iwx_lookup_cmd_ver(s,v.group,v.id);
        if(!kgp_bz::version(found,v.version,v.implicit)) {bzFail(15,EPROTONOSUPPORT);return EPROTONOSUPPORT;}
    }
    return 0;
}

int ItlIwx::bzInterrupt(struct iwx_softc *sc)
{
    __atomic_fetch_add(&fBzStats[1],1ULL,__ATOMIC_RELAXED);
    if(!getMainWorkLoop()->inGate() || !fBzAdmitted || !fDma5Selected ||
       fBzFault || __atomic_load_n(&fPc1Fault,__ATOMIC_ACQUIRE)) return 0;
    uint32_t fh=IWX_READ(sc,IWX_CSR_MSIX_FH_INT_CAUSES_AD);
    uint32_t hw=IWX_READ(sc,IWX_CSR_MSIX_HW_INT_CAUSES_AD);
    if(!kgp_bz::ioValid(fh)||!kgp_bz::ioValid(hw)) {bzFail(16,ENXIO);return 0;}
    IWX_WRITE(sc,IWX_CSR_MSIX_FH_INT_CAUSES_AD,fh);
    IWX_WRITE(sc,IWX_CSR_MSIX_HW_INT_CAUSES_AD,hw);
    fh &= sc->sc_fh_mask;hw &= sc->sc_hw_mask;
    if((fh & IWX_MSIX_FH_INT_CAUSES_FH_ERR) ||
       (hw & (IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR_V2|IWX_MSIX_HW_INT_CAUSES_REG_HW_ERR))) {
        bzFail(17,EIO);
        if(!(sc->sc_flags & IWX_FLAG_SHUTDOWN))task_add(systq,&sc->init_task);
        return 1;
    }
    // A coalesced hardware ALIVE/RX interrupt must publish initial credits
    // before validating returned tags. ALIVE is the RFH-ready handshake,
    // not the parsed firmware ALIVE notification or a DMA completion proof.
    if(hw & IWX_MSIX_HW_INT_CAUSES_REG_ALIVE) bzPublishRx(sc);
    if(fh & (IWX_MSIX_FH_INT_CAUSES_Q0|IWX_MSIX_FH_INT_CAUSES_Q1)) bzNotif(sc);
    if(hw & IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL) {
        iwx_check_rfkill(sc);if(!(sc->sc_flags & IWX_FLAG_SHUTDOWN))task_add(systq,&sc->init_task);
    }
    if(!fBzFault && !__atomic_load_n(&fPc1Fault,__ATOMIC_ACQUIRE))
        IWX_WRITE(sc,IWX_CSR_MSIX_AUTOMASK_ST_AD,1U); // OS-selected single vector0.
    return 1;
}

// P2: execute once per firmware boot, before hardware ownership/ROM start.
// Requested memory direction is not a valid synchronize() copy direction.
// Apple xnu f6217f891ac0 IODMACommand::synchronize rejects Out|In before
// checking preparation. Retain exact failing region/status even if attach
// cleanup removes the HAL. No live DMA memory or hot-path registry polling.
int ItlIwx::bzSyncBootMemory(struct iwx_softc *sc)
{
    struct Region { const char *name; iwx_dma_info *dma; };
    const Region regions[] = {
        {"context", &sc->ctxt_info_dma},
        {"scratch", &sc->prph_scratch_dma},
        {"peripheral-info", &sc->prph_info_dma},
        {"iml", &sc->iml_dma},
        {"rx-free", &sc->rxq.free_desc_dma},
        {"rx-completion", &sc->rxq.used_desc_dma},
        {"rx-status", &sc->rxq.stat_dma},
        {"command-descriptors", &sc->txq[IWX_DQA_CMD_QUEUE].desc_dma}
    };
    for (unsigned i = 0; i < sizeof(regions)/sizeof(regions[0]); ++i) {
        const auto &r = regions[i];
        IOReturn status = kIOReturnBadArgument;
        const char *reason = "missing-backing";
        if (r.dma->cmd && r.dma->buffer && r.dma->vaddr && r.dma->size) {
            reason = "backing-identity";
            if (r.dma->cmd->getIOMemoryDescriptor() == r.dma->buffer) {
                reason = "synchronize-out";
                status = r.dma->cmd->synchronize(kIODirectionOut);
            }
        }
        if (status != kIOReturnSuccess) {
            sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_INDEX", i, 32);
            sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_REGION", r.name);
            sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_REASON", reason);
            sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_RESULT", uint32_t(status), 32);
            bzFail(32, EIO);
            return EIO;
        }
    }
    sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_INDEX", 8U, 32);
    sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_REGION", "complete");
    sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_REASON", "synchronize-out");
    sc->sc_pcitag->setProperty("KGP_BE200_BOOT_SYNC_RESULT", static_cast<unsigned long long>(0), 32);
    return 0;
}
