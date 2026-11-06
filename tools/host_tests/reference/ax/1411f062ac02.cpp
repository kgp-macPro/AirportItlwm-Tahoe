// GPL-2.0; historical KGP AX regression fixture, not linked into the driver.
void ItlIwx::
iwx_ba_task(void *arg)
{
    if (container_of(static_cast<iwx_softc *>(arg), ItlIwx, com)->bzDevice()) return iwx_bz_port_ba_task(arg);

    struct iwx_softc *sc = (struct iwx_softc *)arg;
    struct ieee80211com *ic = &sc->sc_ic;
    ItlIwx *that = container_of(sc, ItlIwx, com);
    struct ieee80211_node *ni = ic->ic_bss;
    struct ieee80211_tx_ba *ba;
    struct iwx_tx_ring *ring;
    int s = splnet();
    int err = 0;
    int qid = 0;
    int tid;
    
    if (sc->sc_flags & IWX_FLAG_SHUTDOWN) {
        //        refcnt_rele_wake(&sc->task_refs);
        splx(s);
        return;
    }

    for (tid = 0; tid < IWX_MAX_TID_COUNT; tid++) {
        if (sc->sc_flags & IWX_FLAG_SHUTDOWN)
            break;
        if (sc->ba_rx.start_tidmask & (1 << tid)) {
            struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
            XYLog("%s ba_rx_start tid=%d, ssn=%d\n", __FUNCTION__, tid, ba->ba_winstart);
            that->iwx_sta_rx_agg(sc, ni, tid, ba->ba_winstart,
                                 ba->ba_winsize, ba->ba_timeout_val, 1);
            sc->ba_rx.start_tidmask &= ~(1 << tid);
        } else if (sc->ba_rx.stop_tidmask & (1 << tid)) {
            that->iwx_sta_rx_agg(sc, ni, tid, 0, 0, 0, 0);
            sc->ba_rx.stop_tidmask &= ~(1 << tid);
        }
    }
    
    for (tid = 0; tid < IWX_MAX_TID_COUNT; tid++) {
        if (sc->sc_flags & IWX_FLAG_SHUTDOWN)
            break;
        if (sc->ba_tx.start_tidmask & (1 << tid)) {
            // ZC21: one failed TID must not poison a later successful request.
            err = 0;
            bool nicLocked = false;
            ba = &ni->ni_tx_ba[tid];
            XYLog("%s ba_tx_start tid=%d, ssn=%d\n", __FUNCTION__, tid, ba->ba_winstart);
            if (!that->iwx_nic_lock(sc)) {
                err = -EBUSY;
                goto out;
            }
            nicLocked = true;
            if ((qid = that->iwx_tvqm_alloc_txq(sc, tid, ba->ba_winstart)) < 0) {
                err = qid;
                goto out;
            }
            ring = &sc->txq[qid];
            ba->ba_winstart = IWX_AGG_SSN_TO_TXQ_IDX(ring->cur, ring->ring_count);
            ba->ba_winend = (ba->ba_winstart + ba->ba_winsize - 1) & 0xfff;
            ba->ba_timeout_val = 0;
            ieee80211_addba_resp_accept(ic, ni, tid);
            XYLog("%s tx queue alloc succeed qid=%d ssn=%d\n", __FUNCTION__, qid, ba->ba_winstart);
        out:
            if (nicLocked) that->iwx_nic_unlock(sc);
            if (err)
                ieee80211_addba_resp_refuse(ic, ni, tid,
                                            IEEE80211_STATUS_UNSPECIFIED);
            sc->ba_tx.start_tidmask &= ~(1 << tid);
        }
    }
    
    //    refcnt_rele_wake(&sc->task_refs);
    splx(s);
}
