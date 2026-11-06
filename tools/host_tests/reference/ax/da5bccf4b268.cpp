// GPL-2.0; historical KGP AX regression fixture, not linked into the driver.
void ItlIwx::axHeDiagBa(struct iwx_softc *sc,const struct iwx_compressed_ba_notif *ba)
{
    if(!axHeDiagActive(sc)||!ba) return;
    uint32_t raw=le32toh(ba->tx_rate);
    if (ba->sta_id == IWX_STATION_ID && le16toh(ba->done) &&
        le32toh(ba->flags) == IWX_BA_RESP_TX_AGG)
        axTxRateNote(raw, 2);
    __atomic_fetch_add(&fAxHeDiagStats[AXD_BA],1ULL,__ATOMIC_RELAXED);
    __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_TXED],uint64_t(le16toh(ba->txed)),__ATOMIC_RELAXED);
    __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_DONE],uint64_t(le16toh(ba->done)),__ATOMIC_RELAXED);
    __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_RETRIES],uint64_t(ba->retry_cnt),__ATOMIC_RELAXED);
    __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_TFD],uint64_t(le16toh(ba->tfd_cnt)),__ATOMIC_RELAXED);
    __atomic_store_n(&fAxHeDiagStats[AXD_BA_RAW],raw,__ATOMIC_RELAXED);
    __atomic_store_n(&fAxHeDiagStats[AXD_BA_FLAGS],le32toh(ba->flags),__ATOMIC_RELAXED);
    if((raw&IWX_RATE_MCS_MOD_TYPE_MSK)!=IWX_RATE_MCS_HE_MSK) return;
    unsigned mcs=raw&IWX_RATE_MCS_CODE_MSK;
    unsigned gi=(raw&IWX_RATE_MCS_HE_GI_LTF_MSK)>>IWX_RATE_MCS_HE_GI_LTF_POS;
    unsigned bw=(raw&IWX_RATE_MCS_CHAN_WIDTH_MSK)>>IWX_RATE_MCS_CHAN_WIDTH_POS;
    unsigned nss=(raw&IWX_RATE_MCS_NSS_MSK)?1:0;
    if(mcs<12) __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_MCS+mcs],1ULL,__ATOMIC_RELAXED);
    if(gi<5) __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_GI+gi],1ULL,__ATOMIC_RELAXED);
    if(bw<4) __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_BW+bw],1ULL,__ATOMIC_RELAXED);
    __atomic_fetch_add(&fAxHeDiagStats[AXD_BA_NSS+nss],1ULL,__ATOMIC_RELAXED);
}
