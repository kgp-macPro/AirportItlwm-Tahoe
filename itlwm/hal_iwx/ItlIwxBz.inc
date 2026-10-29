/*
 * Copyright (C) 2020  Zhong Xianyao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*    $OpenBSD: if_iwx.c,v 1.45 2020/10/11 07:05:28 mpi Exp $    */

/*
 * Copyright (c) 2014, 2016 genua gmbh <info@genua.de>
 *   Author: Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2014 Fixup Software Ltd.
 * Copyright (c) 2017, 2019, 2020 Stefan Sperling <stsp@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 ******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// Private GL/BZ source port. Selective HAL reference: HuajiFruit/AirportBZ
// 0ccab1627764406861fb9eb0a06ca36e31b4440f. Existing GPL/BSD/Intel notices apply.
// Native WCL and reference DMA arena are not imported. See docs/BE200_PROVENANCE.md.
#include <HAL/ItlBzTxReference.hpp>
#include "iwx_boot_checks.h"
#define IWX_FW_ADDR_CACHE_CONTROL 0xC0000000
static uint64_t iwx_boot_time_ns() { uint64_t n; absolutetime_to_nanoseconds(mach_absolute_time(), &n); return n; }
static void ItlStartupStage(IOService *p,const char *s) { if(p) p->setProperty("KGP_BE200_STAGE",s); }
static void ItlStartupFailure(IOService *p,const char *s,int e) { if(p) { p->setProperty("KGP_BE200_STAGE",s);p->setProperty("KGP_BE200_ERROR",uint32_t(e),32); } }
static void iwx_bz_record_pci_irq_mode(struct iwx_softc *sc, bool before_fw)
{
    /* IOKit's PCIMessaged type alone does not distinguish MSI from MSI-X.
     * Read the PCI capability controls; never change OS-owned routing here. */
    const int caps[] = { PCI_CAP_MSI, PCI_CAP_MSIX };
    const char *before_names[] = { "ItlwmBootPreRomMsiCapability", "ItlwmBootPreRomMsixCapability" };
    const char *after_names[] = { "ItlwmBootEndMsiCapability", "ItlwmBootEndMsixCapability" };
    for (unsigned i = 0; i < 2; i++) {
        int offset = 0;
        const char *name = before_fw ? before_names[i] : after_names[i];
        if (pci_get_capability(sc->sc_pct, sc->sc_pcitag, caps[i], &offset, NULL))
            sc->sc_pcitag->setProperty(name,
                static_cast<UInt32>(pci_conf_read(sc->sc_pct, sc->sc_pcitag, offset)), 32);
        else
            sc->sc_pcitag->removeProperty(name);
    }
}
static void
iwx_publish_boot_dma(IOService *provider, const char *name,
                     const struct iwx_dma_info *dma, uint32_t device_offset = 0)
{
    char key[64], value[128];
    snprintf(key, sizeof(key), "ItlwmDma%s", name);
    snprintf(value, sizeof(value), "iova=0x%llx bytes=0x%llx deviceOffset=0x%x",
             (unsigned long long)dma->paddr, (unsigned long long)dma->size,
             device_offset);
    provider->setProperty(key, value);
}
bool ItlIwx::
iwx_has_new_station_api(struct iwx_softc *sc)
{
    /* Linux also selects the new station API for MLD-capable firmware. */
    const uint8_t cmdver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_ADD_STA);
    return isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_MLD_API_SUPPORT) ||
        (cmdver != IWX_FW_CMD_VER_UNKNOWN && cmdver >= 12);
}

bool ItlIwx::
iwx_use_mld_api(struct iwx_softc *sc)
{
    /*
     * MLD firmware does not advertise MAC_CONTEXT_CMD, BINDING_CONTEXT_CMD,
     * ADD_STA, REMOVE_STA or TX_QUEUE_CFG; association must use
     * MAC_CONFIG/LINK_CONFIG/STA_CONFIG and SCD_QUEUE_CONFIG instead.
     */
    return isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_MLD_API_SUPPORT);
}

int ItlIwx::
iwx_bz_port_init_fw_sec(struct iwx_softc *sc, const struct iwx_fw_sects *fws,
                struct iwx_context_info_dram *ctxt_dram)
{
    struct iwx_self_init_dram *dram = &sc->init_dram;
    int i, ret, fw_cnt = 0;

    KASSERT(dram->paging == NULL, "dram->paging == NULL");

    dram->lmac_cnt = iwx_get_num_sections(fws, 0);
    /* add 1 due to separator */
    dram->umac_cnt = iwx_get_num_sections(fws, dram->lmac_cnt + 1);
    /* add 2 due to separators */
    dram->paging_cnt = iwx_get_num_sections(fws,
    dram->lmac_cnt + dram->umac_cnt + 2);
    /* The firmware table can hold more sections than each context DRAM array. */
    if (dram->lmac_cnt > IWX_MAX_DRAM_ENTRY ||
        dram->umac_cnt > IWX_MAX_DRAM_ENTRY ||
        dram->paging_cnt > IWX_MAX_DRAM_ENTRY) {
        XYLog("%s: firmware section group exceeds context DRAM capacity (%d/%d/%d)\n",
              DEVNAME(sc), dram->lmac_cnt, dram->umac_cnt, dram->paging_cnt);
        return EINVAL;
    }

    dram->fw = (struct iwx_dma_info *)kcalloc(dram->umac_cnt + dram->lmac_cnt, sizeof(*dram->fw));
    if (!dram->fw) {
        XYLog("%s: could not allocate memory for firmware sections\n",
        DEVNAME(sc));
        return ENOMEM;
    }
    dram->paging = (struct iwx_dma_info *)kcalloc(dram->paging_cnt, sizeof(*dram->paging));
    if (!dram->paging) {
        XYLog("%s: could not allocate memory for firmware paging\n",
        DEVNAME(sc));
        return ENOMEM;
    }

    /* initialize lmac sections */
    for (i = 0; i < dram->lmac_cnt; i++) {
        ret = iwx_ctxt_info_alloc_dma(sc, &fws->fw_sect[i],
                           &dram->fw[fw_cnt]);
        if (ret)
            return ret;
        ctxt_dram->lmac_img[i] =
            htole64(dram->fw[fw_cnt].paddr);
        DPRINTFN(3, ("%s: firmware LMAC section %d at 0x%llx size %lld\n", __func__, i,
            (unsigned long long)dram->fw[fw_cnt].paddr,
            (unsigned long long)dram->fw[fw_cnt].size));
        fw_cnt++;
    }

    /* initialize umac sections */
    for (i = 0; i < dram->umac_cnt; i++) {
        /* access FW with +1 to make up for lmac separator */
        ret = iwx_ctxt_info_alloc_dma(sc,
            &fws->fw_sect[fw_cnt + 1], &dram->fw[fw_cnt]);
        if (ret)
            return ret;
        ctxt_dram->umac_img[i] =
            htole64(dram->fw[fw_cnt].paddr);
        DPRINTFN(3, ("%s: firmware UMAC section %d at 0x%llx size %lld\n", __func__, i,
            (unsigned long long)dram->fw[fw_cnt].paddr,
            (unsigned long long)dram->fw[fw_cnt].size));
        fw_cnt++;
    }

    /*
     * Initialize paging.
     * Paging memory isn't stored in dram->fw as the umac and lmac - it is
     * stored separately.
     * This is since the timing of its release is different -
     * while fw memory can be released on alive, the paging memory can be
     * freed only when the device goes down.
     * Given that, the logic here in accessing the fw image is a bit
     * different - fw_cnt isn't changing so loop counter is added to it.
     */
    for (i = 0; i < dram->paging_cnt; i++) {
        /* access FW with +2 to make up for lmac & umac separators */
        int fw_idx = fw_cnt + i + 2;

        ret = iwx_ctxt_info_alloc_dma(sc,
            &fws->fw_sect[fw_idx], &dram->paging[i]);
        if (ret)
            return ret;

        ctxt_dram->virtual_img[i] = htole64(dram->paging[i].paddr);
        DPRINTFN(3, ("%s: firmware paging section %d at 0x%llx size %lld\n", __func__, i,
            (unsigned long long)dram->paging[i].paddr,
            (unsigned long long)dram->paging[i].size));
    }

    return 0;
}

void ItlIwx::
iwx_record_boot_dma(struct iwx_softc *sc, const struct iwx_fw_sects *fws)
{
    if (sc->sc_device_family != IWX_DEVICE_FAMILY_BZ)
        return;
    iwx_publish_boot_dma(sc->sc_pcitag, "Context", &sc->ctxt_info_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "Scratch", &sc->prph_scratch_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "PeripheralInfo", &sc->prph_info_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "Iml", &sc->iml_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "RxFree", &sc->rxq.free_desc_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "RxUsed", &sc->rxq.used_desc_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "RxStatus", &sc->rxq.stat_dma);
    iwx_publish_boot_dma(sc->sc_pcitag, "CommandRing", &sc->txq[IWX_DQA_CMD_QUEUE].desc_dma);
    sc->sc_pcitag->setProperty("ItlwmBootImlBytes", sc->sc_iml_len, 32);
    sc->sc_pcitag->setProperty("ItlwmBootRxEntries", sc->rxq.count, 32);
    struct iwx_self_init_dram *dram = &sc->init_dram;
    for (int i = 0; i < dram->lmac_cnt + dram->umac_cnt; i++) {
        char name[32];
        const bool lmac = i < dram->lmac_cnt;
        const int section = lmac ? i : i + 1;
        snprintf(name, sizeof(name), "Fw%s%02d", lmac ? "Lmac" : "Umac",
                 lmac ? i : i - dram->lmac_cnt);
        iwx_publish_boot_dma(sc->sc_pcitag, name, &dram->fw[i],
                             fws->fw_sect[section].fws_devoff);
    }
    for (int i = 0; i < dram->paging_cnt; i++) {
        char name[32];
        snprintf(name, sizeof(name), "FwPaging%02d", i);
        iwx_publish_boot_dma(sc->sc_pcitag, name, &dram->paging[i],
            fws->fw_sect[dram->lmac_cnt + dram->umac_cnt + i + 2].fws_devoff);
    }
}

int ItlIwx::
iwx_bz_port_ctxt_info_gen3_init(struct iwx_softc *sc, const struct iwx_fw_sects *fws)
{
    if (!fBzAdmitted || fBzFault) return ENXIO;
    if (getMainWorkLoop()->runAction(rxZcArmAction,this)!=kIOReturnSuccess) return ENXIO;
    XYLog("%s\n", __FUNCTION__);
    struct iwx_context_info_gen3 *ctxt_info_gen3;
    struct iwx_prph_scratch *prph_scratch;
    struct iwx_prph_scratch_ctrl_cfg *prph_sc_ctrl;
    uint32_t control_flags = 0;
    int err;

    /* Allocate prph scratch. */
    err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->prph_scratch_dma,
                               sizeof(struct iwx_prph_scratch), 0);
    if (err) {
        XYLog("%s: could not allocate prph scratch\n", DEVNAME(sc));
        return err;
    }

    prph_scratch = (struct iwx_prph_scratch *)sc->prph_scratch_dma.vaddr;

    prph_sc_ctrl = &prph_scratch->ctrl_cfg;
    prph_sc_ctrl->version.version = 0;
    /*
     * Device matching keeps the legacy step encoding in bits 3:2. BZ's
     * firmware ABI uses the native encoding in bits 1:0, as Linux does
     * after reading CNVI_AUX_MISC_CHIP. Do not send the matching value.
     */
    uint16_t mac_id = (uint16_t)sc->sc_hw_rev;
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
        mac_id = (mac_id & ~0xfU) | IWX_CSR_HW_REV_STEP(sc->sc_hw_rev);
        sc->sc_pcitag->setProperty("ItlwmFirmwareMacId", mac_id, 32);
    }
    prph_sc_ctrl->version.mac_id = htole16(mac_id);
    prph_sc_ctrl->version.size = htole16(sizeof(*prph_scratch) / 4);

    control_flags = IWX_PRPH_SCRATCH_RB_SIZE_4K;
    control_flags |= IWX_PRPH_SCRATCH_MTR_MODE;
    control_flags |= IWX_PRPH_MTR_FORMAT_256B & IWX_PRPH_SCRATCH_MTR_FORMAT;

    /* initialize RX default queue */
    prph_sc_ctrl->rbd_cfg.free_rbd_addr =
        htole64(sc->rxq.free_desc_dma.paddr);

    prph_sc_ctrl->control.control_flags = htole32(control_flags);

    /* allocate ucode sections in dram and set addresses */
    err = iwx_init_fw_sec(sc, fws, &prph_scratch->dram);
    if (err) {
        iwx_ctxt_info_free_fw_img(sc);
        return err;
    }

    /* Allocate prph information. */
    err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->prph_info_dma,
                               PAGE_SIZE, 0);
    if (err) {
        XYLog("%s: could not allocate prph information\n", DEVNAME(sc));
        goto fail0;
    }

    ctxt_info_gen3 = (struct iwx_context_info_gen3 *)sc->ctxt_info_dma.vaddr;

    ctxt_info_gen3->prph_info_base_addr =
        htole64(sc->prph_info_dma.paddr);
    ctxt_info_gen3->prph_scratch_base_addr =
        htole64(sc->prph_scratch_dma.paddr);
    ctxt_info_gen3->prph_scratch_size =
        htole32(sizeof(*prph_scratch));
    ctxt_info_gen3->cr_head_idx_arr_base_addr =
        htole64(sc->rxq.stat_dma.paddr);
    ctxt_info_gen3->tr_tail_idx_arr_base_addr =
        htole64((uint8_t *)sc->prph_info_dma.paddr + PAGE_SIZE / 2);
    ctxt_info_gen3->cr_tail_idx_arr_base_addr =
        htole64((uint8_t *)sc->prph_info_dma.paddr + 3 * PAGE_SIZE / 4);
    ctxt_info_gen3->mtr_base_addr =
        htole64(sc->txq[IWX_DQA_CMD_QUEUE].desc_dma.paddr);
    ctxt_info_gen3->mcr_base_addr =
        htole64(sc->rxq.used_desc_dma.paddr);
    ctxt_info_gen3->mtr_size =
        htole16(IWX_TFD_QUEUE_CB_SIZE(IWX_CMD_QUEUE_SIZE_GEN3));
    ctxt_info_gen3->mcr_size =
        htole16(IWX_RX_QUEUE_CB_SIZE(sc->rxq.count));

    /* Allocate IML. */
    err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->iml_dma,
                               IWX_IML_MAX_LEN, 0);
    if (err) {
        XYLog("%s: could not allocate IML\n", DEVNAME(sc));
        goto fail1;
    }

    memcpy(sc->iml_dma.vaddr, sc->sc_iml, sc->sc_iml_len);
    iwx_record_boot_dma(sc, fws);
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ)
        iwx_bz_record_pci_irq_mode(sc, true);

    iwx_enable_fwload_interrupt(sc);

    // Synchronize every CPU-written boot extent before the first ROM doorbell.
    // The buffers are bidirectional allocations, but this operation publishes
    // CPU-initialized bytes BEFORE device ownership. synchronize(Out|In) is
    // rejected by IODMACommand; receive-side In belongs after completion.
    err = bzSyncBootMemory(sc);
    if (err) return err; // No context address or ROM doorbell on failure.
    iwx_dma_queue_publish_barrier();
    fDma5Published=true;fBzHardwareOwned=true;
    /* kick FW self load */
    IWX_WRITE64(sc, IWX_CSR_CTXT_INFO_ADDR,
            sc->ctxt_info_dma.paddr);
    IWX_WRITE64(sc, IWX_CSR_IML_DATA_ADDR,
            sc->iml_dma.paddr);
    IWX_WRITE(sc, IWX_CSR_IML_SIZE_ADDR, sc->sc_iml_len);

    IWX_SETBITS(sc, IWX_CSR_CTXT_INFO_BOOT_CTRL,
            IWX_CSR_AUTO_FUNC_BOOT_ENA);

    iwx_set_ltr(sc);

    if (sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ) {
        /* BZ ROM loads the IML after this CSR handshake. */
        IWX_WRITE(sc, IWX_CSR_FUNC_SCRATCH,
                  IWX_CSR_FUNC_SCRATCH_INIT_VALUE);
        if (sc->sc_boot_diag.active)
            sc->sc_boot_diag.rom_start_time_ns = iwx_boot_time_ns();
        IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
                    IWX_CSR_GP_CNTRL_REG_FLAG_ROM_START);
    } else {
        if (!iwx_nic_lock(sc))
            return EBUSY;
        iwx_write_umac_prph(sc, IWX_UREG_CPU_INIT_RUN, 1);
        iwx_nic_unlock(sc);
    }

    return 0;

fail1:
    iwx_dma_contig_free(&sc->iml_dma);
fail0:
    iwx_dma_contig_free(&sc->prph_info_dma);
    iwx_dma_contig_free(&sc->prph_scratch_dma);
    return ENOMEM;
}

int ItlIwx::
iwx_bz_port_read_firmware(struct iwx_softc *sc)
{
    struct iwx_fw_info *fw = &sc->sc_fw;
    struct iwx_tlv_ucode_header *uhdr;
    struct iwx_ucode_tlv tlv;
    uint32_t tlv_type = 0;
    uint8_t *data;
    int err = 0;
    size_t len;
    OSData *fwData = NULL;

    if (fw->fw_status == IWX_FW_STATUS_DONE)
        return 0;

    if (fw->fw_status == IWX_FW_STATUS_INPROGRESS)
        return EBUSY;
    fw->fw_status = IWX_FW_STATUS_INPROGRESS;

    if (fw->fw_rawdata != NULL)
        iwx_fw_info_free(fw);

//    IOLockLock(_fwLoadLock);
//    ResourceCallbackContext context =
//    {
//        .context = this,
//        .resource = NULL
//    };
//
//    //here leaks
//    IOReturn ret = OSKextRequestResource(OSKextGetCurrentIdentifier(), sc->sc_fwname, onLoadFW, &context, NULL);
//    IOLockSleep(_fwLoadLock, this, 0);
//    IOLockUnlock(_fwLoadLock);
//    if (context.resource == NULL) {
//        XYLog("%s resource load fail.\n", sc->sc_fwname);
//        goto out;
//    }
//    fw->fw_rawdata = malloc(context.resource->getLength(), 1, 1);
//    memcpy(fw->fw_rawdata, context.resource->getBytesNoCopy(), context.resource->getLength());
//    fw->fw_rawsize = context.resource->getLength();
    fwData = getFWDescByName(sc->sc_fwname);
    if (fwData == NULL) {
        err = EINVAL;
        XYLog("%s resource load fail.\n", sc->sc_fwname);
        goto out;
    }
    fw->fw_rawsize = fwData->getLength() * 4;
    fw->fw_rawdata = malloc(fw->fw_rawsize, 1, 1);
    if (fw->fw_rawdata == NULL) {
        err = ENOMEM;
        goto out;
    }
    if (!uncompressFirmware((u_char *)fw->fw_rawdata, (uint *)&fw->fw_rawsize,
                            (u_char *)fwData->getBytesNoCopy(), fwData->getLength()) ||
        fw->fw_rawsize < sizeof(*uhdr)) {
        err = EINVAL;
        goto out;
    }
    if(!kgp_bz::validTlvs(static_cast<const uint8_t *>(fw->fw_rawdata)+sizeof(*uhdr),
        fw->fw_rawsize-sizeof(*uhdr))) {err=EINVAL;goto out;}
    XYLog("load firmware %s done\n", sc->sc_fwname);

    sc->sc_capaflags = 0;
    sc->sc_capa_n_scan_channels = IWX_DEFAULT_SCAN_CHANNELS;
    memset(sc->sc_enabled_capa, 0, sizeof(sc->sc_enabled_capa));
    memset(sc->sc_ucode_api, 0, sizeof(sc->sc_ucode_api));
    sc->n_cmd_versions = 0;
    memcpy(sc->sc_fw_mcc, "ZZ", sizeof(sc->sc_fw_mcc));
    sc->sc_fw_mcc_int = 0x3030;

    uhdr = (struct iwx_tlv_ucode_header *)fw->fw_rawdata;
    if (*(uint32_t *)fw->fw_rawdata != 0
        || le32toh(uhdr->magic) != IWX_TLV_UCODE_MAGIC) {
        XYLog("%s: invalid firmware %s\n",
              DEVNAME(sc), sc->sc_fwname);
        err = EINVAL;
        goto out;
    }

    iwx_fw_version_str(sc->sc_fwver, sizeof(sc->sc_fwver),
             IWX_UCODE_MAJOR(le32toh(uhdr->ver)),
             IWX_UCODE_MINOR(le32toh(uhdr->ver)),
             IWX_UCODE_API(le32toh(uhdr->ver)));
    data = uhdr->data;
    len = fw->fw_rawsize - sizeof(*uhdr);

    XYLog("parsing firmware %s\n", sc->sc_fwname);
    while (len >= sizeof(tlv)) {
        size_t tlv_len;
        void *tlv_data;

        memcpy(&tlv, data, sizeof(tlv));
        tlv_len = le32toh(tlv.length);
        tlv_type = le32toh(tlv.type);

        len -= sizeof(tlv);
        data += sizeof(tlv);
        tlv_data = data;

        if (len < tlv_len) {
            XYLog("%s: firmware too short: %zu bytes\n",
                  DEVNAME(sc), len);
            err = EINVAL;
            goto parse_out;
        }

        switch (tlv_type) {
            case IWX_UCODE_TLV_PROBE_MAX_LEN:
                if (tlv_len < sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                sc->sc_capa_max_probe_len
                = le32toh(*(uint32_t *)tlv_data);
                if (sc->sc_capa_max_probe_len >
                    IWX_SCAN_OFFLOAD_PROBE_REQ_SIZE) {
                    err = EINVAL;
                    goto parse_out;
                }
                break;
            case IWX_UCODE_TLV_PAN:
                if (tlv_len) {
                    err = EINVAL;
                    goto parse_out;
                }
                sc->sc_capaflags |= IWX_UCODE_TLV_FLAGS_PAN;
                break;
            case IWX_UCODE_TLV_FLAGS:
                if (tlv_len < sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                /*
                 * Apparently there can be many flags, but Linux driver
                 * parses only the first one, and so do we.
                 *
                 * XXX: why does this override IWX_UCODE_TLV_PAN?
                 * Intentional or a bug?  Observations from
                 * current firmware file:
                 *  1) TLV_PAN is parsed first
                 *  2) TLV_FLAGS contains TLV_FLAGS_PAN
                 * ==> this resets TLV_PAN to itself... hnnnk
                 */
                sc->sc_capaflags = le32toh(*(uint32_t *)tlv_data);
                break;
            case IWX_UCODE_TLV_CSCHEME:
                err = iwx_store_cscheme(sc, (uint8_t *)tlv_data, tlv_len);
                if (err)
                    goto parse_out;
                break;
            case IWX_UCODE_TLV_NUM_OF_CPU: {
                uint32_t num_cpu;
                if (tlv_len != sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                num_cpu = le32toh(*(uint32_t *)tlv_data);
                if (num_cpu < 1 || num_cpu > 2) {
                    err = EINVAL;
                    goto parse_out;
                }
                break;
            }
            case IWX_UCODE_TLV_SEC_RT:
                err = iwx_firmware_store_section(sc,
                                                 IWX_UCODE_TYPE_REGULAR, (uint8_t *)tlv_data, tlv_len);
                if (err)
                    goto parse_out;
                break;
            case IWX_UCODE_TLV_SEC_INIT:
                err = iwx_firmware_store_section(sc,
                                                 IWX_UCODE_TYPE_INIT, (uint8_t *)tlv_data, tlv_len);
                if (err)
                    goto parse_out;
                break;
            case IWX_UCODE_TLV_SEC_WOWLAN:
                err = iwx_firmware_store_section(sc,
                                                 IWX_UCODE_TYPE_WOW, (uint8_t *)tlv_data, tlv_len);
                if (err)
                    goto parse_out;
                break;
            case IWX_UCODE_TLV_DEF_CALIB:
                if (tlv_len != sizeof(struct iwx_tlv_calib_data)) {
                    err = EINVAL;
                    goto parse_out;
                }
                err = iwx_set_default_calib(sc, tlv_data);
                if (err)
                    goto parse_out;
                break;
            case IWX_UCODE_TLV_PHY_SKU:
                if (tlv_len != sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                sc->sc_fw_phy_config = le32toh(*(uint32_t *)tlv_data);
                break;

            case IWX_UCODE_TLV_API_CHANGES_SET: {
                struct iwx_ucode_api *api;
                int idx, i;
                if (tlv_len != sizeof(*api)) {
                    err = EINVAL;
                    goto parse_out;
                }
                api = (struct iwx_ucode_api *)tlv_data;
                idx = le32toh(api->api_index);
                if (idx >= howmany(IWX_NUM_UCODE_TLV_API, 32)) {
                    err = EINVAL;
                    goto parse_out;
                }
                for (i = 0; i < 32; i++) {
                    if ((le32toh(api->api_flags) & (1 << i)) == 0)
                        continue;
                    setbit(sc->sc_ucode_api, i + (32 * idx));
                }
                break;
            }

            case IWX_UCODE_TLV_ENABLED_CAPABILITIES: {
                struct iwx_ucode_capa *capa;
                int idx, i;
                if (tlv_len != sizeof(*capa)) {
                    err = EINVAL;
                    goto parse_out;
                }
                capa = (struct iwx_ucode_capa *)tlv_data;
                idx = le32toh(capa->api_index);
                if (idx >= howmany(IWX_NUM_UCODE_TLV_CAPA, 32)) {
                    err=EINVAL;goto parse_out;
                }
                for (i = 0; i < 32; i++) {
                    if ((le32toh(capa->api_capa) & (1 << i)) == 0)
                        continue;
                    setbit(sc->sc_enabled_capa, i + (32 * idx));
                }
                break;
            }

            case IWX_UCODE_TLV_SDIO_ADMA_ADDR:
            case IWX_UCODE_TLV_FW_GSCAN_CAPA:
                /* ignore, not used by current driver */
                break;

            case IWX_UCODE_TLV_SEC_RT_USNIFFER:
                err = iwx_firmware_store_section(sc,
                                                 IWX_UCODE_TYPE_REGULAR_USNIFFER, (uint8_t *)tlv_data,
                                                 tlv_len);
                if (err)
                    goto parse_out;
                break;

            case IWX_UCODE_TLV_PAGING:
                if (tlv_len != sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                break;

            case IWX_UCODE_TLV_N_SCAN_CHANNELS:
                if (tlv_len != sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                sc->sc_capa_n_scan_channels =
                le32toh(*(uint32_t *)tlv_data);
                if (sc->sc_capa_n_scan_channels > IWX_MAX_SCAN_CHANNELS) {
                    err = ERANGE;
                    goto parse_out;
                }
                break;

            case IWX_UCODE_TLV_FW_VERSION:
                if (tlv_len != sizeof(uint32_t) * 3) {
                    err = EINVAL;
                    goto parse_out;
                }
                iwx_fw_version_str(sc->sc_fwver, sizeof(sc->sc_fwver),
                         le32toh(((uint32_t *)tlv_data)[0]),
                         le32toh(((uint32_t *)tlv_data)[1]),
                         le32toh(((uint32_t *)tlv_data)[2]));
                break;

            case IWX_UCODE_TLV_FW_DBG_DEST: {
                struct iwx_fw_dbg_dest_tlv_v1 *dest_v1 = NULL;

                fw->dbg_dest_ver = (uint8_t *)tlv_data;
                if (*fw->dbg_dest_ver != 0) {
                    err = EINVAL;
                    goto parse_out;
                }

                if (fw->dbg_dest_tlv_init)
                    break;
                fw->dbg_dest_tlv_init = true;

                dest_v1 = (struct iwx_fw_dbg_dest_tlv_v1 *)tlv_data;
                fw->dbg_dest_tlv_v1 = dest_v1;
                fw->n_dest_reg = tlv_len -
                offsetof(struct iwx_fw_dbg_dest_tlv_v1, reg_ops);
                fw->n_dest_reg /= sizeof(dest_v1->reg_ops[0]);
                DPRINTF(("%s: found debug dest; n_dest_reg=%d\n", __func__, fw->n_dest_reg));
                break;
            }

            case IWX_UCODE_TLV_FW_DBG_CONF: {
                struct iwx_fw_dbg_conf_tlv *conf = (struct iwx_fw_dbg_conf_tlv *)tlv_data;

                if (!fw->dbg_dest_tlv_init ||
                    conf->id >= nitems(fw->dbg_conf_tlv) ||
                    fw->dbg_conf_tlv[conf->id] != NULL)
                    break;

                DPRINTF(("Found debug configuration: %d\n", conf->id));
                fw->dbg_conf_tlv[conf->id] = conf;
                fw->dbg_conf_tlv_len[conf->id] = tlv_len;
                break;
            }

            case IWX_UCODE_TLV_UMAC_DEBUG_ADDRS: {
                struct iwx_umac_debug_addrs *dbg_ptrs =
                (struct iwx_umac_debug_addrs *)tlv_data;

                if (tlv_len != sizeof(*dbg_ptrs)) {
                    err = EINVAL;
                    goto parse_out;
                }
                if (sc->sc_device_family < IWX_DEVICE_FAMILY_22000)
                    break;
                sc->sc_uc.uc_umac_error_event_table =
                le32toh(dbg_ptrs->error_info_addr) &
                ~IWX_FW_ADDR_CACHE_CONTROL;
                sc->sc_uc.error_event_table_tlv_status |=
                IWX_ERROR_EVENT_TABLE_UMAC;
                break;
            }

            case IWX_UCODE_TLV_LMAC_DEBUG_ADDRS: {
                struct iwx_lmac_debug_addrs *dbg_ptrs =
                (struct iwx_lmac_debug_addrs *)tlv_data;

                if (tlv_len != sizeof(*dbg_ptrs)) {
                    err = EINVAL;
                    goto parse_out;
                }
                if (sc->sc_device_family < IWX_DEVICE_FAMILY_22000)
                    break;
                sc->sc_uc.uc_lmac_error_event_table[0] =
                le32toh(dbg_ptrs->error_event_table_ptr) &
                ~IWX_FW_ADDR_CACHE_CONTROL;
                sc->sc_uc.error_event_table_tlv_status |=
                IWX_ERROR_EVENT_TABLE_LMAC1;
                break;
            }

            case IWX_UCODE_TLV_FW_MEM_SEG:
                break;

            case IWX_UCODE_TLV_IML:
                if (sizeof(sc->sc_iml) < tlv_len) {
                    XYLog("IML max len mismatch. len1: %zu len2: %zu\n", sizeof(sc->sc_iml), tlv_len);
                    err = EINVAL;
                    goto parse_out;
                }
                sc->sc_iml_len = (uint32_t)tlv_len;
                memcpy(sc->sc_iml, tlv_data, sc->sc_iml_len);
                break;

            case IWX_UCODE_TLV_CMD_VERSIONS:
                if (tlv_len % sizeof(struct iwx_fw_cmd_version)) {
                    err=EINVAL;goto parse_out;
                }
                if (sc->n_cmd_versions != 0) {
                    err = EINVAL;
                    goto parse_out;
                }
                if (tlv_len > sizeof(sc->cmd_versions)) {
                    err = EINVAL;
                    goto parse_out;
                }
                memcpy(&sc->cmd_versions[0], tlv_data, tlv_len);
                sc->n_cmd_versions = tlv_len / sizeof(struct iwx_fw_cmd_version);
                break;

            case IWX_UCODE_TLV_FW_RECOVERY_INFO:
                break;

            case IWX_UCODE_TLV_FW_FSEQ_VERSION: {
                struct sfseq_ver {
                    uint8_t version[32];
                    uint8_t sha1[20];
                };
                sfseq_ver *seq_ver = (sfseq_ver *)tlv_data;
                if (tlv_len != sizeof(struct sfseq_ver)) {
                    err = EINVAL;
                    goto parse_out;
                }
                XYLog("TLV_FW_FSEQ_VERSION: %s\n", seq_ver->version);
            }
                break;

            case IWX_UCODE_PHY_INTEGRATION_VERSION:
                break;
            case IWX_UCODE_TLV_FW_NUM_STATIONS:
                if (tlv_len != sizeof(uint32_t)) {
                    err = EINVAL;
                    goto parse_out;
                }
                sc->sc_capa_num_stations = le32toh(*(uint32_t *)tlv_data);
                if (sc->sc_capa_num_stations > IWX_STATION_COUNT_MAX) {
                    XYLog("%d is an invalid number of station\n",
                          sc->sc_capa_num_stations);
                    err = EINVAL;
                    goto parse_out;
                }
                break;

                /* undocumented TLVs found in iwx-cc-a0-48 image */
            case 58:
            case 0x1000003:
            case 0x1000004:
                break;

                /* undocumented TLVs found in iwx-cc-a0-48 image */
            case 0x1000000:
            case 0x1000002:
            case IWX_UCODE_TLV_TYPE_DEBUG_INFO:
            case IWX_UCODE_TLV_TYPE_BUFFER_ALLOCATION:
            case IWX_UCODE_TLV_TYPE_HCMD:
            case IWX_UCODE_TLV_TYPE_REGIONS:
            case IWX_UCODE_TLV_TYPE_TRIGGERS:
            case IWX_UCODE_TLV_TYPE_CONF_SET:
                break;

                /* undocumented TLV found in iwx-cc-a0-67 image */
            case 0x100000b:
                break;

            default:
                //            err = EINVAL;
                //            goto parse_out;
                XYLog("%s unhandle section type= %d\n", __FUNCTION__, tlv_type);
                break;
        }

        /*
         * Check for size_t overflow and ignore missing padding at
         * end of firmware file.
         */
        if (roundup(tlv_len, 4) > len)
            break;

        len -= roundup(tlv_len, 4);
        data += roundup(tlv_len, 4);
    }

    KASSERT(err == 0, "err == 0");

    XYLog("firmware parse done\n");

    /*
     * MLD-API firmware uses MAC_CONFIG/LINK_CONFIG/STA_CONFIG. The
     * authentication/run/key/BA code is present in this private port.
     * This is source implementation metadata, not physical qualification.
     * Publish on the firmware-load thread, never in an interrupt handler.
     */
    sc->sc_pcitag->setProperty("ItlwmFirmwareMldApi",
        !!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_MLD_API_SUPPORT));
    sc->sc_pcitag->setProperty("ItlwmFirmwareMldAssociationImplemented", true); // Source implementation only.
    sc->sc_pcitag->setProperty("KGP_BE200_MLD_HARDWARE_QUALIFIED", false);

parse_out:
    if (err) {
        XYLog("%s: firmware parse error %d, "
              "section type %d\n", DEVNAME(sc), err, tlv_type);
    }

out:
    if (err) {
        fw->fw_status = IWX_FW_STATUS_NONE;
        if (fw->fw_rawdata != NULL)
            iwx_fw_info_free(fw);
    } else
        fw->fw_status = IWX_FW_STATUS_DONE;
    wakeupOn(&sc->sc_fw);

    OSSafeReleaseNULL(fwData);

    return err;
}

int ItlIwx::
iwx_bz_port_pnvm_handle_section(struct iwx_softc *sc, const uint8_t *data, size_t len)
{
    struct iwx_ucode_tlv *tlv;
    uint32_t sha1 = 0;
    uint16_t mac_type = 0, rf_id = 0;
    uint8_t *pnvm_data = NULL, *tmp;
    bool hw_match = false;
    const bool fragmented = sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ &&
        isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_FRAGMENTED_PNVM_IMG);
    const uint8_t *pnvm_chunks[IWX_MAX_DRAM_ENTRY];
    uint32_t pnvm_chunk_lengths[IWX_MAX_DRAM_ENTRY];
    unsigned int pnvm_chunk_count = 0;
    uint32_t size = 0;
    int err = 0;
    struct iwx_prph_scratch_ctrl_cfg *prph_sc_ctrl;

    sc->sc_pcitag->setProperty("ItlwmPnvmFragmented",
        static_cast<UInt32>(fragmented ? 1 : 0), 32);

    XYLog("Handling PNVM section\n");

    while (len >= sizeof(*tlv)) {
        uint32_t tlv_len, tlv_type;

        len -= sizeof(*tlv);
        tlv = (struct iwx_ucode_tlv *)data;

        tlv_len = le32toh(tlv->length);
        tlv_type = le32toh(tlv->type);

        if (len < tlv_len) {
            XYLog("invalid TLV len: %zd/%u\n",
                  len, tlv_len);
            err = -EINVAL;
            goto out;
        }

        data += sizeof(*tlv);

        switch (tlv_type) {
            case IWX_UCODE_TLV_PNVM_VERSION:
                if (tlv_len < sizeof(__le32)) {
                    XYLog("Invalid size for IWL_UCODE_TLV_PNVM_VERSION (expected %zd, got %d)\n",
                          sizeof(__le32), tlv_len);
                    break;
                }

                sha1 = le32_to_cpup((__le32 *)data);
                sc->sc_pcitag->setProperty("ItlwmPnvmVersion", sha1, 32);

                XYLog("Got IWL_UCODE_TLV_PNVM_VERSION %0x\n",
                      sha1);
                break;
            case IWX_UCODE_TLV_HW_TYPE:
                if (tlv_len < 2 * sizeof(__le16)) {
                    XYLog("Invalid size for IWL_UCODE_TLV_HW_TYPE (expected %zd, got %d)\n",
                          2 * sizeof(__le16), tlv_len);
                    break;
                }

                if (hw_match)
                    break;

                mac_type = le16_to_cpup((__le16 *)data);
                rf_id = le16_to_cpup((__le16 *)(data + sizeof(__le16)));

                XYLog("Got IWL_UCODE_TLV_HW_TYPE mac_type 0x%0x rf_id 0x%0x\n",
                      mac_type, rf_id);

                if (mac_type == CSR_HW_REV_TYPE(sc->sc_hw_rev) &&
                    rf_id == CSR_HW_RFID_TYPE(sc->sc_hw_rf_id))
                    hw_match = true;
                sc->sc_pcitag->setProperty("ItlwmPnvmHwMacType",
                    static_cast<UInt32>(mac_type), 32);
                sc->sc_pcitag->setProperty("ItlwmPnvmHwRfId",
                    static_cast<UInt32>(rf_id), 32);
                sc->sc_pcitag->setProperty("ItlwmPnvmHwMatch",
                    static_cast<UInt32>(hw_match ? 1 : 0), 32);
                break;
            case IWX_UCODE_TLV_SEC_RT: {
                struct iwx_pnvm_section *section = (struct iwx_pnvm_section *)data;
                if (tlv_len < sizeof(*section)) {
                    err = -EINVAL;
                    goto out;
                }
                u32 data_len = tlv_len - sizeof(*section);

                XYLog("Got IWL_UCODE_TLV_SEC_RT len %d\n",
                      tlv_len);

                /* TODO: remove, this is a deprecated separator */
                if (le32_to_cpup((__le32 *)data) == 0xddddeeee) {
                    XYLog("Ignoring separator.\n");
                    break;
                }

                XYLog("Adding data (size %d)\n",
                      data_len);
                if (data_len > UINT32_MAX - size) {
                    err = -EINVAL;
                    goto out;
                }
                if (fragmented) {
                    if (pnvm_chunk_count == IWX_MAX_DRAM_ENTRY) {
                        err = -EINVAL;
                        goto out;
                    }
                    pnvm_chunks[pnvm_chunk_count] = section->data;
                    pnvm_chunk_lengths[pnvm_chunk_count++] = data_len;
                    size += data_len;
                    break;
                }

                tmp = (uint8_t *)malloc(size + data_len, 0, 0);
                if (!tmp) {
                    XYLog("Couldn't allocate (more) pnvm_data\n");

                    err = -ENOMEM;
                    goto out;
                }
                if (pnvm_data && size > 0) {
                    memcpy(tmp, pnvm_data, size);
                    ::free(pnvm_data);
                }

                pnvm_data = tmp;

                memcpy(pnvm_data + size, section->data, data_len);

                size += data_len;

                break;
            }
            case IWX_UCODE_TLV_PNVM_SKU:
                XYLog("New PNVM section started, stop parsing.\n");
                goto done;
            default:
                XYLog("Found TLV 0x%0x, len %d\n",
                      tlv_type, tlv_len);
                break;
        }

        if (roundup(tlv_len, 4) > len)
            break;

        len -= roundup(tlv_len, 4);
        data += roundup(tlv_len, 4);
    }

done:
    /* Chunk bookkeeping: BZ -92 splits PNVM into several payloads. */
    sc->sc_pcitag->setProperty("ItlwmPnvmChunkCount",
        static_cast<UInt32>(pnvm_chunk_count), 32);
    sc->sc_pcitag->setProperty("ItlwmPnvmTotalSize", size, 32);
    if (pnvm_chunk_count > 0)
        sc->sc_pcitag->setProperty("ItlwmPnvmChunk0Len", pnvm_chunk_lengths[0], 32);
    if (pnvm_chunk_count > 1)
        sc->sc_pcitag->setProperty("ItlwmPnvmChunk1Len", pnvm_chunk_lengths[1], 32);
    if (pnvm_chunk_count > 2)
        sc->sc_pcitag->setProperty("ItlwmPnvmChunk2Len", pnvm_chunk_lengths[2], 32);
    if (pnvm_chunk_count > 3)
        sc->sc_pcitag->setProperty("ItlwmPnvmChunk3Len", pnvm_chunk_lengths[3], 32);
    if (!hw_match) {
        XYLog("HW mismatch, skipping PNVM section (need mac_type 0x%x rf_id 0x%x)\n",
                 CSR_HW_REV_TYPE(sc->sc_hw_rev),
                 CSR_HW_RFID_TYPE(sc->sc_hw_rf_id));
        err = -ENOENT;
        goto out;
    }

    if (!size) {
        XYLog("Empty PNVM, skipping.\n");
        err = -ENOENT;
        goto out;
    }

    XYLog("loaded PNVM version %08x\n", sha1);

    prph_sc_ctrl = &((struct iwx_prph_scratch *)sc->prph_scratch_dma.vaddr)->ctrl_cfg;

    if (fragmented) {
        if(sc->pnvm_desc_dma.cmd) return EBUSY;
        uint64_t *descriptors;
        unsigned int i;

        /* BZ -92 advertises fragmented PNVM: the base points to an array
         * of DMA addresses, not to the concatenated PNVM bytes. */
        iwx_dma_contig_free(&sc->pnvm_dram);
        iwx_dma_contig_free(&sc->pnvm_desc_dma);
        for (i = 0; i < IWX_MAX_DRAM_ENTRY; i++)
            iwx_dma_contig_free(&sc->pnvm_frag_dma[i]);

        err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->pnvm_desc_dma,
                                   IWX_MAX_DRAM_ENTRY * sizeof(uint64_t), 0);
        sc->sc_pcitag->setProperty("ItlwmPnvmDescDmaError",
            static_cast<UInt32>(err < 0 ? -err : err), 32);
        if (err)
            goto out;
        descriptors = (uint64_t *)sc->pnvm_desc_dma.vaddr;
        memset(descriptors, 0, IWX_MAX_DRAM_ENTRY * sizeof(*descriptors));

        for (i = 0; i < pnvm_chunk_count; i++) {
            err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->pnvm_frag_dma[i],
                                       pnvm_chunk_lengths[i], 0);
            if (err) {
                sc->sc_pcitag->setProperty("ItlwmPnvmFragDmaError",
                    static_cast<UInt32>(err < 0 ? -err : err), 32);
                sc->sc_pcitag->setProperty("ItlwmPnvmFragDmaIndex", i, 32);
                break;
            }
            memcpy(sc->pnvm_frag_dma[i].vaddr, pnvm_chunks[i],
                   pnvm_chunk_lengths[i]);
            descriptors[i] = htole64(sc->pnvm_frag_dma[i].paddr);
        }
        if (err) {
            for (i = 0; i < IWX_MAX_DRAM_ENTRY; i++)
                iwx_dma_contig_free(&sc->pnvm_frag_dma[i]);
            iwx_dma_contig_free(&sc->pnvm_desc_dma);
            goto out;
        }
        prph_sc_ctrl->pnvm_cfg.pnvm_base_addr =
            htole64(sc->pnvm_desc_dma.paddr);
        prph_sc_ctrl->pnvm_cfg.pnvm_size = htole32(size);
        goto out;
    }

    iwx_dma_contig_free(&sc->pnvm_dram);
    err = iwx_dma_contig_alloc(sc->sc_dmat, &sc->pnvm_dram, size, 0);
    if (err) {
        sc->sc_pcitag->setProperty("ItlwmPnvmContigDmaError",
            static_cast<UInt32>(err < 0 ? -err : err), 32);
        XYLog("%s: could not allocate context info DMA memory\n",
              DEVNAME(sc));
        goto out;
    }

    memcpy(sc->pnvm_dram.vaddr, pnvm_data, size);

    prph_sc_ctrl->pnvm_cfg.pnvm_base_addr =
        htole64(sc->pnvm_dram.paddr);
    prph_sc_ctrl->pnvm_cfg.pnvm_size =
        htole32(sc->pnvm_dram.size);

out:
    ::free(pnvm_data);

    return err;
}

int ItlIwx::
iwx_bz_port_read_pnvm(struct iwx_softc *sc)
{
    int err = 0;
    OSData *fwData = NULL;
    struct iwx_fw_info *fw = &sc->sc_fw;
    const char *pnvm_name = NULL;
    struct iwx_ucode_tlv *tlv;
    uint8_t *data;
    size_t len;
    /* Probe timers; declared up front so the early goto out does not cross
     * an initialization (C++ forbids jumps into a variable's scope). */
    uint64_t probe_start = iwx_boot_time_ns();
    uint64_t lookup_start = 0;
    uint64_t inflate_start = 0;
    uint64_t parse_start = 0;
    uint32_t inflate_cap = 0;
    bool inflated = false;

    /* Probe step 0: name derivation. */
    sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(0), 32);
    if (fw->pnvm_rawdata != NULL)
        iwx_pnvm_free(fw);
    pnvm_name = kgp_bz::pnvmForFirmware(sc->sc_fwname);
    if (!pnvm_name) {
        err = EINVAL;
        sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(1), 32);
        goto out;
    }
    sc->sc_pcitag->setProperty("ItlwmPnvmName", pnvm_name);
    sc->sc_pcitag->setProperty("ItlwmPnvmNameTimeNs",
        iwx_boot_time_ns() - probe_start, 64);

    /* Probe step 1: embedded blob lookup. */
    lookup_start = iwx_boot_time_ns();
    fwData = getFWDescByName(pnvm_name);
    sc->sc_pcitag->setProperty("ItlwmPnvmLookupTimeNs",
        iwx_boot_time_ns() - lookup_start, 64);

    if (fwData == NULL) {
        err = EINVAL;
        sc->sc_pcitag->setProperty("ItlwmPnvmDescFound", static_cast<UInt32>(0), 32);
        sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(2), 32);
        XYLog("%s resource load fail.\n", pnvm_name);
        goto out;
    }
    sc->sc_pcitag->setProperty("ItlwmPnvmDescFound", static_cast<UInt32>(1), 32);
    sc->sc_pcitag->setProperty("ItlwmPnvmDescLength",
        static_cast<UInt32>(fwData->getLength()), 32);

    /* Probe step 2: zlib inflate. Buffer is compressed length * 20. */
    if (fwData->getLength() > UINT32_MAX / 20U) {
        err = EINVAL;
        sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(3), 32);
        goto out;
    }
    fw->pnvm_rawsize = fwData->getLength() * 20;
    fw->pnvm_rawdata = malloc(fw->pnvm_rawsize, 1, 1);
    if (fw->pnvm_rawdata == NULL) {
        err = ENOMEM;
        sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(3), 32);
        goto out;
    }
    inflate_start = iwx_boot_time_ns();
    inflate_cap = static_cast<uint32_t>(fw->pnvm_rawsize);
    inflated = uncompressFirmware((u_char *)fw->pnvm_rawdata,
                        (uint *)&fw->pnvm_rawsize,
                        (u_char *)fwData->getBytesNoCopy(), fwData->getLength());
    sc->sc_pcitag->setProperty("ItlwmPnvmInflateTimeNs",
        iwx_boot_time_ns() - inflate_start, 64);
    sc->sc_pcitag->setProperty("ItlwmPnvmInflateOk", static_cast<UInt32>(inflated), 32);
    sc->sc_pcitag->setProperty("ItlwmPnvmInflateCap", inflate_cap, 32);
    sc->sc_pcitag->setProperty("ItlwmPnvmInflatedLength",
        static_cast<UInt32>(fw->pnvm_rawsize), 32);
    if (!inflated) {
        err = EINVAL;
        sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(3), 32);
        goto out;
    }
    XYLog("load firmware %s done %zu\n", pnvm_name, fw->pnvm_rawsize);

    XYLog("Parsing PNVM file\n");

    /* Probe step 3: TLV walk and section handling. */
    sc->sc_pcitag->setProperty("ItlwmPnvmFailStep", static_cast<UInt32>(4), 32);
    parse_start = iwx_boot_time_ns();
    data = (uint8_t *)fw->pnvm_rawdata;
    len = fw->pnvm_rawsize;
    // Validate the complete envelope before any typed SKU/section access.
    if (!kgp_bz::validTlvs(data, len)) { err = EINVAL; goto out; }
    err = ENOENT; // No matching SKU/section is never successful initialization.

    while (len >= sizeof(*tlv)) {
        uint32_t tlv_len, tlv_type;

        len -= sizeof(*tlv);
        tlv = (struct iwx_ucode_tlv *)data;

        tlv_len = le32toh(tlv->length);
        tlv_type = le32toh(tlv->type);

        if (len < tlv_len || roundup(tlv_len, 4) > len) {
            XYLog("invalid TLV len: %zd/%u\n",
                len, tlv_len);
            err = -EINVAL;
            goto out;
        }

        if (tlv_type == IWX_UCODE_TLV_PNVM_SKU) {
            if (tlv_len != sizeof(struct iwx_sku_id)) { err = EINVAL; goto out; }
            struct iwx_sku_id *sku_id =
                (struct iwx_sku_id *)(data + sizeof(*tlv));

            XYLog("Got IWL_UCODE_TLV_PNVM_SKU len %d\n",
                     tlv_len);
            XYLog("sku_id 0x%0x 0x%0x 0x%0x\n",
                     le32toh(sku_id->data[0]),
                     le32toh(sku_id->data[1]),
                     le32toh(sku_id->data[2]));

            data += sizeof(*tlv) + roundup(tlv_len, 4);
            len -= roundup(tlv_len, 4);

            if (sc->sku_id[0] == le32toh(sku_id->data[0]) &&
                sc->sku_id[1] == le32toh(sku_id->data[1]) &&
                sc->sku_id[2] == le32toh(sku_id->data[2])) {

                err = iwx_pnvm_handle_section(sc, data, len);
                if (!err)
                    goto out;
            } else {
                XYLog("SKU ID didn't match!\n");
            }
        } else {
            data += sizeof(*tlv) + roundup(tlv_len, 4);
            len -= roundup(tlv_len, 4);
        }
    }

    XYLog("PNVM parse done\n");

out:
    if (parse_start)
        sc->sc_pcitag->setProperty("ItlwmPnvmParseTimeNs",
            iwx_boot_time_ns() - parse_start, 64);
    OSSafeReleaseNULL(fwData);
    if (err && fw->pnvm_rawdata != NULL)
        iwx_pnvm_free(fw);

    return err < 0 ? -err : err;
}

int ItlIwx::
iwx_bz_port_load_pnvm(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    int err;

    /* if the SKU_ID is empty, there's nothing to do */
    if (!sc->sku_id[0] && !sc->sku_id[1] && !sc->sku_id[2]) {
        ItlStartupStage(sc->sc_pcitag, "hal:pnvm-empty-sku");
        return 0;
    }

    ItlStartupStage(sc->sc_pcitag, "hal:pnvm-read");
    // GL92 requires the selected PNVM. Do not turn a missing/invalid image
    // into successful initialization by ringing a fallback doorbell.
    uint64_t read_start = iwx_boot_time_ns();
    int readError = iwx_read_pnvm(sc);
    sc->sc_pcitag->setProperty("ItlwmPnvmReadTimeNs",
        iwx_boot_time_ns() - read_start, 64);
    sc->sc_pcitag->setProperty("ItlwmPnvmReadError", static_cast<UInt32>(readError), 32);
    if(readError) {bzFail(27,readError);return readError;}
    if(!sc->pnvm_desc_dma.cmd) {bzFail(27,EINVAL);return EINVAL;}
    for(unsigned i=0;i<IWX_MAX_DRAM_ENTRY;++i) {
        auto &r=sc->pnvm_frag_dma[i];
        if(r.cmd && r.cmd->synchronize(kIODirectionOut)!=kIOReturnSuccess) {bzFail(27,EIO);return EIO;}
    }
    if(sc->pnvm_desc_dma.cmd->synchronize(kIODirectionOut)!=kIOReturnSuccess ||
       sc->prph_scratch_dma.cmd->synchronize(kIODirectionOut)!=kIOReturnSuccess) {bzFail(27,EIO);return EIO;}
    iwx_dma_queue_publish_barrier();
    sc->sc_init_complete &= ~IWX_PNVM_COMPLETE;
    /* kick the doorbell */
    if (!iwx_nic_lock(sc)) {
        iwx_pnvm_free(&sc->sc_fw);
        ItlStartupFailure(sc->sc_pcitag, "hal:pnvm-doorbell-lock", EBUSY);
        return EBUSY;
    }
    iwx_write_umac_prph(sc, IWX_UREG_DOORBELL_TO_ISR6,
                       IWX_UREG_DOORBELL_TO_ISR6_PNVM);
    iwx_nic_unlock(sc);

    ItlStartupStage(sc->sc_pcitag, "hal:pnvm-wait-complete");
    err = iwx_wait_notification(sc, &sc->sc_init_complete, IWX_PNVM_COMPLETE, 2000);
    sc->sc_pcitag->setProperty("ItlwmFirmwareInitFlags", sc->sc_init_complete, 32);
    /* Notification status is captured in the ISR, published only here. */
    sc->sc_pcitag->setProperty("ItlwmPnvmNotifyStatus", sc->sc_pnvm_status, 32);
    if (err)
        ItlStartupFailure(sc->sc_pcitag, "hal:pnvm-wait-complete", err);
    if (err == ENXIO)
        return err;

    iwx_pnvm_free(&sc->sc_fw);

    return err;
}

int ItlIwx::
iwx_bz_port_nic_lock(struct iwx_softc *sc)
{
    if (sc->sc_nic_locks > 0) {
        iwx_nic_assert_locked(sc);
        sc->sc_nic_locks++;
        return 1; /* already locked */
    }

    const bool bz = sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ;
    const uint32_t request = bz ?
        IWX_CSR_GP_CNTRL_REG_FLAG_BZ_MAC_ACCESS_REQ :
        IWX_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ;
    const uint32_t ready = bz ?
        IWX_CSR_GP_CNTRL_REG_FLAG_MAC_STATUS :
        IWX_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN;
    const uint32_t mask = bz ?
        IWX_CSR_GP_CNTRL_REG_FLAG_MAC_STATUS :
        IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY |
        IWX_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP;

    IWX_SETBITS(sc, IWX_CSR_GP_CNTRL, request);

    DELAY(2);

    if (iwx_poll_bit(sc, IWX_CSR_GP_CNTRL,
                     ready, mask, 150000)) {
        sc->sc_nic_locks++;
        return 1;
    }

    IWX_CLRBITS(sc, IWX_CSR_GP_CNTRL, request);
    XYLog("%s: acquiring device failed\n", DEVNAME(sc));
    return 0;
}

void ItlIwx::
iwx_bz_port_nic_unlock(struct iwx_softc *sc)
{
    if (sc->sc_nic_locks > 0) {
        if (--sc->sc_nic_locks == 0)
            IWX_CLRBITS(sc, IWX_CSR_GP_CNTRL,
                sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ ?
                IWX_CSR_GP_CNTRL_REG_FLAG_BZ_MAC_ACCESS_REQ :
                IWX_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
    } else
        XYLog("%s: NIC already unlocked\n", DEVNAME(sc));
}

void ItlIwx::
iwx_bz_port_enable_fwload_interrupt(struct iwx_softc *sc)
{

    XYLog("%s\n", __FUNCTION__);
    if (!sc->sc_msix) {
        sc->sc_intmask = IWX_CSR_INT_BIT_ALIVE | IWX_CSR_INT_BIT_FH_RX;
        if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ && sc->sc_boot_diag.active)
            sc->sc_boot_diag.fwload_mask_time_ns = iwx_boot_time_ns();
        IWX_WRITE(sc, IWX_CSR_INT_MASK, sc->sc_intmask);
        if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ && sc->sc_boot_diag.active)
            sc->sc_boot_diag.fwload_mask_readback = IWX_READ(sc, IWX_CSR_INT_MASK);
    } else {
        IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
                  ~IWX_MSIX_HW_INT_CAUSES_REG_ALIVE);
        sc->sc_hw_mask = IWX_MSIX_HW_INT_CAUSES_REG_ALIVE;
        /*
         * Leave all the FH causes enabled to get the ALIVE
         * notification.
         */
        IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
                  ~sc->sc_fh_init_mask);
        sc->sc_fh_mask = sc->sc_fh_init_mask;
    }
}

void ItlIwx::
iwx_bz_restore_boot_interrupts(struct iwx_softc *sc)
{
    struct iwx_boot_diagnostics *diag = &sc->sc_boot_diag;
    const uint32_t expected = IWX_CSR_INT_BIT_ALIVE | IWX_CSR_INT_BIT_FH_RX;
    const uint32_t filters = __atomic_load_n(&sc->sc_boot_filter_count, __ATOMIC_ACQUIRE);
    diag->restore_expected = expected;
    if (filters != diag->action_filter_mark) {
        diag->restore_raced++;
        return;
    }
    sc->sc_intmask = expected;
    diag->restore_last_time_ns = iwx_boot_time_ns();
    IWX_WRITE(sc, IWX_CSR_INT_MASK, expected);
    uint32_t actual = IWX_READ(sc, IWX_CSR_INT_MASK);
    if (diag->restore_checks++ == 0) {
        diag->restore_first = actual;
        diag->restore_first_time_ns = diag->restore_last_time_ns;
    }
    diag->restore_last = actual;

    /* The hard filter masks CSR_INT_MASK when a new IRQ arrives. Do not
     * overwrite that handoff or mistake it for a failed register write. */
    if (__atomic_load_n(&sc->sc_boot_filter_count, __ATOMIC_ACQUIRE) != filters) {
        diag->restore_raced++;
        return;
    }
    if (iwx_boot_io_invalid(actual)) {
        diag->error = ENXIO;
    } else if ((actual & expected) != expected) {
        /* Linux restores this mask once and then waits for the RX ALIVE
         * message. A missing readback bit alone is not a firmware error.
         * Keep the evidence without rewriting, polling or ending the wait. */
        diag->restore_mismatches++;
    }
    if (diag->error)
        getMainCommandGate()->commandWakeup(&sc->sc_uc.uc_intr);
}

void ItlIwx::
iwx_bz_port_ict_reset(struct iwx_softc *sc)
{
    if(sc->sc_msix) return;
    iwx_disable_interrupts(sc);

    memset(sc->ict_dma.vaddr, 0, IWX_ICT_SIZE);
    sc->ict_cur = 0;

    /* Set physical address of ICT (4KB aligned). */
    IWX_WRITE(sc, IWX_CSR_DRAM_INT_TBL_REG,
              IWX_CSR_DRAM_INT_TBL_ENABLE
              | IWX_CSR_DRAM_INIT_TBL_WRAP_CHECK
              | IWX_CSR_DRAM_INIT_TBL_WRITE_POINTER
              | sc->ict_dma.paddr >> IWX_ICT_PADDR_SHIFT);

    /* Switch to ICT interrupt mode in driver. */
    sc->sc_flags |= IWX_FLAG_USE_ICT;

    IWX_WRITE(sc, IWX_CSR_INT, sc->sc_intmask);
    iwx_enable_interrupts(sc);
}

int ItlIwx::
iwx_bz_port_apm_init(struct iwx_softc *sc)
{
    int err = 0;

    /*
     * Disable L0s without affecting L1;
     *  don't wait for ICH L0s (ICH bug W/A)
     */
    IWX_SETBITS(sc, IWX_CSR_GIO_CHICKEN_BITS,
                IWX_CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX);

    /* Set FH wait threshold to maximum (HW error during stress W/A) */
    IWX_SETBITS(sc, IWX_CSR_DBG_HPET_MEM_REG, IWX_CSR_DBG_HPET_MEM_REG_VAL);

    /*
     * Enable HAP INTA (interrupt from management bus) to
     * wake device's PCI Express link L1a -> L0s
     */
    IWX_SETBITS(sc, IWX_CSR_HW_IF_CONFIG_REG,
                IWX_CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);

    iwx_apm_config(sc);

    /*
     * Set "initialization complete" bit to move adapter from
     * D0U* --> D0A* (powered-up active) state.
     */
    const bool bz = sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ;
    const uint32_t ready = bz ?
        IWX_CSR_GP_CNTRL_REG_FLAG_MAC_STATUS :
        IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY;
    if (bz)
        IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
            IWX_CSR_GP_CNTRL_REG_FLAG_BZ_MAC_ACCESS_REQ |
            IWX_CSR_GP_CNTRL_REG_FLAG_MAC_INIT);
    else
        IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
                    IWX_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

    /*
     * Wait for clock stabilization; once stabilized, access to
     * device-internal resources is supported, e.g. iwx_write_prph()
     * and accesses to uCode SRAM.
     */
    if (!iwx_poll_bit(sc, IWX_CSR_GP_CNTRL,
                      ready, ready, 25000)) {
        XYLog("%s: timeout waiting for clock stabilization\n",
              DEVNAME(sc));
        err = ETIMEDOUT;
        goto out;
    }
out:
    if (err)
        XYLog("%s: apm init error %d\n", DEVNAME(sc), err);
    return err;
}

int ItlIwx::
iwx_bz_port_init_msix_hw(struct iwx_softc *sc)
{
    int err = iwx_bz_port_conf_msix_hw(sc, 0);
    if (err)
        return err;

    if (!sc->sc_msix)
        return 0;

    sc->sc_fh_init_mask = ~IWX_READ(sc, IWX_CSR_MSIX_FH_INT_MASK_AD);
    sc->sc_fh_mask = sc->sc_fh_init_mask;
    sc->sc_hw_init_mask = ~IWX_READ(sc, IWX_CSR_MSIX_HW_INT_MASK_AD);
    sc->sc_hw_mask = sc->sc_hw_init_mask;
    return 0;
}

int ItlIwx::
iwx_bz_config_msi(struct iwx_softc *sc, uint32_t *before, uint32_t *after)
{
    *before = *after = 0xffffffffU;
    if (!iwx_nic_lock(sc))
        return EBUSY;
    *before = iwx_read_umac_prph(sc, IWX_UREG_CHICK);
    if (iwx_boot_io_invalid(*before)) {
        iwx_nic_unlock(sc);
        return ENXIO;
    }
    iwx_write_umac_prph(sc, IWX_UREG_CHICK, IWX_UREG_CHICK_MSI_ENABLE);
    *after = iwx_read_umac_prph(sc, IWX_UREG_CHICK);
    iwx_nic_unlock(sc);
    /* Upstream does not gate startup on this bit reading as latched.
     * Preserve its value as evidence; only an inaccessible device is fatal. */
    return iwx_boot_io_invalid(*after) ? ENXIO : 0;
}

int ItlIwx::
iwx_bz_port_conf_msix_hw(struct iwx_softc *sc, int stopped)
{
    int vector = 0;

    if (!sc->sc_msix) {
        if (!stopped && sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
            uint32_t before, after;
            int err = iwx_bz_config_msi(sc, &before, &after);
            /* This setup path runs outside the interrupt action and NIC lock. */
            sc->sc_pcitag->setProperty("ItlwmMsiSetupBefore", before, 32);
            sc->sc_pcitag->setProperty("ItlwmMsiSetupAfter", after, 32);
            sc->sc_pcitag->setProperty("ItlwmMsiSetupError", err, 32);
            sc->sc_pcitag->setProperty("ItlwmMsiSetupTimeNs", iwx_boot_time_ns(), 64);
            sc->sc_pcitag->setProperty("ItlwmMsiSetupReadbackMatches",
                !err && (after & (IWX_UREG_CHICK_MSI_ENABLE | IWX_UREG_CHICK_MSIX_ENABLE)) ==
                IWX_UREG_CHICK_MSI_ENABLE);
            return err;
        }
        /* Newer chips default to MSIX. */
        if (!stopped && iwx_nic_lock(sc)) {
            iwx_write_umac_prph(sc, IWX_UREG_CHICK,
                           IWX_UREG_CHICK_MSI_ENABLE);
            iwx_nic_unlock(sc);
        }
        return 0;
    }

    if (!stopped && iwx_nic_lock(sc)) {
        iwx_write_umac_prph(sc, IWX_UREG_CHICK, IWX_UREG_CHICK_MSIX_ENABLE);
        iwx_nic_unlock(sc);
    }

    /* Disable all interrupts */
    IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_MASK_AD, ~0);
    IWX_WRITE(sc, IWX_CSR_MSIX_HW_INT_MASK_AD, ~0);

    /* Map fallback-queue (command/mgmt) to a single vector */
    IWX_WRITE_1(sc, IWX_CSR_MSIX_RX_IVAR(0),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    /* Map RSS queue (data) to the same vector */
    IWX_WRITE_1(sc, IWX_CSR_MSIX_RX_IVAR(1),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);

    /* Enable the RX queues cause interrupts */
    IWX_CLRBITS(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
                IWX_MSIX_FH_INT_CAUSES_Q0 | IWX_MSIX_FH_INT_CAUSES_Q1);

    /* Map non-RX causes to the same vector */
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_D2S_CH0_NUM),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_D2S_CH1_NUM),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_S2D),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_FH_ERR),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_ALIVE),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_WAKEUP),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_IML),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_CT_KILL),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_RF_KILL),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_PERIODIC),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(
                sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ ?
                IWX_MSIX_IVAR_CAUSE_REG_SW_ERR_BZ :
                IWX_MSIX_IVAR_CAUSE_REG_SW_ERR),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_SCD),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_FH_TX),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_HW_ERR),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);
    IWX_WRITE_1(sc, IWX_CSR_MSIX_IVAR(IWX_MSIX_IVAR_CAUSE_REG_HAP),
                vector | IWX_MSIX_NON_AUTO_CLEAR_CAUSE);

    /* Enable non-RX causes interrupts */
    IWX_CLRBITS(sc, IWX_CSR_MSIX_FH_INT_MASK_AD,
                IWX_MSIX_FH_INT_CAUSES_D2S_CH0_NUM |
                IWX_MSIX_FH_INT_CAUSES_D2S_CH1_NUM |
                IWX_MSIX_FH_INT_CAUSES_S2D |
                IWX_MSIX_FH_INT_CAUSES_FH_ERR);
    IWX_CLRBITS(sc, IWX_CSR_MSIX_HW_INT_MASK_AD,
                IWX_MSIX_HW_INT_CAUSES_REG_ALIVE |
                IWX_MSIX_HW_INT_CAUSES_REG_WAKEUP |
                IWX_MSIX_HW_INT_CAUSES_REG_IML |
                IWX_MSIX_HW_INT_CAUSES_REG_CT_KILL |
                IWX_MSIX_HW_INT_CAUSES_REG_RF_KILL |
                IWX_MSIX_HW_INT_CAUSES_REG_PERIODIC |
                (sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ ?
                 IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR_V2 :
                 IWX_MSIX_HW_INT_CAUSES_REG_SW_ERR) |
                IWX_MSIX_HW_INT_CAUSES_REG_SCD |
                IWX_MSIX_HW_INT_CAUSES_REG_FH_TX |
                IWX_MSIX_HW_INT_CAUSES_REG_HW_ERR |
                IWX_MSIX_HW_INT_CAUSES_REG_HAP);
    IWX_WRITE(sc,IWX_CSR_MSIX_AUTOMASK_ST_AD,1U); // clear any pre-admission masked edge
    return 0;
}

int ItlIwx::
iwx_bz_port_start_hw(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    int err;

    err = iwx_prepare_card_hw(sc);
    if (err)
        return err;

    iwx_clear_persistence_bit(sc);

    /* Reset the entire device */
    if (sc->sc_device_family >= IWX_DEVICE_FAMILY_BZ) {
        IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
                    IWX_CSR_GP_CNTRL_REG_FLAG_SW_RESET);
        DELAY(10000);
        err = iwx_prepare_card_hw(sc);
        if (err)
            return err;
    } else {
        IWX_SETBITS(sc, IWX_CSR_RESET, IWX_CSR_RESET_REG_FLAG_SW_RESET);
        DELAY(5000);
    }

    if (sc->sc_device_family == IWX_DEVICE_FAMILY_22000 && sc->sc_integrated) {
        IWX_SETBITS(sc, IWX_CSR_GP_CNTRL,
                    IWX_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);
        DELAY(20);
        if (!iwx_poll_bit(sc, IWX_CSR_GP_CNTRL,
                          IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
                          IWX_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000)) {
            XYLog("%s: timeout waiting for clock stabilization\n",
                  DEVNAME(sc));
            return ETIMEDOUT;
        }

        iwx_force_power_gating(sc);

        /* Reset the entire device */
        IWX_SETBITS(sc, IWX_CSR_RESET, IWX_CSR_RESET_REG_FLAG_SW_RESET);
        DELAY(5000);
    }

    err = iwx_apm_init(sc);
    if (err)
        return err;

    err = iwx_bz_port_init_msix_hw(sc);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:msi-route-setup", err);
        return err;
    }

    iwx_enable_rfkill_int(sc);
    iwx_check_rfkill(sc);

    return 0;
}

void ItlIwx::
iwx_bz_port_nic_config(struct iwx_softc *sc)
{
    /* BZ uses Gen3 boot control at CSR 0, not legacy MAC/PHY config. */
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ)
        return;

    uint8_t radio_cfg_type, radio_cfg_step, radio_cfg_dash;
    uint32_t mask, val, reg_val = 0;

    radio_cfg_type = (sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RADIO_TYPE) >>
    IWX_FW_PHY_CFG_RADIO_TYPE_POS;
    radio_cfg_step = (sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RADIO_STEP) >>
    IWX_FW_PHY_CFG_RADIO_STEP_POS;
    radio_cfg_dash = (sc->sc_fw_phy_config & IWX_FW_PHY_CFG_RADIO_DASH) >>
    IWX_FW_PHY_CFG_RADIO_DASH_POS;

    reg_val |= IWX_CSR_HW_REV_STEP(sc->sc_hw_rev) <<
    IWX_CSR_HW_IF_CONFIG_REG_POS_MAC_STEP;
    reg_val |= IWX_CSR_HW_REV_DASH(sc->sc_hw_rev) <<
    IWX_CSR_HW_IF_CONFIG_REG_POS_MAC_DASH;

    /* radio configuration */
    reg_val |= radio_cfg_type << IWX_CSR_HW_IF_CONFIG_REG_POS_PHY_TYPE;
    reg_val |= radio_cfg_step << IWX_CSR_HW_IF_CONFIG_REG_POS_PHY_STEP;
    reg_val |= radio_cfg_dash << IWX_CSR_HW_IF_CONFIG_REG_POS_PHY_DASH;

    mask = IWX_CSR_HW_IF_CONFIG_REG_MSK_MAC_DASH |
    IWX_CSR_HW_IF_CONFIG_REG_MSK_MAC_STEP |
    IWX_CSR_HW_IF_CONFIG_REG_MSK_PHY_STEP |
    IWX_CSR_HW_IF_CONFIG_REG_MSK_PHY_DASH |
    IWX_CSR_HW_IF_CONFIG_REG_MSK_PHY_TYPE |
    IWX_CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI |
    IWX_CSR_HW_IF_CONFIG_REG_BIT_MAC_SI;

    val = IWX_READ(sc, IWX_CSR_HW_IF_CONFIG_REG);
    val &= ~mask;
    val |= reg_val;
    IWX_WRITE(sc, IWX_CSR_HW_IF_CONFIG_REG, val);

    XYLog("%s Radio type=0x%x-0x%x-0x%x\n", __FUNCTION__, radio_cfg_type,
          radio_cfg_step, radio_cfg_dash);
}

int ItlIwx::
iwx_bz_port_nic_rx_init(struct iwx_softc *sc)
{
    IWX_WRITE_1(sc, IWX_CSR_INT_COALESCING, IWX_HOST_INT_TIMEOUT_DEF);

    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
        /* Prepare host-owned state before ROM can publish completions. */
        bzResetRx(sc);
        memset(sc->rxq.stat,0,sc->rxq.stat_dma.size);
        if(fBzFault) return EIO;
        for (int i = 0; i < sc->rxq.count - 1; i++)
            bzUpdateRxDesc(sc, &sc->rxq, i, i);
    }

    /*
     * We don't configure the RFH; the firmware will do that.
     * BZ descriptors are submitted at ALIVE without resetting DMA state.
     * Older families also populate their descriptors at ALIVE.
     */
    return 0;
}

int ItlIwx::
iwx_bz_port_nic_init(struct iwx_softc *sc)
{
    int err;

    err = iwx_apm_init(sc);
    if (err)
        return err;
    iwx_nic_config(sc);

    err = iwx_nic_rx_init(sc);
    if (err)
        return err;

    IWX_SETBITS(sc, IWX_CSR_MAC_SHADOW_REG_CTRL, 0x800fffff);

    return 0;
}

int ItlIwx::
iwx_bz_port_schedule_protect_session(struct iwx_softc *sc, struct iwx_node *in,
                    uint32_t duration)
{
    struct iwx_session_prot_cmd cmd = {
        .id_and_color =
            htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color)),
        .action = htole32(IWX_FW_CTXT_ACTION_ADD),
        .duration_tu = htole32(duration * IEEE80211_DUR_TU),
    };
    int err;

    /*
     * SESSION_PROTECTION_CMD v2 on MLD firmware is indexed by link ID.
     * Request what Linux v6.11 iwl_mvm_protect_assoc() does: 900 ms
     * converted to TU, instead of duration * 1024 TU.
     */
    if (iwx_use_mld_api(sc)) {
        cmd.id_and_color = htole32(IWX_MLD_LINK_ID);
        cmd.duration_tu = htole32(900 * 1000 / IEEE80211_DUR_TU);
        sc->sc_pcitag->setProperty("ItlwmSessProtTu",
            le32toh(cmd.duration_tu), 32);
    }
    cmd.conf_id = IWX_SESSION_PROTECT_CONF_ASSOC;

    DPRINTFN(1, ("Add new session protection, duration %d TU\n",
             htole32(cmd.duration_tu)));

    err = iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_SESSION_PROTECTION_CMD, IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
    if (err)
        XYLog("Couldn't send the SESSION_PROTECTION_CMD %d\n", err);
    return err;
}

int ItlIwx::
iwx_bz_port_cancel_session_protection(struct iwx_softc *sc, struct iwx_node *in)
{
    struct iwx_session_prot_cmd cmd = {
        .id_and_color =
            htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color)),
        .action = htole32(IWX_FW_CTXT_ACTION_REMOVE),
        .conf_id = IWX_SESSION_PROTECT_CONF_ASSOC,
    };
    int err;

    if (iwx_use_mld_api(sc))
        cmd.id_and_color = htole32(IWX_MLD_LINK_ID);
    DPRINTFN(1, ("Remove session protection\n"));

    err = iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_SESSION_PROTECTION_CMD, IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
    if (err)
        XYLog("Couldn't send the Cancel SESSION_PROTECTION_CMD %d\n", err);
    return err;
}

void ItlIwx::
iwx_bz_port_setup_he_rates(struct iwx_softc *sc)
{
    struct ieee80211com *ic = &sc->sc_ic;
    ic->ic_flags &= ~IEEE80211_F_HEON;
    memset(&ic->ic_he_cap_elem, 0, sizeof(ic->ic_he_cap_elem));
    memset(&ic->ic_he_mcs_nss_supp, 0xff, sizeof(ic->ic_he_mcs_nss_supp));
    memset(ic->ic_ppe_thres, 0, sizeof(ic->ic_ppe_thres));
    uint8_t version = iwx_lookup_cmd_ver(sc, IWX_DATA_PATH_GROUP, IWX_STA_HE_CTXT_CMD);
    /* Preinit builds modecaps before media_init enables HT. Requiring HTON
     * here would omit 11ax from the initial capability table permanently.
     * Negotiation still requires HT and QoS on the associated node. */
    if (!sc->sc_nvm.sku_cap_11ax_enable ||
        !(ic->ic_userflags & IEEE80211_F_HEON) ||
        (ic->ic_userflags & IEEE80211_F_NOVHT) ||
        (version != 1 && version != 2))
        return;
    unsigned rx = iwx_num_of_ant(iwx_fw_valid_rx_ant(sc));
    unsigned tx = iwx_num_of_ant(iwx_fw_valid_tx_ant(sc));
    if (!rx || !tx)
        return;
    if (!iwx_mimo_enabled(sc))
        rx = tx = 1;
    /* Conservative station profile: no TWT, OMI, multi-TID or fragmentation. */
    ic->ic_he_cap_elem.mac_cap_info[0] = IEEE80211_HE_MAC_CAP0_HTC_HE;
    ic->ic_he_cap_elem.mac_cap_info[1] = IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US;
    ic->ic_he_cap_elem.mac_cap_info[2] = IEEE80211_HE_MAC_CAP2_32BIT_BA_BITMAP;
    ic->ic_he_cap_elem.mac_cap_info[4] = IEEE80211_HE_MAC_CAP4_AMDSU_IN_AMPDU;
    if (ic->ic_htcaps & IEEE80211_HTCAP_CBW20_40)
        ic->ic_he_cap_elem.phy_cap_info[0] |= IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G;
    if (sc->sc_nvm.sku_cap_band_52GHz_enable)
        ic->ic_he_cap_elem.phy_cap_info[0] |= IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G;
    ic->ic_he_cap_elem.phy_cap_info[1] = IEEE80211_HE_PHY_CAP1_DEVICE_CLASS_A |
        IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD;
    ic->ic_he_cap_elem.phy_cap_info[2] = IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US;
    ic->ic_he_cap_elem.phy_cap_info[7] = IEEE80211_HE_PHY_CAP7_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI;
    /* Require 16us padding without advertising an optional PPE field. */
    ic->ic_he_cap_elem.phy_cap_info[9] = IEEE80211_HE_PHY_CAP9_NOMIMAL_PKT_PADDING_16US |
        IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_COMP_SIGB |
        IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_NON_COMP_SIGB;
    ic->ic_he_mcs_nss_supp.rx_mcs_80 = htole16(rx > 1 ? 0xfffa : 0xfffe);
    ic->ic_he_mcs_nss_supp.tx_mcs_80 = htole16(tx > 1 ? 0xfffa : 0xfffe);
    if (sc->sc_nvm.vht160_supported) {
        ic->ic_he_cap_elem.phy_cap_info[0] |= IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G;
        ic->ic_he_mcs_nss_supp.rx_mcs_160 = ic->ic_he_mcs_nss_supp.rx_mcs_80;
        ic->ic_he_mcs_nss_supp.tx_mcs_160 = ic->ic_he_mcs_nss_supp.tx_mcs_80;
    }
    ic->ic_flags |= IEEE80211_F_HEON;
    XYLog("Experimental HE enabled: context-v%u rx80=%04x tx80=%04x width-bits=%02x\n",
        version, le16toh(ic->ic_he_mcs_nss_supp.rx_mcs_80),
        le16toh(ic->ic_he_mcs_nss_supp.tx_mcs_80), ic->ic_he_cap_elem.phy_cap_info[0]);
}

int ItlIwx::
iwx_bz_port_sta_rx_agg(struct iwx_softc *sc, struct ieee80211_node *ni, uint8_t tid,
               uint16_t ssn, uint16_t winsize, int timeout_val, int start)
{
    XYLog("%s start=%d tid=%d ssn=%d winsize=%d\n", __FUNCTION__, start, tid, ssn, winsize);
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_add_sta_cmd cmd;
    struct iwx_node *in = (struct iwx_node *)ni;
    int err, s, generation = sc->sc_generation;
    uint32_t status;
    struct iwx_rxba_data *rxba = NULL;
    uint8_t baid = 0;

    s = splnet();

    if (start && sc->sc_rx_ba_sessions >= IWX_MAX_RX_BA_SESSIONS) {
        ieee80211_addba_req_refuse(ic, ni, tid);
        splx(s);
        return ENOSPC;
    }

    if (iwx_use_mld_api(sc)) {
        /*
         * Firmware with BAID_ML_SUPPORT allocates the reorder ID with
         * RX_BAID_ALLOCATION_CONFIG_CMD (Linux iwl_mvm_fw_baid_op_cmd);
         * the BAID comes back in place of the command status.
         */
        struct iwx_rx_baid_cfg_cmd bcmd;

        memset(&bcmd, 0, sizeof(bcmd));
        if (start) {
            bcmd.action = htole32(IWX_RX_BAID_ACTION_ADD);
            bcmd.u.alloc.sta_id_mask = htole32(1 << IWX_STATION_ID);
            bcmd.u.alloc.tid = tid;
            bcmd.u.alloc.ssn = htole16(ssn);
            bcmd.u.alloc.win_size = htole16(winsize);
        } else {
            bcmd.action = htole32(IWX_RX_BAID_ACTION_REMOVE);
            bcmd.u.remove.sta_id_mask = htole32(1 << IWX_STATION_ID);
            bcmd.u.remove.tid = htole32(tid);
        }
        status = 0;
        err = iwx_send_cmd_pdu_status(sc,
            iwx_cmd_id(IWX_RX_BAID_ALLOCATION_CONFIG_CMD, IWX_DATA_PATH_GROUP, 0),
            sizeof(bcmd), &bcmd, &status);
        if (generation != sc->sc_generation) {
            splx(s);
            return ENXIO;
        }
        sc->sc_pcitag->setProperty("ItlwmMldRxBaErr",
            static_cast<UInt32>(err < 0 ? -err : err), 32);
        if (err || (start && status >= nitems(sc->sc_rxba_data))) {
            if (start)
                ieee80211_addba_req_refuse(ic, ni, tid);
            splx(s);
            return err ? err : EIO;
        }
        if (start) {
            baid = status;
            ++sc->sc_mld_rx_ba;
            sc->sc_pcitag->setProperty("ItlwmMldRxBa", sc->sc_mld_rx_ba, 32);
        }
    } else {
        memset(&cmd, 0, sizeof(cmd));

        cmd.sta_id = IWX_STATION_ID;
        cmd.mac_id_n_color
        = htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
        cmd.add_modify = IWX_STA_MODE_MODIFY;

        if (start) {
            cmd.add_immediate_ba_tid = (uint8_t)tid;
            cmd.add_immediate_ba_ssn = htole16(ssn);
            cmd.rx_ba_window = htole16(winsize);
        } else {
            cmd.remove_immediate_ba_tid = (uint8_t)tid;
        }
        cmd.modify_mask = start ? IWX_STA_MODIFY_ADD_BA_TID :
        IWX_STA_MODIFY_REMOVE_BA_TID;

        status = IWX_ADD_STA_SUCCESS;
        err = iwx_send_cmd_pdu_status(sc, IWX_ADD_STA, sizeof(cmd), &cmd,
                                      &status);

        if (generation != sc->sc_generation) {
            splx(s);
            return ENXIO;
        }
        if (err || (status & IWX_ADD_STA_STATUS_MASK) != IWX_ADD_STA_SUCCESS) {
            if (start)
                ieee80211_addba_req_refuse(ic, ni, tid);
            splx(s);
            return err ? err : EIO;
        }

        if (start) {
            if (!(status & IWX_ADD_STA_BAID_VALID_MASK)) {
                ieee80211_addba_req_refuse(ic, ni, tid);
                splx(s);
                return EIO;
            }
            baid = (status & IWX_ADD_STA_BAID_MASK) >>
            IWX_ADD_STA_BAID_SHIFT;
        }
    }

    /* Deaggregation is done in hardware. */
    if (start) {
        if (baid == IWX_RX_REORDER_DATA_INVALID_BAID ||
            baid >= nitems(sc->sc_rxba_data)) {
            ieee80211_addba_req_refuse(ic, ni, tid);
            splx(s);
            return EIO;
        }
        rxba = &sc->sc_rxba_data[baid];
        if (rxba->baid != IWX_RX_REORDER_DATA_INVALID_BAID) {
            ieee80211_addba_req_refuse(ic, ni, tid);
            splx(s);
            return EIO;
        }
        rxba->sta_id = IWX_STATION_ID;
        rxba->tid = tid;
        rxba->baid = baid;
        rxba->timeout = timeout_val;
        getmicrouptime(&rxba->last_rx);
        iwx_init_reorder_buffer(&rxba->reorder_buf, ssn,
                                winsize);
        if (timeout_val != 0) {
            struct ieee80211_rx_ba *ba;
            timeout_add_usec(&rxba->session_timer,
                             timeout_val);
            /* XXX disable net80211's BA timeout handler */
            ba = &ni->ni_rx_ba[tid];
            ba->ba_timeout_val = 0;
        }
    } else {
        int i;
        for (i = 0; i < nitems(sc->sc_rxba_data); i++) {
            rxba = &sc->sc_rxba_data[i];
            if (rxba->baid ==
                IWX_RX_REORDER_DATA_INVALID_BAID)
                continue;
            if (rxba->tid != tid)
                continue;
            iwx_clear_reorder_buffer(sc, rxba);
            break;
        }
    }

    if (start) {
        sc->sc_rx_ba_sessions++;
        ieee80211_addba_req_accept(ic, ni, tid);
    } else if (sc->sc_rx_ba_sessions > 0)
        sc->sc_rx_ba_sessions--;

    splx(s);
    return 0;
}

void ItlIwx::
iwx_bz_port_mac_ctxt_task(void *arg)
{
    struct iwx_softc *sc = (struct iwx_softc *)arg;
    ItlIwx *that = container_of(sc, ItlIwx, com);
    if (!that->getMainWorkLoop()->inGate()) {
        that->getMainCommandGate()->runAction(
            [](OSObject *, void *arg0, void *, void *, void *) -> IOReturn {
                iwx_mac_ctxt_task(arg0);
                return kIOReturnSuccess;
            }, arg);
        return;
    }
    struct ieee80211com *ic = &sc->sc_ic;

    struct iwx_node *in = (struct iwx_node *)ic->ic_bss;
    int err, s = splnet();

    if (sc->sc_flags & IWX_FLAG_SHUTDOWN ||
        ic->ic_state != IEEE80211_S_RUN ||
        in->in_phyctxt == NULL) {
        //        refcnt_rele_wake(&sc->task_refs);
        splx(s);
        return;
    }

    /* MLD firmware carries ERP/HT protection and EDCA on the link. */
    if (that->iwx_use_mld_api(sc)) {
        err = that->iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY,
            IWX_LINK_CONTEXT_MODIFY_RATES_INFO |
            IWX_LINK_CONTEXT_MODIFY_PROTECT_FLAGS |
            IWX_LINK_CONTEXT_MODIFY_QOS_PARAMS, 1);
        if (err)
            XYLog("%s: failed to update link (error %d)\n", DEVNAME(sc), err);
        splx(s);
        return;
    }

    err = 0; /* No HE in initial legacy profile. */
    if (err) {
        XYLog("HE context update failed: %d\n", err);
        splx(s);
        return;
    }
    err = that->iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 1);
    if (err) {
        printf("%s: failed to update MAC\n", DEVNAME(sc));
        splx(s);
        return;
    }

    if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_SESSION_PROT_CMD))
        that->iwx_unprotect_session(sc, in);

    //    refcnt_rele_wake(&sc->task_refs);
    splx(s);
}

void ItlIwx::
iwx_bz_port_chan_ctxt_task(void *arg)
{
    struct iwx_softc *sc = (struct iwx_softc *)arg;
    ItlIwx *that = container_of(sc, ItlIwx, com);
    if (!that->getMainWorkLoop()->inGate()) {
        that->getMainCommandGate()->runAction(
            [](OSObject *, void *arg0, void *, void *, void *) -> IOReturn {
                iwx_chan_ctxt_task(arg0);
                return kIOReturnSuccess;
            }, arg);
        return;
    }
    struct ieee80211com *ic = &sc->sc_ic;

    struct iwx_node *in = (struct iwx_node *)ic->ic_bss;
    int chains = that->iwx_mimo_enabled(sc) ? 2 : 1;
    int err, s = splnet();

    if (sc->sc_flags & IWX_FLAG_SHUTDOWN ||
        ic->ic_state != IEEE80211_S_RUN ||
        in->in_phyctxt == NULL) {
        //        refcnt_rele_wake(&sc->task_refs);
        splx(s);
        return;
    }

    err = that->iwx_phy_ctxt_update(sc, in->in_phyctxt,
                                    in->in_phyctxt->channel, chains, chains, 0);
    if (err) {
        XYLog("%s: failed to update PHY (error %d)\n",
              __FUNCTION__, err);
        //        refcnt_rele_wake(&sc->task_refs);
        splx(s);
        return;
    }
    err = that->iwx_rs_init(sc, in, true);
    if (err) {
        XYLog("%s: could not update rate scaling (error %d)\n",
              __FUNCTION__, err);
        //        refcnt_rele_wake(&sc->task_refs);
        splx(s);
        return;
    }

    //    refcnt_rele_wake(&sc->task_refs);
    splx(s);
}

void ItlIwx::
iwx_bz_port_ba_task(void *arg)
{
    struct iwx_softc *sc = (struct iwx_softc *)arg;
    ItlIwx *that = container_of(sc, ItlIwx, com);
    if (!that->getMainWorkLoop()->inGate()) {
        that->getMainCommandGate()->runAction(
            [](OSObject *, void *arg0, void *, void *, void *) -> IOReturn {
                iwx_ba_task(arg0);
                return kIOReturnSuccess;
            }, arg);
        return;
    }
    struct ieee80211com *ic = &sc->sc_ic;

    struct ieee80211_node *ni = ic->ic_bss;
    struct ieee80211_tx_ba *ba;
    struct iwx_tx_ring *ring;
    int s = splnet();
    int generation = sc->sc_generation;
    int err = 0;
    int qid = 0;
    int tid;
    /*
     * Take the per-TID queue requests up front: an early exit must not
     * leave a request bit set, or iwx_tx() never asks for that TID again.
     */
    const uint8_t txq_req = sc->sc_mld_txq_pending;
    sc->sc_mld_txq_pending = 0;

    if (sc->sc_flags & IWX_FLAG_SHUTDOWN || ic->ic_state != IEEE80211_S_RUN ||
        sc->ns_nstate != IEEE80211_S_RUN) {
        //        refcnt_rele_wake(&sc->task_refs);
        splx(s);
        return;
    }

    for (tid = 0; tid < IWX_MAX_TID_COUNT && that->iwx_use_mld_api(sc); tid++) {
        if (!(txq_req & (1 << tid)))
            continue;
        if (sc->sc_flags & IWX_FLAG_SHUTDOWN || ic->ic_state != IEEE80211_S_RUN ||
            sc->ns_nstate != IEEE80211_S_RUN)
            break;
        if (!that->iwx_nic_lock(sc))
            break; /* the next frame for this TID retries the allocation */
        qid = that->iwx_tvqm_alloc_txq(sc, tid, 0);
        if (generation != sc->sc_generation) {
            splx(s);
            return;
        }
        that->iwx_nic_unlock(sc);
        if (qid < 0) {
            sc->sc_mld_txq_failed |= 1 << tid;
            XYLog("%s: could not allocate TX queue for tid %d (error %d)\n",
                  DEVNAME(sc), tid, qid);
            sc->sc_pcitag->setProperty("ItlwmMldTxqErr",
                static_cast<UInt32>(-qid), 32);
            continue;
        }
        uint32_t allocated = 0;
        for (int t = 0; t < IWX_MAX_TID_COUNT; t++)
            if (sc->sc_tid_data[t].qid > 0 &&
                sc->sc_tid_data[t].qid != IWX_INVALID_QUEUE)
                allocated |= 1 << t;
        sc->sc_pcitag->setProperty("ItlwmMldDataQueues", allocated, 32);
    }

    for (tid = 0; tid < IWX_MAX_TID_COUNT; tid++) {
        if (sc->sc_flags & IWX_FLAG_SHUTDOWN || ic->ic_state != IEEE80211_S_RUN ||
            sc->ns_nstate != IEEE80211_S_RUN)
            break;
        if (sc->ba_rx.start_tidmask & (1 << tid)) {
            struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
            XYLog("%s ba_rx_start tid=%d, ssn=%d\n", __FUNCTION__, tid, ba->ba_winstart);
            that->iwx_bz_port_sta_rx_agg(sc, ni, tid, ba->ba_winstart,
                                 ba->ba_winsize, ba->ba_timeout_val, 1);
            if (generation != sc->sc_generation) {
                splx(s);
                return;
            }
            sc->ba_rx.start_tidmask &= ~(1 << tid);
        } else if (sc->ba_rx.stop_tidmask & (1 << tid)) {
            that->iwx_bz_port_sta_rx_agg(sc, ni, tid, 0, 0, 0, 0);
            if (generation != sc->sc_generation) {
                splx(s);
                return;
            }
            sc->ba_rx.stop_tidmask &= ~(1 << tid);
        }
    }

    for (tid = 0; tid < IWX_MAX_TID_COUNT; tid++) {
        if (sc->sc_flags & IWX_FLAG_SHUTDOWN || ic->ic_state != IEEE80211_S_RUN ||
            sc->ns_nstate != IEEE80211_S_RUN)
            break;
        if (sc->ba_tx.start_tidmask & (1 << tid)) {
            ba = &ni->ni_tx_ba[tid];
            XYLog("%s ba_tx_start tid=%d, ssn=%d\n", __FUNCTION__, tid, ba->ba_winstart);
            if (!that->iwx_nic_lock(sc)) {
                err = -1;
                goto out;
            }
            if ((qid = that->iwx_tvqm_alloc_txq(sc, tid, ba->ba_winstart)) < 0) {
                err = -1;
                goto out;
            }
            ring = &sc->txq[qid];
            ba->ba_winstart = IWX_AGG_SSN_TO_TXQ_IDX(ring->cur, ring->ring_count);
            ba->ba_winend = (ba->ba_winstart + ba->ba_winsize - 1) & 0xfff;
            ba->ba_timeout_val = 0;
            ieee80211_addba_resp_accept(ic, ni, tid);
            XYLog("%s tx queue alloc succeed qid=%d ssn=%d\n", __FUNCTION__, qid, ba->ba_winstart);
        out:
            if (generation != sc->sc_generation) {
                splx(s);
                return;
            }
            that->iwx_nic_unlock(sc);
            if (err)
                ieee80211_addba_resp_refuse(ic, ni, tid,
                                            IEEE80211_STATUS_UNSPECIFIED);
            sc->ba_tx.start_tidmask &= ~(1 << tid);
        }
    }

    //    refcnt_rele_wake(&sc->task_refs);
    splx(s);
}

int ItlIwx::
iwx_bz_port_set_mac_addr_from_csr(struct iwx_softc *sc, struct iwx_nvm_data *data)
{
    uint32_t mac_addr0, mac_addr1;
    /* BZ moves the OTP/STRAP address block from 0x380 to 0x30. */
    const uint32_t base = sc->sc_device_family == IWX_DEVICE_FAMILY_BZ ?
        0x30 : IWX_CSR_ADDR_BASE;

    if (!iwx_nic_lock(sc))
        return EBUSY;

    mac_addr0 = htole32(IWX_READ(sc, base + 0x08));
    mac_addr1 = htole32(IWX_READ(sc, base + 0x0c));

    iwx_flip_hw_address(mac_addr0, mac_addr1, data->hw_addr);

    /*
     * If the OEM fused a valid address, use it instead of the one in the
     * OTP
     */
    if (iwx_is_valid_mac_addr(data->hw_addr))
        goto done;

    mac_addr0 = htole32(IWX_READ(sc, base));
    mac_addr1 = htole32(IWX_READ(sc, base + 0x04));

    iwx_flip_hw_address(mac_addr0, mac_addr1, data->hw_addr);

done:

    iwx_nic_unlock(sc);
    return 0;
}

int ItlIwx::
iwx_bz_port_nvm_get(struct iwx_softc *sc)
{
    struct iwx_nvm_get_info cmd = {};
    struct iwx_nvm_data *nvm = &sc->sc_nvm;
    struct iwx_host_cmd hcmd = {
        .flags = IWX_CMD_WANT_RESP | IWX_CMD_SEND_IN_RFKILL,
        .data = { &cmd, },
        .len = { sizeof(cmd) },
        .id = IWX_WIDE_ID(IWX_REGULATORY_AND_NVM_GROUP,
                          IWX_NVM_GET_INFO)
    };
    int err;
    uint32_t mac_flags;
    /*
     * All the values in iwx_nvm_get_info_rsp v4 are the same as
     * in v3, except for the channel profile part of the
     * regulatory.  So we can just access the new struct, with the
     * exception of the latter.
     */
    struct iwx_nvm_get_info_rsp *rsp;
    struct iwx_nvm_get_info_rsp_v3 *rsp_v3;
    int v4 = isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_REGULATORY_NVM_INFO);
    size_t resp_len = v4 ? sizeof(*rsp) : sizeof(*rsp_v3);

    hcmd.resp_pkt_len = sizeof(struct iwx_rx_packet) + resp_len;
    err = iwx_send_cmd(sc, &hcmd);
    if (err)
        return err;

    if (iwx_rx_packet_payload_len(hcmd.resp_pkt) != resp_len) {
        err = EIO;
        goto out;
    }

    memset(nvm, 0, sizeof(*nvm));

    err = iwx_set_mac_addr_from_csr(sc, nvm);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:nvm-read-mac", err);
        goto out;
    }
    if (!iwx_is_valid_mac_addr(nvm->hw_addr)) {
        XYLog("%s: no valid mac address was found\n", DEVNAME(sc));
        err = EINVAL;
        goto out;
    }

    rsp = (struct iwx_nvm_get_info_rsp *)hcmd.resp_pkt->data;

    /* Initialize general data */
    nvm->nvm_version = le16toh(rsp->general.nvm_version);
    nvm->n_hw_addrs = rsp->general.n_hw_addrs;

    /* Initialize MAC sku data */
    mac_flags = le32toh(rsp->mac_sku.mac_sku_flags);
    nvm->sku_cap_11ac_enable =
    !!(mac_flags & IWX_NVM_MAC_SKU_FLAGS_802_11AC_ENABLED);
    nvm->sku_cap_11n_enable =
    !!(mac_flags & IWX_NVM_MAC_SKU_FLAGS_802_11N_ENABLED);
    nvm->sku_cap_11ax_enable =
    !!(mac_flags & IWX_NVM_MAC_SKU_FLAGS_802_11AX_ENABLED);
    nvm->sku_cap_band_24GHz_enable =
    !!(mac_flags & IWX_NVM_MAC_SKU_FLAGS_BAND_2_4_ENABLED);
    nvm->sku_cap_band_52GHz_enable =
    !!(mac_flags & IWX_NVM_MAC_SKU_FLAGS_BAND_5_2_ENABLED);
    nvm->sku_cap_mimo_disable =
    !!(mac_flags & IWX_NVM_MAC_SKU_FLAGS_MIMO_DISABLED);

    /* Initialize PHY sku data */
    nvm->valid_tx_ant = (uint8_t)le32toh(rsp->phy_sku.tx_chains);
    nvm->valid_rx_ant = (uint8_t)le32toh(rsp->phy_sku.rx_chains);

    if (le32toh(rsp->regulatory.lar_enabled) &&
        isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_LAR_SUPPORT)) {
        nvm->lar_enabled = 1;
    }

    if (v4) {
        iwx_init_channel_map(sc, NULL,
                             rsp->regulatory.channel_profile, IWX_NUM_CHANNELS);
    } else {
        rsp_v3 = (struct iwx_nvm_get_info_rsp_v3 *)rsp;
        iwx_init_channel_map(sc, rsp_v3->regulatory.channel_profile,
                             NULL, IWX_NUM_CHANNELS_V1);
    }
out:
    iwx_free_resp(sc, &hcmd);
    return err;
}

int ItlIwx::
iwx_gate_sleep(struct iwx_softc *sc, void *event, uint32_t *budget_ms, int wait)
{
    AbsoluteTime slice;
    clock_interval_to_deadline(IWX_GATE_SLICE_MS, kMillisecondScale,
        reinterpret_cast<uint64_t *>(&slice));
    const uint64_t started = iwx_boot_time_ns();
    int result = getMainCommandGate()->commandSleep(event, slice, THREAD_UNINT);
    /* A sleep longer than its slice was waiting to get the gate back. */
    const uint64_t slept_ms = (iwx_boot_time_ns() - started) / 1000000;
    if (slept_ms >= IWX_GATE_STALL_REPORT_MS) {
        uint32_t ms = slept_ms > 0xffffffffULL ? 0xffffffffU : (uint32_t)slept_ms;
        if (ms >= 1000) {
            ++sc->sc_gate_stalls;
            sc->sc_pcitag->setProperty("ItlwmGateStalls", sc->sc_gate_stalls, 32);
        }
        if (ms > sc->sc_gate_stall_max_ms) {
            sc->sc_gate_stall_max_ms = ms;
            sc->sc_pcitag->setProperty("ItlwmGateStallMaxMs", ms, 32);
            sc->sc_pcitag->setProperty("ItlwmGateStallWait", wait, 32);
        }
    }
    if(result!=THREAD_AWAKENED && result!=THREAD_TIMED_OUT) return EINTR;
    // Count wall time, including unrelated wakeups and gate reacquisition.
    // A stream of early wakes must not make a firmware wait unbounded.
    const uint32_t elapsed=slept_ms>0xffffffffULL?0xffffffffU:(slept_ms?uint32_t(slept_ms):1U);
    if(*budget_ms<=elapsed) {*budget_ms=0;return EWOULDBLOCK;}
    *budget_ms-=elapsed;
    return 0;
}

int ItlIwx::
iwx_wait_notification(struct iwx_softc *sc, int *value, int bits, uint32_t milliseconds)
{
    int generation = sc->sc_generation;
    uint32_t budget = milliseconds;
    while (generation == sc->sc_generation && (*value & bits) != bits) {
        if (sc->sc_boot_diag.active && value == &sc->sc_uc.uc_intr && sc->sc_boot_diag.error)
            return sc->sc_boot_diag.error;
        int err = iwx_gate_sleep(sc, value, &budget, IWX_GATE_WAIT_NOTIF);
        if (generation != sc->sc_generation)
            return ENXIO;
        if (sc->sc_boot_diag.active && value == &sc->sc_uc.uc_intr && sc->sc_boot_diag.error)
            return sc->sc_boot_diag.error;
        if (err && (*value & bits) != bits)
            return err;
    }
    if (generation != sc->sc_generation)
        return ENXIO;
    if (sc->sc_boot_diag.active && value == &sc->sc_uc.uc_intr && sc->sc_boot_diag.error)
        return sc->sc_boot_diag.error;
    return 0;
}

int ItlIwx::
iwx_bz_poll_msix_causes(struct iwx_softc *sc)
{
    uint32_t fh = IWX_READ(sc, IWX_CSR_MSIX_FH_INT_CAUSES_AD);
    uint32_t hw = IWX_READ(sc, IWX_CSR_MSIX_HW_INT_CAUSES_AD);
    uint32_t fh_rx;

    if (fh == 0xffffffff || hw == 0xffffffff)
        return 0;
    fh_rx = fh & (IWX_MSIX_FH_INT_CAUSES_Q0 | IWX_MSIX_FH_INT_CAUSES_Q1);
    if (!fh_rx)
        return 0;

    IWX_WRITE(sc, IWX_CSR_MSIX_FH_INT_CAUSES_AD, fh_rx);
    sc->sc_boot_diag.alive_poll_hits++;
    iwx_notif_intr(sc);
    return 1;
}

int ItlIwx::
iwx_wait_alive_poll(struct iwx_softc *sc, int *value, int bits, uint32_t milliseconds)
{
    int generation = sc->sc_generation;
    uint32_t remaining = milliseconds;

    while (generation == sc->sc_generation && (*value & bits) != bits) {
        if (sc->sc_boot_diag.active && sc->sc_boot_diag.error)
            return sc->sc_boot_diag.error;
        if (sc->sc_boot_diag.active)
            iwx_bz_poll_msix_causes(sc);
        if ((*value & bits) == bits)
            break;
        int err = iwx_gate_sleep(sc, value, &remaining, IWX_GATE_WAIT_ALIVE);
        if (generation != sc->sc_generation)
            return ENXIO;
        if (sc->sc_boot_diag.active && sc->sc_boot_diag.error)
            return sc->sc_boot_diag.error;
        if (err && (*value & bits) != bits)
            return err;
    }
    if (generation != sc->sc_generation)
        return ENXIO;
    if (sc->sc_boot_diag.active && sc->sc_boot_diag.error)
        return sc->sc_boot_diag.error;
    return 0;
}

int ItlIwx::
iwx_bz_port_load_firmware(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct iwx_fw_sects *fws;
    int err;

    sc->sc_uc.uc_intr = 0;
    sc->sc_uc.uc_ok = 0;
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
        memset(&sc->sc_boot_diag, 0, sizeof(sc->sc_boot_diag));
        sc->sc_boot_diag.active = true;
        sc->sc_pcitag->setProperty("ItlwmBootErrorTablesCaptured", false);
        __atomic_store_n(&sc->sc_boot_filter_count, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&sc->sc_boot_filter_active, true, __ATOMIC_RELEASE);
    }
    ItlStartupStage(sc->sc_pcitag, "hal:firmware-context");

    fws = &sc->sc_fw.fw_sects[IWX_UCODE_TYPE_REGULAR];
    if (sc->sc_device_family >= IWX_DEVICE_FAMILY_AX210)
        err = iwx_ctxt_info_gen3_init(sc, fws);
    else
        err = iwx_ctxt_info_init(sc, fws);
    if (err) {
        sc->sc_boot_diag.active = false;
        __atomic_store_n(&sc->sc_boot_filter_active, false, __ATOMIC_RELEASE);
        XYLog("%s: could not init context info\n", DEVNAME(sc));
        ItlStartupFailure(sc->sc_pcitag, "hal:firmware-context", err);
        return err;
    }

    ItlStartupStage(sc->sc_pcitag, "hal:firmware-wait-alive");
    const uint64_t wait_started = iwx_boot_time_ns();
    /*
     * Linux allows two seconds for the BZ firmware ALIVE notification. BZ
     * also gets the cause-bank poll: the MSI-X path should deliver it, and
     * if MSI-X was refused this still lets the RX ring prove itself.
     */
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ)
        err = iwx_wait_alive_poll(sc, &sc->sc_uc.uc_intr, 1, 2000);
    else
        err = iwx_wait_notification(sc, &sc->sc_uc.uc_intr, 1, 1000);
    const uint64_t wait_finished = iwx_boot_time_ns();
    sc->sc_boot_diag.active = false;
    __atomic_store_n(&sc->sc_boot_filter_active, false, __ATOMIC_RELEASE);
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
        /* Retained data only: safe even if the device disappeared. */
        const struct iwx_boot_diagnostics &d = sc->sc_boot_diag;
        sc->sc_pcitag->setProperty("ItlwmBootIrqError", d.error, 32);
        sc->sc_pcitag->setProperty("ItlwmBootFatalCause", d.fatal_cause, 32);
        sc->sc_pcitag->setProperty("ItlwmBootFwloadMaskReadback", d.fwload_mask_readback, 32);
        sc->sc_pcitag->setProperty("ItlwmBootAliveMaskBefore", d.alive_mask_before, 32);
        sc->sc_pcitag->setProperty("ItlwmBootAliveRouteRewrites", static_cast<unsigned long long>(0), 32);
        sc->sc_pcitag->setProperty("ItlwmBootAliveRxDoorbell", d.alive_rx_doorbell, 32);
        sc->sc_pcitag->setProperty("ItlwmBootAliveRxDoorbellWrites", d.alive_rx_doorbell_writes, 32);
        sc->sc_pcitag->setProperty("ItlwmBootAlivePollHits", d.alive_poll_hits, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreMaskExpected", d.restore_expected, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreMaskFirst", d.restore_first, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreMaskLast", d.restore_last, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreChecks", d.restore_checks, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreRetries", static_cast<unsigned long long>(0), 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreMismatches", d.restore_mismatches, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreFilterRaces", d.restore_raced, 32);
        sc->sc_pcitag->setProperty("ItlwmBootFwloadMaskTimeNs", d.fwload_mask_time_ns, 64);
        sc->sc_pcitag->setProperty("ItlwmBootRomStartTimeNs", d.rom_start_time_ns, 64);
        sc->sc_pcitag->setProperty("ItlwmBootHardwareAliveTimeNs", d.alive_time_ns, 64);
        sc->sc_pcitag->setProperty("ItlwmBootRxDoorbellTimeNs", d.alive_rx_doorbell_time_ns, 64);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreFirstTimeNs", d.restore_first_time_ns, 64);
        sc->sc_pcitag->setProperty("ItlwmBootRestoreLastTimeNs", d.restore_last_time_ns, 64);
        sc->sc_pcitag->setProperty("ItlwmBootWaitStartTimeNs", wait_started, 64);
        sc->sc_pcitag->setProperty("ItlwmBootWaitEndTimeNs", wait_finished, 64);
        sc->sc_pcitag->setProperty("ItlwmBootWaitDurationNs", wait_finished - wait_started, 64);
        if (err != ENXIO)
            iwx_bz_record_pci_irq_mode(sc, false);
    }
    if (err == ENXIO) {
        ItlStartupFailure(sc->sc_pcitag, "hal:firmware-wait-alive", err);
        return err;
    }
    sc->sc_pcitag->setProperty("ItlwmFirmwareAliveInterrupt", sc->sc_uc.uc_intr, 32);
    sc->sc_pcitag->setProperty("ItlwmFirmwareAliveValid", sc->sc_uc.uc_ok, 32);
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
        // Actual private transport counters; legacy reference boot counters
        // below do not observe bzInterrupt/bzNotif. Boot-thread export only.
        sc->sc_pcitag->setProperty("KGP_BE200_INIT_FILTER_COUNT",
            __atomic_load_n(&fBzStats[0], __ATOMIC_RELAXED), 64);
        sc->sc_pcitag->setProperty("KGP_BE200_INIT_ACTION_COUNT", fBzStats[1], 64);
        sc->sc_pcitag->setProperty("KGP_BE200_INIT_RX_CLAIMS", fBzStats[7], 64);
        sc->sc_pcitag->setProperty("KGP_BE200_INIT_ALIVE_COUNT", fBzStats[8], 64);
        sc->sc_pcitag->setProperty("KGP_BE200_ALIVE_SKU0", sc->sku_id[0], 32);
        sc->sc_pcitag->setProperty("KGP_BE200_ALIVE_SKU1", sc->sku_id[1], 32);
        sc->sc_pcitag->setProperty("KGP_BE200_ALIVE_SKU2", sc->sku_id[2], 32);
        /* uc_intr is a parsed firmware message, not the hardware ALIVE IRQ. */
        sc->sc_pcitag->setProperty("ItlwmBootParsedAlive", sc->sc_uc.uc_intr, 32);
        sc->sc_pcitag->setProperty("ItlwmBootMsixMode", sc->sc_msix, 32);
        sc->sc_pcitag->setProperty("ItlwmBootIrqCallbacks", sc->sc_boot_diag.callbacks, 32);
        sc->sc_pcitag->setProperty("ItlwmBootIrqFilterCalls",
            __atomic_load_n(&sc->sc_boot_filter_count, __ATOMIC_RELAXED), 32);
        sc->sc_pcitag->setProperty("ItlwmBootHardwareAliveIrqs", sc->sc_boot_diag.hardware_alive, 32);
        sc->sc_pcitag->setProperty("ItlwmBootRxCauseIrqs", sc->sc_boot_diag.rx_causes, 32);
        sc->sc_pcitag->setProperty("ItlwmBootLegacyCauseBits", sc->sc_boot_diag.legacy_cause_bits, 32);
        sc->sc_pcitag->setProperty("ItlwmBootLegacyFhCauseBits", sc->sc_boot_diag.legacy_fh_cause_bits, 32);
        sc->sc_pcitag->setProperty("ItlwmBootMsixFhCauseBits", sc->sc_boot_diag.msix_fh_cause_bits, 32);
        sc->sc_pcitag->setProperty("ItlwmBootMsixHwCauseBits", sc->sc_boot_diag.msix_hw_cause_bits, 32);
        sc->sc_pcitag->setProperty("ItlwmBootFailureSnapshot", false);
    }
    if (!err && !sc->sc_uc.uc_ok)
        err = EIO;
    if (err || !sc->sc_uc.uc_ok) {
        ItlStartupFailure(sc->sc_pcitag, "hal:firmware-wait-alive", err);
        if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
            /* Read-only snapshot before failure cleanup; never touch DMA after ENXIO. */
            uint32_t csr_int = IWX_READ(sc, IWX_CSR_INT);
            uint32_t csr_mask = IWX_READ(sc, IWX_CSR_INT_MASK);
            uint32_t iml_response = IWX_READ(sc, IWX_CSR_IML_RESP_ADDR);
            uint32_t fh_status = IWX_READ(sc, IWX_CSR_FH_INT_STATUS);
            uint32_t msix_hw = IWX_READ(sc, IWX_CSR_MSIX_HW_INT_CAUSES_AD);
            uint32_t msix_fh = IWX_READ(sc, IWX_CSR_MSIX_FH_INT_CAUSES_AD);
            sc->sc_pcitag->setProperty("ItlwmBootFailureSnapshot", true);
            sc->sc_pcitag->setProperty("ItlwmBootCsrInt", csr_int, 32);
            sc->sc_pcitag->setProperty("ItlwmBootCsrIntMask", csr_mask, 32);
            sc->sc_pcitag->setProperty("ItlwmBootImlResponse", iml_response, 32);
            sc->sc_pcitag->setProperty("ItlwmBootFhPending", fh_status, 32);
            sc->sc_pcitag->setProperty("ItlwmBootMsixHwPending", msix_hw, 32);
            sc->sc_pcitag->setProperty("ItlwmBootMsixFhPending", msix_fh, 32);
            sc->sc_pcitag->setProperty("ItlwmBootGpControl", IWX_READ(sc, IWX_CSR_GP_CNTRL), 32);
            sc->sc_pcitag->setProperty("ItlwmBootReset", IWX_READ(sc, IWX_CSR_RESET), 32);
            sc->sc_pcitag->setProperty("ItlwmBootPciCommand",
                sc->sc_pcitag->configRead16(kIOPCIConfigCommand), 32);
            int msi_offset = 0;
            if (pci_get_capability(sc->sc_pct, sc->sc_pcitag, PCI_CAP_MSI, &msi_offset, NULL))
                sc->sc_pcitag->setProperty("ItlwmBootMsiCapability",
                    pci_conf_read(sc->sc_pct, sc->sc_pcitag, msi_offset), 32);
            uint64_t context_addr = IWX_READ(sc, IWX_CSR_CTXT_INFO_ADDR);
            context_addr |= (uint64_t)IWX_READ(sc, IWX_CSR_CTXT_INFO_ADDR + 4) << 32;
            uint64_t iml_addr = IWX_READ(sc, IWX_CSR_IML_DATA_ADDR);
            iml_addr |= (uint64_t)IWX_READ(sc, IWX_CSR_IML_DATA_ADDR + 4) << 32;
            sc->sc_pcitag->setProperty("ItlwmBootContextAddressReadback", context_addr, 64);
            sc->sc_pcitag->setProperty("ItlwmBootImlAddressReadback", iml_addr, 64);
            sc->sc_pcitag->setProperty("ItlwmBootImlSizeReadback", IWX_READ(sc, IWX_CSR_IML_SIZE_ADDR), 32);
            if (sc->prph_info_dma.vaddr) {
                volatile struct iwx_prph_info *info =
                    (volatile struct iwx_prph_info *)sc->prph_info_dma.vaddr;
                sc->sc_pcitag->setProperty("ItlwmBootStageMirror",
                    le32toh(info->boot_stage_mirror), 32);
                sc->sc_pcitag->setProperty("ItlwmBootIpcStatusMirror",
                    le32toh(info->ipc_status_mirror), 32);
            }
            if (sc->rxq.stat)
                sc->sc_pcitag->setProperty("ItlwmBootRxProducerHead",
                    le16toh(*(volatile uint16_t *)sc->rxq.stat), 32);
            sc->sc_pcitag->setProperty("ItlwmBootRxConsumer", sc->rxq.cur, 32);
            sc->sc_pcitag->setProperty("ItlwmBootSecBootSnapshotValid", false);
        }
        if (iwx_nic_lock(sc)) {
            uint32_t secboot_cpu1 = iwx_read_umac_prph(sc, IWX_UMAG_SB_CPU_1_STATUS);
            uint32_t secboot_cpu2 = iwx_read_umac_prph(sc, IWX_UMAG_SB_CPU_2_STATUS);
            uint32_t umac_pc = iwx_read_umac_prph(sc, IWX_UREG_UMAC_CURRENT_PC);
            uint32_t lmac_pc = iwx_read_umac_prph(sc,
                sc->sc_device_family == IWX_DEVICE_FAMILY_BZ ?
                IWX_BZ_UREG_LMAC1_CURRENT_PC : IWX_UREG_LMAC1_CURRENT_PC);
            uint32_t umac_pc1 = 0;
            uint32_t msi_route = 0, power_state = 0, lmac_power = 0;
            uint32_t otp_address = 0, otp_data = 0;
            if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
                umac_pc1 = iwx_read_umac_prph(sc, IWX_BZ_UREG_UMAC_CURRENT_PC1);
                msi_route = iwx_read_umac_prph(sc, IWX_UREG_CHICK);
                lmac_power = iwx_read_umac_prph(sc, IWX_WFPM_LMAC1_PD_NOTIFICATION);
                power_state = iwx_read_umac_prph(sc, IWX_HPM_SECONDARY_DEVICE_STATE);
                otp_address = iwx_read_umac_prph(sc, IWX_WFPM_MAC_OTP_CFG7_ADDR);
                otp_data = iwx_read_umac_prph(sc, IWX_WFPM_MAC_OTP_CFG7_DATA);
            }
            iwx_nic_unlock(sc);
            XYLog("SecBoot CPU1 Status: 0x%x, CPU2 Status: 0x%x\n",
                  secboot_cpu1, secboot_cpu2);
            XYLog("UMAC PC: 0x%x\n", umac_pc);
            XYLog("LMAC PC: 0x%x\n", lmac_pc);
            if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ) {
                sc->sc_pcitag->setProperty("ItlwmBootSecBootSnapshotValid", true);
                sc->sc_pcitag->setProperty("ItlwmBootSecBootCpu1", secboot_cpu1, 32);
                sc->sc_pcitag->setProperty("ItlwmBootSecBootCpu2", secboot_cpu2, 32);
                sc->sc_pcitag->setProperty("ItlwmBootUmacPc", umac_pc, 32);
                sc->sc_pcitag->setProperty("ItlwmBootUmacPc1", umac_pc1, 32);
                sc->sc_pcitag->setProperty("ItlwmBootLmacPc", lmac_pc, 32);
                sc->sc_pcitag->setProperty("ItlwmBootMsiRoute", msi_route, 32);
                sc->sc_pcitag->setProperty("ItlwmBootLmacPower", lmac_power, 32);
                sc->sc_pcitag->setProperty("ItlwmBootSecondaryPower", power_state, 32);
                sc->sc_pcitag->setProperty("ItlwmBootOtpCfg7Address", otp_address, 32);
                sc->sc_pcitag->setProperty("ItlwmBootOtpCfg7Data", otp_data, 32);
            }
        }
        if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ)
            iwx_bz_capture_error_tables(sc);
        /* BZ retains paging until positive hardware stop. */
        XYLog("%s: could not load firmware\n", DEVNAME(sc));
    }

    /* BZ retains firmware image descriptors until positive stop. */

    /* BZ retains IML until positive stop. */

    if (err)
        return err;

    XYLog("%s: load firmware ok\n", DEVNAME(sc));
    ItlStartupStage(sc->sc_pcitag, "hal:firmware-alive");

    return err;
}

int ItlIwx::
iwx_bz_port_start_fw(struct iwx_softc *sc)
{

    XYLog("%s\n", __FUNCTION__);
    int err;

    ItlStartupStage(sc->sc_pcitag, "hal:firmware-prepare-card");
    err = iwx_prepare_card_hw(sc);
    if (err) {
        XYLog("%s: could not initialize hardware\n", DEVNAME(sc));
        ItlStartupFailure(sc->sc_pcitag, "hal:firmware-prepare-card", err);
        return err;
    }

    iwx_enable_rfkill_int(sc);

    IWX_WRITE(sc, IWX_CSR_INT, 0xFFFFFFFF);

    iwx_disable_interrupts(sc);

    /* make sure rfkill handshake bits are cleared */
    IWX_WRITE(sc, IWX_CSR_UCODE_DRV_GP1_CLR, IWX_CSR_UCODE_SW_BIT_RFKILL);
    IWX_WRITE(sc, IWX_CSR_UCODE_DRV_GP1_CLR,
              IWX_CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);

    /* clear (again), then enable firwmare load interrupt */
    IWX_WRITE(sc, IWX_CSR_INT, ~0);

    ItlStartupStage(sc->sc_pcitag, "hal:firmware-nic-init");
    err = iwx_nic_init(sc);
    if (err) {
        XYLog("%s: unable to init nic\n", DEVNAME(sc));
        ItlStartupFailure(sc->sc_pcitag, "hal:firmware-nic-init", err);
        return err;
    }

    return iwx_load_firmware(sc);
}

int ItlIwx::
iwx_bz_port_load_ucode_wait_alive(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    int err;

    ItlStartupStage(sc->sc_pcitag, "hal:firmware-read");
    err = iwx_read_firmware(sc);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:firmware-read", err);
        return err;
    }

    err = iwx_start_fw(sc);
    if (err)
        return err;

    ItlStartupStage(sc->sc_pcitag, "hal:pnvm-load");
    err = iwx_load_pnvm(sc);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:pnvm-load", err);
        return err;
    }

    ItlStartupStage(sc->sc_pcitag, "hal:firmware-post-alive");
    iwx_post_alive(sc);

    return 0;
}

int ItlIwx::
iwx_bz_port_run_init_mvm_ucode(struct iwx_softc *sc, int readnvm)
{
    XYLog("%s\n", __FUNCTION__);
    const int wait_flags = IWX_INIT_COMPLETE;
    struct iwx_nvm_access_complete_cmd nvm_complete = {};
    struct iwx_init_extended_cfg_cmd init_cfg = {
        .init_flags = htole32(IWX_INIT_NVM),
    };
    int err;

    if ((sc->sc_flags & IWX_FLAG_RFKILL) && !readnvm) {
        XYLog("%s: radio is disabled by hardware switch\n",
            DEVNAME(sc));
        ItlStartupFailure(sc->sc_pcitag, "hal:radio-killed", EPERM);
        return EPERM;
    }

    sc->sc_init_complete = 0;
    err = iwx_load_ucode_wait_alive(sc);
    if (err) {
        XYLog("%s: failed to load init firmware\n", DEVNAME(sc));
        return err;
    }

    if (sc->sc_tx_with_siso_diversity)
        init_cfg.init_flags |= htole32(IWX_INIT_PHY);

    /*
     * Send init config command to mark that we are sending NVM
     * access commands
     */
    ItlStartupStage(sc->sc_pcitag, "hal:nvm-init-config-command");
    err = iwx_send_cmd_pdu(sc, IWX_WIDE_ID(IWX_SYSTEM_GROUP,
                                           IWX_INIT_EXTENDED_CFG_CMD), IWX_CMD_SEND_IN_RFKILL, sizeof(init_cfg), &init_cfg);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:nvm-init-config-command", err);
        return err;
    }

    ItlStartupStage(sc->sc_pcitag, "hal:nvm-access-complete-command");
    err = iwx_send_cmd_pdu(sc, IWX_WIDE_ID(IWX_REGULATORY_AND_NVM_GROUP,
                                           IWX_NVM_ACCESS_COMPLETE), IWX_CMD_SEND_IN_RFKILL, sizeof(nvm_complete), &nvm_complete);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:nvm-access-complete-command", err);
        return err;
    }

    /* INIT_COMPLETE can arrive before NVM_ACCESS_COMPLETE is acknowledged. */
    ItlStartupStage(sc->sc_pcitag, "hal:nvm-wait-init-complete");
    err = iwx_wait_notification(sc, &sc->sc_init_complete, wait_flags, 2000);
    if (err) {
        ItlStartupFailure(sc->sc_pcitag, "hal:nvm-wait-init-complete", err);
        return err;
    }

    if (readnvm) {
        ItlStartupStage(sc->sc_pcitag, "hal:nvm-read");
        err = iwx_nvm_get(sc);
        if (err) {
            XYLog("%s: failed to read nvm\n", DEVNAME(sc));
            ItlStartupFailure(sc->sc_pcitag, "hal:nvm-read", err);
            return err;
        }
        if (IEEE80211_ADDR_EQ(etheranyaddr, sc->sc_ic.ic_myaddr))
            IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr,
                sc->sc_nvm.hw_addr);

    }
    return 0;
}

int ItlIwx::
iwx_bz_port_oldsn_workaround(struct iwx_softc *sc, struct ieee80211_node *ni, int tid,
    struct iwx_reorder_buffer *buffer, uint32_t reorder_data, uint32_t gp2)
{
    struct ieee80211com *ic = &sc->sc_ic;

    if (gp2 != buffer->consec_oldsn_ampdu_gp2) {
        /* we have a new (A-)MPDU ... */

        /*
         * reset counter to 0 if we didn't have any oldsn in
         * the last A-MPDU (as detected by GP2 being identical)
         */
        if (!buffer->consec_oldsn_prev_drop)
            buffer->consec_oldsn_drops = 0;

        /* either way, update our tracking state */
        buffer->consec_oldsn_ampdu_gp2 = gp2;
    } else if (buffer->consec_oldsn_prev_drop) {
        /*
         * tracking state didn't change, and we had an old SN
         * indication before - do nothing in this case, we
         * already noted this one down and are waiting for the
         * next A-MPDU (by GP2)
         */
        return 0;
    }

    /* return unless this MPDU has old SN */
    if (!(reorder_data & IWX_RX_MPDU_REORDER_BA_OLD_SN))
        return 0;

    /* update state */
    buffer->consec_oldsn_prev_drop = 1;
    buffer->consec_oldsn_drops++;

    /* if limit is reached, send del BA and reset state */
    if (buffer->consec_oldsn_drops == IWX_AMPDU_CONSEC_DROPS_DELBA) {
        XYLog("reached %d old SN frames, stopping BA session on TID %d\n",
              IWX_AMPDU_CONSEC_DROPS_DELBA, tid);
        ieee80211_delba_request(ic, ni, IEEE80211_REASON_UNSPECIFIED,
            0, tid);
        buffer->consec_oldsn_prev_drop = 0;
        buffer->consec_oldsn_drops = 0;
        return 1;
    }

    return 0;
}

void ItlIwx::
iwx_bz_port_rx_mpdu_mq(struct iwx_softc *sc, mbuf_t m, void *pktdata,
               size_t maxlen, struct mbuf_list *ml)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_rxinfo rxi;
    struct iwx_rx_mpdu_desc *desc;
    uint32_t len, hdrlen, rate_n_flags, device_timestamp;
    int rssi;
    uint8_t chanidx;
    uint16_t phy_info;
    size_t desc_size;

    desc = (struct iwx_rx_mpdu_desc *)pktdata;
    if (sc->sc_device_family >= IWX_DEVICE_FAMILY_AX210)
        desc_size = sizeof(struct iwx_rx_mpdu_desc);
    else
        desc_size = IWX_RX_DESC_SIZE_V1;

    if (!(desc->status & htole16(IWX_RX_MPDU_RES_STATUS_CRC_OK)) ||
        !(desc->status & htole16(IWX_RX_MPDU_RES_STATUS_OVERRUN_OK))) {
        mbuf_freem(m);
        return; /* drop */
    }

    len = le16toh(desc->mpdu_len);
    if (ic->ic_opmode == IEEE80211_M_MONITOR) {
        /* Allow control frames in monitor mode. */
        if (len < sizeof(struct ieee80211_frame_cts)) {
            ic->ic_stats.is_rx_tooshort++;
            IC2IFP(ic)->netStat->inputErrors++;
            mbuf_freem(m);
            return;
        }
    } else if (len < sizeof(struct ieee80211_frame)) {
        ic->ic_stats.is_rx_tooshort++;
        IC2IFP(ic)->netStat->inputErrors++;
        mbuf_freem(m);
        return;
    }
    if (len > maxlen - desc_size) {
        IC2IFP(ic)->netStat->inputErrors++;
        mbuf_freem(m);
        return;
    }

    //    m->m_data = pktdata + sizeof(*desc);
    //    m->m_pkthdr.len = m->m_len = len;
    /*
     * Outside monitor mode, strip the MIC/CRC bytes the RX decryption block
     * may have left at the end of the frame (it can be disabled, e.g. for
     * management frames), as Linux iwl_mvm_create_skb() does. Without this
     * every management frame reached net80211 with a trailing 4-byte FCS.
     */
    if (ic->ic_opmode != IEEE80211_M_MONITOR) {
        uint32_t mic_crc_len = (desc->mac_flags1 &
            IWX_RX_MPDU_MFLG1_MIC_CRC_LEN_MASK) >>
            IWX_RX_MPDU_MFLG1_MIC_CRC_LEN_SHIFT;
        if (len > mic_crc_len + sizeof(struct ieee80211_frame))
            len -= mic_crc_len;
    }

    mbuf_setdata(m, (uint8_t*)pktdata + desc_size, len);
    mbuf_pkthdr_setlen(m, len);
    mbuf_setlen(m, len);

    /*
     * Count AUTH frames as early as possible so a missing reply can be told
     * apart from one dropped later. Body: algorithm, sequence, status.
     */
    {
        const uint8_t *frm = (const uint8_t *)mbuf_data(m);
        const size_t body = sizeof(struct ieee80211_frame) +
            ((desc->mac_flags2 & IWX_RX_MPDU_MFLG2_PAD) ? 2 : 0);
        if ((frm[0] & (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
            (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_AUTH)) {
            ++sc->sc_rx_auth;
            sc->sc_pcitag->setProperty("ItlwmRxAuth", sc->sc_rx_auth, 32);
            if (len >= body + 6) {
                sc->sc_pcitag->setProperty("ItlwmRxAuthSeq",
                    static_cast<UInt32>(frm[body + 2] | (frm[body + 3] << 8)), 32);
                ++fBzStats[9];
            }
        }
    }

    /* Account for padding following the frame header. */
    if (desc->mac_flags2 & IWX_RX_MPDU_MFLG2_PAD) {
        struct ieee80211_frame *wh = mtod(m, struct ieee80211_frame *);
        int type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
        if (type == IEEE80211_FC0_TYPE_CTL) {
            switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
                case IEEE80211_FC0_SUBTYPE_CTS:
                    hdrlen = sizeof(struct ieee80211_frame_cts);
                    break;
                case IEEE80211_FC0_SUBTYPE_ACK:
                    hdrlen = sizeof(struct ieee80211_frame_ack);
                    break;
                default:
                    hdrlen = sizeof(struct ieee80211_frame_min);
                    break;
            }
        } else
            hdrlen = ieee80211_get_hdrlen(wh);

        if ((le16toh(desc->status) &
             IWX_RX_MPDU_RES_STATUS_SEC_ENC_MSK) ==
            IWX_RX_MPDU_RES_STATUS_SEC_CCM_ENC) {
            /* Padding is inserted after the IV. */
            hdrlen += IEEE80211_CCMP_HDRLEN;
        }

        //        memmove(m->m_data + 2, m->m_data, hdrlen);
        memmove((uint8_t*)mbuf_data(m) + 2, mbuf_data(m), hdrlen);
        mbuf_adj(m, 2);
    }

    memset(&rxi, 0, sizeof(rxi));

    /*
     * Hardware de-aggregates A-MSDUs and copies the same MAC header
     * in place for each subframe. But it leaves the 'A-MSDU present'
     * bit set in the frame header. We need to clear this bit ourselves.
     * (XXX This workaround is not required on AX200/AX201 devices that
     * have been tested by me, but it's unclear when this problem was
     * fixed in the hardware. It definitely affects the 9k generation.
     * Leaving this in place for now since some 9k/AX200 hybrids seem
     * to exist that we may eventually add support for.)
     *
     * And we must allow the same CCMP PN for subframes following the
     * first subframe. Otherwise they would be discarded as replays.
     */
    if (desc->mac_flags2 & IWX_RX_MPDU_MFLG2_AMSDU) {
        struct ieee80211_frame *wh = mtod(m, struct ieee80211_frame *);
        uint8_t subframe_idx = (desc->amsdu_info &
                                IWX_RX_MPDU_AMSDU_SUBFRAME_IDX_MASK);
        if (subframe_idx > 0)
            rxi.rxi_flags |= IEEE80211_RXI_HWDEC_SAME_PN;
        if (ieee80211_has_qos(wh) && ieee80211_has_addr4(wh) &&
            mbuf_len(m) >= sizeof(struct ieee80211_qosframe_addr4)) {
            struct ieee80211_qosframe_addr4 *qwh4 = mtod(m,
                                                         struct ieee80211_qosframe_addr4 *);
            qwh4->i_qos[0] &= htole16(~IEEE80211_QOS_AMSDU);
        } else if (ieee80211_has_qos(wh) &&
                   mbuf_len(m) >= sizeof(struct ieee80211_qosframe)) {
            struct ieee80211_qosframe *qwh = mtod(m,
                                                  struct ieee80211_qosframe *);
            qwh->i_qos[0] &= htole16(~IEEE80211_QOS_AMSDU);
        }
    }

    /*
     * Verify decryption before duplicate detection. The latter uses
     * the TID supplied in QoS frame headers and this TID is implicitly
     * verified as part of the CCMP nonce.
     */
    if (iwx_rx_hwdecrypt(sc, m, le16toh(desc->status), &rxi)) {
        mbuf_freem(m);
        return;
    }

    if (iwx_detect_duplicate(sc, m, desc, &rxi)) {
        mbuf_freem(m);
        return;
    }

    rssi = iwx_rxmq_get_signal_strength(sc, desc);
    rssi = (0 - IWX_MIN_DBM) + rssi;    /* normalize */
    rssi = MIN(rssi, ic->ic_max_rssi);    /* clip to max. 100% */

    rxi.rxi_rssi = rssi;

    phy_info = le16toh(desc->phy_info);
    if (sc->sc_device_family >= IWX_DEVICE_FAMILY_AX210) {
        rate_n_flags = le32toh(desc->v3.rate_n_flags);
        chanidx = desc->v3.channel;
        device_timestamp = desc->v3.gp2_on_air_rise;
        rxi.rxi_tstamp = (uint32_t)le64toh(desc->v3.tsf_on_air_rise);
    } else {
        rate_n_flags = le32toh(desc->v1.rate_n_flags);
        chanidx = desc->v1.channel;
        device_timestamp = desc->v1.gp2_on_air_rise;
        rxi.rxi_tstamp = (uint32_t)le64toh(desc->v1.tsf_on_air_rise);
    }
    rxi.rxi_chan = chanidx;

    if (iwx_rx_reorder(sc, m, chanidx, desc,
                       (phy_info & IWX_RX_MPDU_PHY_SHORT_PREAMBLE),
                       rate_n_flags, device_timestamp, &rxi, ml))
        return;

    iwx_rx_frame(sc, m, chanidx, le16toh(desc->status),
                 (phy_info & IWX_RX_MPDU_PHY_SHORT_PREAMBLE),
                 rate_n_flags, device_timestamp, &rxi, ml);
}

void ItlIwx::
iwx_bz_port_rx_tx_cmd_single(struct iwx_softc *sc, struct iwx_rx_packet *pkt,
                     struct iwx_tx_data *txd)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct _ifnet *ifp = IC2IFP(ic);
    struct iwx_tx_resp *tx_resp = (struct iwx_tx_resp *)pkt->data;
    int status = le16toh(tx_resp->status.status) & IWX_TX_STATUS_MSK;
    int txfail;

    KASSERT(tx_resp->frame_count == 1, "tx_resp->frame_count == 1");

    txfail = (status != IWX_TX_STATUS_SUCCESS);

    if (txfail) {
        DPRINTFN(3, ("%s %d OUTPUT_ERROR type=%d status=%d\n", __FUNCTION__, __LINE__, txd->type, status));
        ifp->netStat->outputErrors++;
        if (txd->type == IEEE80211_FC0_TYPE_MGT)
            iwx_toggle_tx_ant(sc, &sc->sc_mgmt_last_antenna_idx);
    }
}

void ItlIwx::
iwx_bz_port_clear_oactive(struct iwx_softc *sc, struct iwx_tx_ring *ring)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct _ifnet *ifp = &ic->ic_if;

    if (ring->queued < ring->low_mark) {
        sc->qfullmsk &= ~(1U << ring->qid);
        if ((sc->qfullmsk==0) && ifq_is_oactive(&ifp->if_snd)) {
            ifq_clr_oactive(&ifp->if_snd);
            (*ifp->if_start)(ifp);
        }
#ifdef __PRIVATE_SPI__
        ifp->iface->signalOutputThread();
#endif
    }
}

int ItlIwx::
iwx_send_rlc_config(struct iwx_softc *sc, struct iwx_phy_ctxt *ctxt,
                    uint32_t rx_chain_info)
{
    static_assert(sizeof(struct iwx_rlc_config_cmd) == 32,
                  "RLC v2 command layout must be 32 bytes");
    struct iwx_rlc_config_cmd cmd = {};
    cmd.phy_id = htole32(ctxt->id);
    cmd.rlc.rx_chain_info = rx_chain_info;
    int err = iwx_send_cmd_pdu(sc,
        iwx_cmd_id(IWX_RLC_CONFIG_CMD, IWX_DATA_PATH_GROUP, 2),
        0, sizeof(cmd), &cmd);
    sc->sc_pcitag->setProperty("ItlwmRlcErr",
        static_cast<UInt32>(err < 0 ? -err : err), 32);
    sc->sc_pcitag->setProperty("ItlwmRlcPhyId", ctxt->id, 32);
    sc->sc_pcitag->setProperty("ItlwmRlcChainInfo", rx_chain_info, 32);
    return err;
}

int ItlIwx::
iwx_bz_port_phy_ctxt_cmd_v3(struct iwx_softc *sc, struct iwx_phy_ctxt *ctxt,
    uint8_t chains_static, uint8_t chains_dynamic, uint32_t action,
    uint32_t apply_time)
{
    static_assert(sizeof(struct iwx_phy_context_cmd) == 32,
                  "PHY context v3-v5 command layout must be 32 bytes");
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_phy_context_cmd cmd;
    uint8_t active_cnt, idle_cnt;
    uint8_t rlc_ver = iwx_lookup_cmd_ver(sc, IWX_DATA_PATH_GROUP,
                                          IWX_RLC_CONFIG_CMD);
    uint32_t rx_chain_info;
    int err;
    struct ieee80211_channel *chan = ctxt->channel;
    memset(&cmd, 0, sizeof(cmd));
    cmd.id_and_color = htole32(IWX_FW_CMD_ID_AND_COLOR(ctxt->id,
                                                       ctxt->color));
    cmd.action = htole32(action);
    cmd.lmac_id = htole32(iwx_lmac_id(sc, chan));

    cmd.ci.band = IEEE80211_IS_CHAN_2GHZ(chan) ?
    IWX_PHY_BAND_24 : IWX_PHY_BAND_5;
    cmd.ci.channel = htole32(ieee80211_chan2ieee(ic, chan));
    cmd.ci.width = iwx_get_channel_width(ic, chan);
    cmd.ci.ctrl_pos = iwx_get_ctrl_pos(ic, chan);

    XYLog("%s: 2ghz=%d, channel=%d, channel_width=%d pos=%d chains static=0x%x, dynamic=0x%x, "
          "rx_ant=0x%x, tx_ant=0x%x\n",
          __FUNCTION__,
          cmd.ci.band,
          cmd.ci.channel,
          cmd.ci.width,
          cmd.ci.ctrl_pos,
          chains_static,
          chains_dynamic,
          iwx_fw_valid_rx_ant(sc),
          iwx_fw_valid_tx_ant(sc));

    idle_cnt = chains_static;
    active_cnt = chains_dynamic;

    /* In scenarios where we only ever use a single-stream rates,
     * i.e. legacy 11b/g/a associations, single-stream APs or even
     * static SMPS, enable both chains to get diversity, improving
     * the case where we're far enough from the AP that attenuation
     * between the two antennas is sufficiently different to impact
     * performance.
     */
    if (active_cnt == 1 && iwx_num_of_ant(iwx_fw_valid_rx_ant(sc)) != 1) {
        idle_cnt = 2;
        active_cnt = 2;
    }

    rx_chain_info = htole32(iwx_fw_valid_rx_ant(sc) <<
                             IWX_PHY_RX_CHAIN_VALID_POS);
    rx_chain_info |= htole32(idle_cnt << IWX_PHY_RX_CHAIN_CNT_POS);
    rx_chain_info |= htole32(active_cnt << IWX_PHY_RX_CHAIN_MIMO_CNT_POS);
    if (rlc_ver < 2 || rlc_ver == IWX_FW_CMD_VER_UNKNOWN)
        cmd.rxchain_info = rx_chain_info;

    /* Diagnostics: PHY context payload actually sent to the firmware. */
    sc->sc_pcitag->setProperty("ItlwmPhyBand", static_cast<UInt32>(cmd.ci.band), 32);
    sc->sc_pcitag->setProperty("ItlwmPhyChannel",
        static_cast<UInt32>(le32toh(cmd.ci.channel)), 32);
    sc->sc_pcitag->setProperty("ItlwmPhyWidth", static_cast<UInt32>(cmd.ci.width), 32);
    sc->sc_pcitag->setProperty("ItlwmPhyCtrlPos", static_cast<UInt32>(cmd.ci.ctrl_pos), 32);
    sc->sc_pcitag->setProperty("ItlwmPhyLmac",
        static_cast<UInt32>(le32toh(cmd.lmac_id)), 32);
    sc->sc_pcitag->setProperty("ItlwmPhyAction", static_cast<UInt32>(action), 32);
    sc->sc_pcitag->setProperty("ItlwmPhyChanFlags",
        static_cast<UInt32>(chan->ic_flags), 32);
    err = iwx_send_cmd_pdu(sc, IWX_PHY_CONTEXT_CMD, 0, sizeof(cmd), &cmd);
    sc->sc_pcitag->setProperty("ItlwmPhyErr",
        static_cast<UInt32>(err < 0 ? -err : err), 32);
    if (err || action == IWX_FW_CTXT_ACTION_REMOVE)
        return err;
    /* Linux iwlwifi treats RLC v3+ as firmware offload. v2 takes an
     * explicit RLC_CONFIG_CMD after every PHY_CONTEXT_CMD, on MLD firmware
     * too (Linux v6.11 iwl_mvm_phy_send_rlc). Earlier builds skipped it on
     * BZ because it "never acknowledged"; the acknowledgement was in fact
     * dropped by the RX dispatcher, which did not list this command. On
     * MLD firmware a failure is logged rather than failing PHY setup. */
    if (rlc_ver == 2) {
        err = iwx_send_rlc_config(sc, ctxt, rx_chain_info);
        if (err && iwx_use_mld_api(sc)) {
            XYLog("%s: RLC_CONFIG_CMD failed (error %d), continuing\n",
                  DEVNAME(sc), err);
            return 0;
        }
        return err;
    }
    return 0;
}

int ItlIwx::
iwx_bz_port_phy_ctxt_cmd(struct iwx_softc *sc, struct iwx_phy_ctxt *ctxt,
                 uint8_t chains_static, uint8_t chains_dynamic, uint32_t action,
                 uint32_t apply_time)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_phy_context_cmd_v1 cmd;
    uint8_t active_cnt, idle_cnt;
    struct ieee80211_channel *chan = ctxt->channel;

    const uint8_t phy_ver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP,
                                                IWX_PHY_CONTEXT_CMD);
    if (phy_ver >= 3 && phy_ver <= 5)
        return iwx_phy_ctxt_cmd_v3(sc, ctxt, chains_static,
                                   chains_dynamic, action, apply_time);
    if (phy_ver >= 6 && phy_ver != IWX_FW_CMD_VER_UNKNOWN)
        return EINVAL;

    /*
     * Intel increased the size of the fw_channel_info struct and neglected
     * to bump the phy_context_cmd struct, which contains an fw_channel_info
     * member in the middle.
     * To keep things simple we use a separate function to handle the larger
     * variant of the phy context command.
     */
    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_ULTRA_HB_CHANNELS))
        return iwx_phy_ctxt_cmd_uhb(sc, ctxt, chains_static,
                                    chains_dynamic, action, apply_time);

    memset(&cmd, 0, sizeof(cmd));
    cmd.id_and_color = htole32(IWX_FW_CMD_ID_AND_COLOR(ctxt->id,
                                                       ctxt->color));
    cmd.action = htole32(action);
    cmd.apply_time = htole32(apply_time);

    cmd.ci.band = IEEE80211_IS_CHAN_2GHZ(chan) ?
    IWX_PHY_BAND_24 : IWX_PHY_BAND_5;
    cmd.ci.channel = ieee80211_chan2ieee(ic, chan);
    cmd.ci.width = iwx_get_channel_width(ic, chan);
    cmd.ci.ctrl_pos = iwx_get_ctrl_pos(ic, chan);

    idle_cnt = chains_static;
    active_cnt = chains_dynamic;
    cmd.rxchain_info = htole32(iwx_fw_valid_rx_ant(sc) <<
                               IWX_PHY_RX_CHAIN_VALID_POS);
    cmd.rxchain_info |= htole32(idle_cnt << IWX_PHY_RX_CHAIN_CNT_POS);
    cmd.rxchain_info |= htole32(active_cnt <<
                                IWX_PHY_RX_CHAIN_MIMO_CNT_POS);
    cmd.txchain_info = htole32(iwx_fw_valid_tx_ant(sc));

    return iwx_send_cmd_pdu(sc, IWX_PHY_CONTEXT_CMD, 0, sizeof(cmd), &cmd);
}

const struct iwx_rate *ItlIwx::
iwx_bz_port_tx_fill_cmd(struct iwx_softc *sc, struct iwx_node *in,
                struct ieee80211_frame *wh, uint32_t *flags, uint32_t *rate_n_flags)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni = &in->in_ni;
    struct ieee80211_rateset *rs = &ni->ni_rates;
    const struct iwx_rate *rinfo;
    int type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    int min_ridx = iwx_rval2ridx(ieee80211_min_basic_rate(ic));
    int ridx, rate_flags;
    uint8_t cmdver;
    uint8_t hwrate;

    *flags = 0;
    /*
     * MLD (BZ) firmware: follow Linux iwl_mvm_set_tx_params(). Frames to
     * the AP station use the firmware's rate selection (TLC is configured
     * when the station is added) and are marked high priority, as are
     * data frames (EAPOL) before the port is authorized.
     */
    if (iwx_use_mld_api(sc) && (sc->sc_flags & IWX_FLAG_STA_ACTIVE) &&
        !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
        IEEE80211_ADDR_EQ(wh->i_addr1, in->in_macaddr) &&
        (type != IEEE80211_FC0_TYPE_DATA || !ni->ni_port_valid)) {
        *flags |= IWX_TX_FLAGS_HIGH_PRI;
        *rate_n_flags = 0;
        return &iwx_rates[min_ridx];
    }
    if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
        type != IEEE80211_FC0_TYPE_DATA) {
        /* for non-data, use the lowest supported rate */
        ridx = min_ridx;
        *flags |= IWX_TX_FLAGS_CMD_RATE;
    } else if (ic->ic_fixed_mcs != -1) {
        ridx = sc->sc_fixed_ridx;
        *flags |= IWX_TX_FLAGS_CMD_RATE;
    } else if (ic->ic_fixed_rate != -1) {
        ridx = sc->sc_fixed_ridx;
        *flags |= IWX_TX_FLAGS_CMD_RATE;
    } else if (ni->ni_flags & IEEE80211_NODE_HT) {
        ridx = iwx_mcs2ridx[ni->ni_txmcs];
        return &iwx_rates[ridx];
    } else {
        uint8_t rval;
        rval = (rs->rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL);
        ridx = iwx_rval2ridx(rval);
        if (ridx < min_ridx)
            ridx = min_ridx;
        return &iwx_rates[ridx];
    }

    rinfo = &iwx_rates[ridx];

    rate_flags = iwx_get_tx_ant(sc, ni, rinfo, type, wh);

    hwrate = rinfo->plcp;
    cmdver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_TX_CMD);
    if (cmdver > 8 && cmdver != IWX_FW_CMD_VER_UNKNOWN) {
        if (IWX_RIDX_IS_CCK(ridx)) {
            rate_flags |= IWX_RATE_MCS_CCK_MSK;
            hwrate = ridx;
        } else {
            rate_flags |= IWX_RATE_MCS_LEGACY_OFDM_MSK;
            hwrate = iwx_rate2idx(iwx_ridx2rate(rs, ridx) & IEEE80211_RATE_VAL);
        }
    } else if (IWX_RIDX_IS_CCK(ridx))
        rate_flags |= IWX_RATE_MCS_CCK_MSK_V1;
    XYLog("%s hwrate: %d ridx: %d rate: %d minrate: %d\n", __FUNCTION__, hwrate, ridx, iwx_ridx2rate(rs, ridx) & IEEE80211_RATE_VAL, ieee80211_min_basic_rate(ic));
    *rate_n_flags = htole32(rate_flags | hwrate);

    return rinfo;
}

void ItlIwx::
iwx_bz_port_tx_update_byte_tbl(struct iwx_softc *sc, struct iwx_tx_ring *txq, int idx, uint16_t byte_cnt,
                       uint16_t num_tbs)
{
    uint8_t filled_tfd_size, num_fetch_chunks;
    uint16_t len = byte_cnt;
    uint16_t bc_ent;

    filled_tfd_size = offsetof(struct iwx_tfh_tfd, tbs) +
    num_tbs * sizeof(struct iwx_tfh_tb);
    /*
     * filled_tfd_size contains the number of filled bytes in the TFD.
     * Dividing it by 64 will give the number of chunks to fetch
     * to SRAM- 0 for one chunk, 1 for 2 and so on.
     * If, for example, TFD contains only 3 TBs then 32 bytes
     * of the TFD are used, and only one chunk of 64 bytes should
     * be fetched
     */
    num_fetch_chunks = howmany(filled_tfd_size, 64) - 1;

    if (sc->sc_device_family >= IWX_DEVICE_FAMILY_AX210) {
        uint16_t *bc_entries = (uint16_t *)txq->bc_tbl.vaddr;
        if (idx < 0 || (size_t)idx >= txq->bc_tbl.size / sizeof(*bc_entries)) {
            XYLog("%s: TX byte count index %d exceeds %zu entries\n",
                  DEVNAME(sc), idx, txq->bc_tbl.size / sizeof(*bc_entries));
            return;
        }

        /* Starting from AX210, the HW expects bytes */
        bc_ent = htole16(len | (num_fetch_chunks << 14));
        bc_entries[idx] = bc_ent;
    } else {
        struct iwx_agn_scd_bc_tbl *scd_bc_tbl = (struct iwx_agn_scd_bc_tbl *)txq->bc_tbl.vaddr;

        /* Before AX210, the HW expects DW */
        len = howmany(len, 4);
        bc_ent = htole16(len | (num_fetch_chunks << 12));
        scd_bc_tbl->tfd_offset[idx] = bc_ent;
    }
}

int ItlIwx::
iwx_bz_port_flush_sta_tids(struct iwx_softc *sc, int sta_id, uint16_t tids)
{
    struct iwx_rx_packet *pkt;
    struct iwx_tx_path_flush_cmd_rsp *resp;
    struct iwx_tx_path_flush_cmd flush_cmd = {
        .sta_id = htole32(sta_id),
        .tid_mask = htole16(tids),
    };
    struct iwx_host_cmd hcmd = {
        .id = IWX_TXPATH_FLUSH,
        .len = { sizeof(flush_cmd), },
        .data = { &flush_cmd, },
        .flags = IWX_CMD_WANT_RESP,
        .resp_pkt_len = sizeof(*pkt) + sizeof(*resp),
    };
    int err, resp_len, i, num_flushed_queues;

    err = iwx_send_cmd(sc, &hcmd);
    if (err)
        return err;

    pkt = hcmd.resp_pkt;
    if (!pkt || (pkt->hdr.group_id & IWX_CMD_FAILED_MSK)) {
        err = EIO;
        goto out;
    }

    resp_len = iwx_rx_packet_payload_len(pkt);
    /* Some firmware versions don't provide a response. */
    if (resp_len == 0)
        goto out;
    else if (resp_len != sizeof(*resp)) {
        err = EIO;
        goto out;
    }

    resp = (struct iwx_tx_path_flush_cmd_rsp *)pkt->data;

    if (le16toh(resp->sta_id) != sta_id) {
        err = EIO;
        goto out;
    }

    num_flushed_queues = le16toh(resp->num_flushed_queues);
    if (num_flushed_queues > IWX_TX_FLUSH_QUEUE_RSP) {
        err = EIO;
        goto out;
    }

    for (i = 0; i < num_flushed_queues; i++) {
        struct iwx_flush_queue_info *queue_info = &resp->queues[i];
        uint16_t tid = le16toh(queue_info->tid);
        uint16_t read_after = le16toh(queue_info->read_after_flush);
        uint16_t qid = le16toh(queue_info->queue_num);
        struct iwx_tx_ring *txq;

        if (qid >= nitems(sc->txq))
            continue;

        if (sc->sc_tid_data[tid].qid != qid)
            continue;
        txq = &sc->txq[qid];

        iwx_ampdu_txq_advance(sc, txq, IWX_AGG_SSN_TO_TXQ_IDX(read_after, txq->ring_count));
    }
out:
    iwx_free_resp(sc, &hcmd);
    return err;
}

int ItlIwx::
iwx_bz_port_flush_sta(struct iwx_softc *sc, struct iwx_node *in)
{
    int err, generation = sc->sc_generation;

    splassert(IPL_NET);

    sc->sc_flags |= IWX_FLAG_TXFLUSH;

    err = iwx_drain_sta(sc, in, 1);

    if (err)
        goto done;

    err = iwx_flush_sta_tids(sc, IWX_STATION_ID, 0xffff);
    if (err) {
        XYLog("%s: could not flush Tx path (error %d)\n",
               __FUNCTION__, err);
        goto done;
    }

    err = iwx_drain_sta(sc, in, 0);
    if (err)
        goto done;

done:
    if (generation == sc->sc_generation)
        sc->sc_flags &= ~IWX_FLAG_TXFLUSH;
    return err;
}

int ItlIwx::
iwx_bz_port_add_sta_cmd(struct iwx_softc *sc, struct iwx_node *in, int update)
{
    struct iwx_add_sta_cmd add_sta_cmd;
    int err;
    uint32_t status, agg_size = 0;
    uint32_t max_aggsize = (IWX_STA_FLG_MAX_AGG_SIZE_4M >> IWX_STA_FLG_MAX_AGG_SIZE_SHIFT);
    struct ieee80211com *ic = &sc->sc_ic;

    if (!update && (sc->sc_flags & IWX_FLAG_STA_ACTIVE)) {
        XYLog("STA already added\n");
        return 0;
    }

    memset(&add_sta_cmd, 0, sizeof(add_sta_cmd));

    if (ic->ic_opmode == IEEE80211_M_MONITOR) {
        add_sta_cmd.sta_id = IWX_MONITOR_STA_ID;
        add_sta_cmd.station_type = IWX_STA_GENERAL_PURPOSE;
    } else {
        add_sta_cmd.sta_id = IWX_STATION_ID;
        add_sta_cmd.station_type = IWX_STA_LINK;
    }
    add_sta_cmd.mac_id_n_color
    = htole32(IWX_FW_CMD_ID_AND_COLOR(in->in_id, in->in_color));
    if (!update) {
        if (ic->ic_opmode == IEEE80211_M_MONITOR)
            IEEE80211_ADDR_COPY(&add_sta_cmd.addr,
                                etheranyaddr);
        else
            IEEE80211_ADDR_COPY(&add_sta_cmd.addr,
                                in->in_macaddr);
    }
    add_sta_cmd.add_modify = update ? 1 : 0;
    add_sta_cmd.station_flags_msk
    |= htole32(IWX_STA_FLG_FAT_EN_MSK | IWX_STA_FLG_MIMO_EN_MSK);
    add_sta_cmd.tid_disable_tx = htole16(0xffff);

    if ((in->in_ni.ni_flags & IEEE80211_NODE_HT) ||
        (in->in_ni.ni_flags & IEEE80211_NODE_VHT) ||
        (in->in_ni.ni_flags & IEEE80211_NODE_HE)) {
        add_sta_cmd.station_flags_msk
        |= htole32(IWX_STA_FLG_MAX_AGG_SIZE_MSK |
                   IWX_STA_FLG_AGG_MPDU_DENS_MSK);

        if (ic->ic_state >= IEEE80211_S_ASSOC) {
            switch (in->in_ni.ni_chw) {
                case IEEE80211_CHAN_WIDTH_80P80:
                case IEEE80211_CHAN_WIDTH_160:
                    add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_FAT_EN_160MHZ);
                case IEEE80211_CHAN_WIDTH_80:
                    add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_FAT_EN_80MHZ);
                case IEEE80211_CHAN_WIDTH_40:
                    add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_FAT_EN_40MHZ);
                case IEEE80211_CHAN_WIDTH_20:
                default:
                    add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_FAT_EN_20MHZ);
                    break;
            }
        }

        if (iwx_mimo_enabled(sc) && ic->ic_bss->ni_rx_nss > 1)
            add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_MIMO_EN_MIMO2);
        else
            add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_MIMO_EN_SISO);

        if (in->in_ni.ni_flags & IEEE80211_NODE_HE) {
            agg_size = in->in_ni.ni_vhtcaps &
            IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
            agg_size >>=
            IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT;
            agg_size += u8_get_bits(in->in_ni.ni_he_cap_elem.mac_cap_info[3],
                                    IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_MASK);
            add_sta_cmd.station_flags |=
            htole32(min(max_aggsize, agg_size) << IWX_STA_FLG_MAX_AGG_SIZE_SHIFT);
        } else if (in->in_ni.ni_flags & IEEE80211_NODE_VHT) {
            agg_size = in->in_ni.ni_vhtcaps &
            IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
            agg_size >>=
            IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT;
            add_sta_cmd.station_flags |=
            htole32(min(max_aggsize, agg_size) << IWX_STA_FLG_MAX_AGG_SIZE_SHIFT);
        } else if (in->in_ni.ni_flags & IEEE80211_NODE_HT)
            add_sta_cmd.station_flags |= htole32(IWX_STA_FLG_MAX_AGG_SIZE_64K);

        switch (ic->ic_ampdu_params & IEEE80211_AMPDU_PARAM_SS) {
            case IEEE80211_AMPDU_PARAM_SS_2:
                add_sta_cmd.station_flags
                |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_2US);
                break;
            case IEEE80211_AMPDU_PARAM_SS_4:
                add_sta_cmd.station_flags
                |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_4US);
                break;
            case IEEE80211_AMPDU_PARAM_SS_8:
                add_sta_cmd.station_flags
                |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_8US);
                break;
            case IEEE80211_AMPDU_PARAM_SS_16:
                add_sta_cmd.station_flags
                |= htole32(IWX_STA_FLG_AGG_MPDU_DENS_16US);
                break;
            default:
                break;
        }
    }

    status = IWX_ADD_STA_SUCCESS;
    err = iwx_send_cmd_pdu_status(sc, IWX_ADD_STA, sizeof(add_sta_cmd),
                                  &add_sta_cmd, &status);
    if (!err && (status & IWX_ADD_STA_STATUS_MASK) != IWX_ADD_STA_SUCCESS)
        err = EIO;
    else
        sc->sc_flags |= IWX_FLAG_STA_ACTIVE;

    return err;
}

int ItlIwx::
iwx_bz_port_add_aux_sta(struct iwx_softc *sc)
{

    XYLog("%s\n", __FUNCTION__);
    struct iwx_add_sta_cmd cmd;
    int err, qid = IWX_DQA_AUX_QUEUE;
    uint32_t status;

    /*
     * Add auxiliary station for scanning.
     * Newer versions of this command implies that the fw uses
     * internal aux station for all aux activities that don't
     * requires a dedicated data queue.
     * In old version the aux station uses mac id like other
     * station and not lmac id
     */
    if (iwx_has_new_station_api(sc))
        return 0;

    memset(&cmd, 0, sizeof(cmd));
    cmd.sta_id = IWX_AUX_STA_ID;
    cmd.station_type = IWX_STA_AUX_ACTIVITY;
    cmd.mac_id_n_color =
    htole32(IWX_FW_CMD_ID_AND_COLOR(IWX_MAC_INDEX_AUX, 0));
    cmd.tid_disable_tx = htole16(0xffff);

    status = IWX_ADD_STA_SUCCESS;
    err = iwx_send_cmd_pdu_status(sc, IWX_ADD_STA, sizeof(cmd), &cmd,
                                  &status);
    if (!err && (status & IWX_ADD_STA_STATUS_MASK) != IWX_ADD_STA_SUCCESS)
        return EIO;

    return iwx_enable_txq(sc, IWX_AUX_STA_ID, qid, IWX_MGMT_TID,
                          sc->txq[qid].ring_count);
    return 0;
}

int ItlIwx::
iwx_bz_port_rm_sta_cmd(struct iwx_softc *sc, struct iwx_node *in)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_rm_sta_cmd rm_sta_cmd;
    int err;

    if ((sc->sc_flags & IWX_FLAG_STA_ACTIVE) == 0) {
        XYLog("sta already removed\n");
        return 0;
    }

    memset(&rm_sta_cmd, 0, sizeof(rm_sta_cmd));
    if (ic->ic_opmode == IEEE80211_M_MONITOR)
        rm_sta_cmd.sta_id = IWX_MONITOR_STA_ID;
    else
        rm_sta_cmd.sta_id = IWX_STATION_ID;

    err = iwx_send_cmd_pdu(sc, IWX_REMOVE_STA, 0, sizeof(rm_sta_cmd),
                           &rm_sta_cmd);

    if (!err)
        sc->sc_flags &= ~IWX_FLAG_STA_ACTIVE;

    return err;
}

uint8_t ItlIwx::
iwx_bz_port_umac_scan_fill_channels(struct iwx_softc *sc,
                            struct iwx_scan_channel_cfg_umac *chan, int n_ssids, int bgscan)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_channel *c;
    uint8_t nchan;
    uint8_t scan_ver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_SCAN_REQ_UMAC);
    unsigned int max_channels = sc->sc_capa_n_scan_channels;

    /* Fixed-size UMAC requests from v12 onward have only 67 slots. */
    if (scan_ver >= 12 && scan_ver != IWX_FW_CMD_VER_UNKNOWN &&
        max_channels > IWX_SCAN_MAX_NUM_CHANS_V3)
        max_channels = IWX_SCAN_MAX_NUM_CHANS_V3;

    for (nchan = 0, c = &ic->ic_channels[1];
         c <= &ic->ic_channels[IEEE80211_CHAN_MAX] &&
         nchan < max_channels;
         c++) {
        uint8_t channel_num;

        if (c->ic_flags == 0)
            continue;
        /* The current net80211 port has no 6 GHz scan representation. Do
         * not mislabel a future/unknown band as 5 GHz in a v17 request. */
        if (scan_ver == 17 && !IEEE80211_IS_CHAN_2GHZ(c) &&
            !IEEE80211_IS_CHAN_5GHZ(c))
            continue;

        channel_num = ieee80211_mhz2ieee(c->ic_freq, 0);
        if ((scan_ver >= 14 && scan_ver != IWX_FW_CMD_VER_UNKNOWN) ||
            isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER)) {
            chan->v2.channel_num = channel_num;
            const uint8_t band = IEEE80211_IS_CHAN_2GHZ(c) ?
                IWX_PHY_BAND_24 : IWX_PHY_BAND_5;
            if (scan_ver == 17)
                chan->flags = htole32((uint32_t)band << IWX_CHAN_CFG_FLAGS_BAND_POS);
            else
                chan->v2.band = band;
            chan->v2.iter_count = 1;
            chan->v2.iter_interval = 0;
        } else {
            chan->v1.channel_num = channel_num;
            chan->v1.iter_count = 1;
            chan->v1.iter_interval = htole16(0);
        }

        if (n_ssids != 0 && !bgscan)
            chan->flags |= htole32(1 << 0); /* select SSID 0 */
        chan++;
        nchan++;
    }

    return nchan;
}

int ItlIwx::
iwx_bz_port_config_umac_scan(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct iwx_scan_config *scan_config;
    int err;
    size_t cmd_size;
    struct iwx_host_cmd hcmd = {
        .id = iwx_cmd_id(IWX_SCAN_CFG_CMD, IWX_LONG_GROUP, 0),
        .flags = 0,
    };
    uint8_t cmdver;

    if (!isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_REDUCED_SCAN_CONFIG))
        return iwx_config_legacy_umac_scan(sc);

    cmd_size = sizeof(*scan_config);

    scan_config = (struct iwx_scan_config *)malloc(cmd_size, 0, M_ZERO);
    if (scan_config == NULL)
        return ENOMEM;

    scan_config->tx_chains = htole32(iwx_fw_valid_tx_ant(sc));
    scan_config->rx_chains = htole32(iwx_fw_valid_rx_ant(sc));

    cmdver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_SCAN_CFG_CMD);
    if (!iwx_has_new_station_api(sc))
        scan_config->bcast_sta_id = IWX_AUX_STA_ID;
    else if (cmdver == IWX_FW_CMD_VER_UNKNOWN || cmdver < 5)
        scan_config->bcast_sta_id = 0xFF;

    hcmd.data[0] = scan_config;
    hcmd.len[0] = cmd_size;

    err = iwx_send_cmd(sc, &hcmd);
    ::free(scan_config);
    return err;
}

int ItlIwx::
iwx_bz_port_umac_scan_size(struct iwx_softc *sc)
{
    int base_size = IWX_SCAN_REQ_UMAC_SIZE_V1;
    int tail_size;
    uint8_t scan_ver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_SCAN_REQ_UMAC);

    if (scan_ver == 12)
        return sizeof(iwx_scan_req_umac_v12);
    else if (scan_ver >= 14 && scan_ver <= 17)
        return sizeof(iwx_scan_req_umac_v14);

    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2))
        base_size = IWX_SCAN_REQ_UMAC_SIZE_V8;
    else if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL))
        base_size = IWX_SCAN_REQ_UMAC_SIZE_V7;
    else
        base_size = IWX_SCAN_REQ_UMAC_SIZE_V6;

    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER))
        tail_size = sizeof(struct iwx_scan_req_umac_tail_v2);
    else
        tail_size = sizeof(struct iwx_scan_req_umac_tail_v1);

    return base_size + sizeof(struct iwx_scan_channel_cfg_umac) *
    sc->sc_capa_n_scan_channels + tail_size;
}

int ItlIwx::
iwx_bz_port_umac_scan(struct iwx_softc *sc, int bgscan)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    int scanSSIDLength = ic->ic_des_esslen;
    const uint8_t *scanSSID = ic->ic_des_essid;
    bool scanActive = scanSSIDLength != 0 && !bgscan;
    struct iwx_host_cmd hcmd = {
        .id = iwx_cmd_id(IWX_SCAN_REQ_UMAC, IWX_LONG_GROUP, 0),
        .len = { 0, },
        .data = { NULL, },
        .flags = 0,
    };
    struct iwx_scan_req_umac *req;
    void *cmd_data, *tail_data;
    struct iwx_scan_req_umac_tail_v2 *tail;
    struct iwx_scan_req_umac_tail_v1 *tailv1;
    struct iwx_scan_umac_chan_param *chanparam;
    size_t req_len;
    int err, async = bgscan;
    const uint32_t timeout = bgscan ?  htole32(120) : htole32(0);
    const uint32_t maxOut = timeout;
    uint8_t scan_ver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_SCAN_REQ_UMAC);
    /* Diagnostics: which SCAN_REQ_UMAC version the firmware advertised. */
    sc->sc_pcitag->setProperty("ItlwmScanVer", static_cast<UInt32>(scan_ver), 32);

    if (scan_ver == 12)
        return iwx_umac_scan_v12(sc, bgscan);
    else if (scan_ver == 14 || scan_ver == 17)
        return iwx_umac_scan_v14_and_above(sc, bgscan, scan_ver);
    else if (scan_ver >= 15 && scan_ver != IWX_FW_CMD_VER_UNKNOWN) {
        XYLog("%s: unsupported scan command version %u\n", DEVNAME(sc), scan_ver);
        return EINVAL;
    }

    req_len = iwx_umac_scan_size(sc);
    if ((req_len < IWX_SCAN_REQ_UMAC_SIZE_V1 +
         sizeof(struct iwx_scan_req_umac_tail_v1)) ||
        req_len > IWX_MAX_CMD_PAYLOAD_SIZE)
        return ERANGE;
    req = (struct iwx_scan_req_umac *)malloc(req_len, 0,
                                             (async ? M_NOWAIT : M_WAITOK) | M_ZERO);
    if (req == NULL)
        return ENOMEM;

    hcmd.len[0] = (uint16_t)req_len;
    hcmd.data[0] = (void *)req;
    hcmd.flags |= async ? IWX_CMD_ASYNC : 0;

    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL)) {
        req->v7.adwell_default_n_aps_social =
        IWX_SCAN_ADWELL_DEFAULT_N_APS_SOCIAL;
        req->v7.adwell_default_n_aps =
        IWX_SCAN_ADWELL_DEFAULT_LB_N_APS;

        if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADWELL_HB_DEF_N_AP))
            req->v9.adwell_default_hb_n_aps = IWX_SCAN_ADWELL_DEFAULT_HB_N_APS;

        if (scanActive && scanSSIDLength != 0)
            req->v7.adwell_max_budget =
            htole16(IWX_SCAN_ADWELL_MAX_BUDGET_DIRECTED_SCAN);
        else
            req->v7.adwell_max_budget =
            htole16(IWX_SCAN_ADWELL_MAX_BUDGET_FULL_SCAN);

        req->v7.scan_priority = htole32(IWX_SCAN_PRIORITY_EXT_6);
        req->v7.max_out_time[IWX_SCAN_LB_LMAC_IDX] = maxOut;
        req->v7.suspend_time[IWX_SCAN_LB_LMAC_IDX] = timeout;

        if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CDB_SUPPORT)) {
            req->v7.max_out_time[IWX_SCAN_HB_LMAC_IDX] =
                maxOut;
            req->v7.suspend_time[IWX_SCAN_HB_LMAC_IDX] =
                timeout;
        }

        if (isset(sc->sc_ucode_api,
                  IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2)) {
            req->v8.active_dwell[IWX_SCAN_LB_LMAC_IDX] = IWL_SCAN_DWELL_ACTIVE;
            req->v8.passive_dwell[IWX_SCAN_LB_LMAC_IDX] = IWL_SCAN_DWELL_PASSIVE;
            if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CDB_SUPPORT)) {
                req->v8.active_dwell[IWX_SCAN_HB_LMAC_IDX] =
                    IWL_SCAN_DWELL_ACTIVE;
                req->v8.passive_dwell[IWX_SCAN_HB_LMAC_IDX] =
                    IWL_SCAN_DWELL_PASSIVE;
            }
        } else {
            req->v7.active_dwell = IWL_SCAN_DWELL_ACTIVE;
            req->v7.passive_dwell = IWL_SCAN_DWELL_PASSIVE;
            req->v7.fragmented_dwell = IWL_SCAN_DWELL_FRAGMENTED;
        }
    } else {
        /* These timings correspond to iwlwifi's UNASSOC scan. */
        req->v1.active_dwell = IWL_SCAN_DWELL_ACTIVE;
        req->v1.passive_dwell = IWL_SCAN_DWELL_PASSIVE;
        req->v1.fragmented_dwell = IWL_SCAN_DWELL_FRAGMENTED;
        req->v1.extended_dwell = IWL_SCAN_DWELL_EXTENDED;

        if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CDB_SUPPORT)) {
            req->v6.max_out_time[IWX_SCAN_HB_LMAC_IDX] =
                maxOut;
            req->v6.suspend_time[IWX_SCAN_HB_LMAC_IDX] =
                timeout;
        }

        req->v6.scan_priority =
            htole32(IWX_SCAN_PRIORITY_EXT_6);
        req->v6.max_out_time[IWX_SCAN_LB_LMAC_IDX] =
            maxOut;
        req->v6.suspend_time[IWX_SCAN_LB_LMAC_IDX] =
            timeout;
    }

    req->ooc_priority = htole32(IWX_SCAN_PRIORITY_EXT_6);

    cmd_data = iwx_get_scan_req_umac_data(sc, req);
    chanparam = iwx_get_scan_req_umac_chan_param(sc, req);
    chanparam->count = iwx_umac_scan_fill_channels(sc,
                                                   (struct iwx_scan_channel_cfg_umac *)cmd_data,
                                                   scanActive, bgscan);
    chanparam->flags = 0;

    tail_data = (uint8_t*)cmd_data + sizeof(struct iwx_scan_channel_cfg_umac) *
    sc->sc_capa_n_scan_channels;
    tail = (struct iwx_scan_req_umac_tail_v2 *)tail_data;
    /* tail v1 layout differs in preq and direct_scan member fields. */
    tailv1 = (struct iwx_scan_req_umac_tail_v1 *)tail_data;

    req->general_flags = htole32(IWX_UMAC_SCAN_GEN_FLAGS_PASS_ALL |
                                 IWX_UMAC_SCAN_GEN_FLAGS_ITER_COMPLETE);
    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL_V2)) {
        req->v8.general_flags2 =
        IWX_UMAC_SCAN_GEN_FLAGS2_ALLOW_CHNL_REORDER;
    }

    /* Check if we're doing an active directed scan. */
    if (scanActive) {
        if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER)) {
            tail->direct_scan[0].id = IEEE80211_ELEMID_SSID;
            tail->direct_scan[0].len = scanSSIDLength;
            memcpy(tail->direct_scan[0].ssid, scanSSID,
                   scanSSIDLength);
        } else {
            tailv1->direct_scan[0].id = IEEE80211_ELEMID_SSID;
            tailv1->direct_scan[0].len = scanSSIDLength;
            memcpy(tailv1->direct_scan[0].ssid, scanSSID,
                   scanSSIDLength);
        }
        if (scanSSIDLength != 0)
            req->general_flags |= htole32(IWX_UMAC_SCAN_GEN_FLAGS_PRE_CONNECT);
    } else
        req->general_flags |= htole32(IWX_UMAC_SCAN_GEN_FLAGS_PASSIVE);

    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT) &&
        isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_WFA_TPC_REP_IE_SUPPORT))
        req->general_flags |=
        htole32(IWX_UMAC_SCAN_GEN_FLAGS_RRM_ENABLED);

    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_ADAPTIVE_DWELL)) {
        req->general_flags |=
        htole32(IWX_UMAC_SCAN_GEN_FLAGS_ADAPTIVE_DWELL);
    } else {
        req->general_flags |=
        htole32(IWX_UMAC_SCAN_GEN_FLAGS_EXTENDED_DWELL);
    }


    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_SCAN_EXT_CHAN_VER))
        err = iwx_fill_probe_req(sc, &tail->preq);
    else
        err = iwx_fill_probe_req_v1(sc, &tailv1->preq);
    if (err) {
        ::free(req);
        return err;
    }

    /* Specify the scan plan: We'll do one iteration. */
    tail->schedule[0].interval = 0;
    tail->schedule[0].iter_count = 1;

    err = iwx_send_cmd(sc, &hcmd);
    ::free(req);
    return err;
}

int ItlIwx::
iwx_bz_port_umac_scan_v12(struct iwx_softc *sc, int bgscan)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    int scanSSIDLength = ic->ic_des_esslen;
    const uint8_t *scanSSID = ic->ic_des_essid;
    bool scanActive = scanSSIDLength != 0 && !bgscan;
    int err = 0, async = bgscan;
    struct iwx_scan_req_umac_v12 *req;
    size_t req_len;
    uint16_t gen_flags = 0;
    const uint32_t timeout = bgscan ?  htole32(120) : htole32(0);
    const uint32_t maxOut = timeout;
    struct iwx_scan_general_params_v10 *general_params;
    struct iwx_scan_channel_params_v4 *cp;
    struct iwx_host_cmd hcmd = {
        .id = iwx_cmd_id(IWX_SCAN_REQ_UMAC, IWX_LONG_GROUP, 0),
        .len = { 0, },
        .data = { NULL, },
        .flags = 0,
    };

    req_len = iwx_umac_scan_size(sc);
    req = (struct iwx_scan_req_umac_v12 *)malloc(req_len, 0,
                                             (async ? M_NOWAIT : M_WAITOK) | M_ZERO);
    if (req == NULL)
        return ENOMEM;

    hcmd.len[0] = (uint16_t)req_len;
    hcmd.data[0] = (void *)req;
    hcmd.flags |= async ? IWX_CMD_ASYNC : 0;

    general_params = &req->scan_params.general_params;

    req->ooc_priority = htole32(IWX_SCAN_PRIORITY_EXT_6);

    if (!scanActive)
        gen_flags |= IWX_UMAC_SCAN_GEN_FLAGS_V2_FORCE_PASSIVE;

    gen_flags |= IWX_UMAC_SCAN_GEN_FLAGS_V2_PASS_ALL |
    IWX_UMAC_SCAN_GEN_FLAGS_V2_ADAPTIVE_DWELL;

    req->scan_params.general_params.flags = gen_flags;

    general_params->adwell_default_social_chn =
        IWX_SCAN_ADWELL_DEFAULT_N_APS_SOCIAL;
    general_params->adwell_default_2g = IWX_SCAN_ADWELL_DEFAULT_LB_N_APS;
    general_params->adwell_default_5g = IWX_SCAN_ADWELL_DEFAULT_HB_N_APS;

    if (scanActive && scanSSIDLength != 0)
        general_params->adwell_max_budget =
            cpu_to_le16(IWX_SCAN_ADWELL_MAX_BUDGET_DIRECTED_SCAN);
    else
        general_params->adwell_max_budget =
            cpu_to_le16(IWX_SCAN_ADWELL_MAX_BUDGET_FULL_SCAN);

    general_params->scan_priority = htole32(IWX_SCAN_PRIORITY_EXT_6);
    general_params->max_out_of_time[IWX_SCAN_LB_LMAC_IDX] =
        maxOut;
    general_params->suspend_time[IWX_SCAN_LB_LMAC_IDX] =
        timeout;

    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CDB_SUPPORT)) {
        general_params->max_out_of_time[IWX_SCAN_HB_LMAC_IDX] = maxOut;
        general_params->suspend_time[IWX_SCAN_HB_LMAC_IDX] = timeout;
    }

    general_params->active_dwell[IWX_SCAN_LB_LMAC_IDX] = IWL_SCAN_DWELL_ACTIVE;
    general_params->passive_dwell[IWX_SCAN_LB_LMAC_IDX] = IWL_SCAN_DWELL_PASSIVE;
    general_params->active_dwell[IWX_SCAN_HB_LMAC_IDX] = IWL_SCAN_DWELL_ACTIVE;
    general_params->passive_dwell[IWX_SCAN_HB_LMAC_IDX] = IWL_SCAN_DWELL_PASSIVE;

    /* Specify the scan plan: We'll do one iteration. */
    req->scan_params.periodic_params.schedule[0].interval = 0;
    req->scan_params.periodic_params.schedule[0].iter_count = 1;

    err = iwx_fill_probe_req(sc, &req->scan_params.probe_params.preq);
    if (err)
        return err;

    if (scanActive) {
        req->scan_params.probe_params.ssid_num = 1;
        req->scan_params.probe_params.direct_scan[0].id = IEEE80211_ELEMID_SSID;
        req->scan_params.probe_params.direct_scan[0].len = scanSSIDLength;
        memcpy(req->scan_params.probe_params.direct_scan[0].ssid, scanSSID,
               scanSSIDLength);
    } else
        req->scan_params.probe_params.ssid_num = 0;

    cp = &req->scan_params.channel_params;

    cp->flags = IWX_SCAN_CHANNEL_FLAG_ENABLE_CHAN_ORDER;
    cp->count = iwx_umac_scan_fill_channels(sc,
                                            (struct iwx_scan_channel_cfg_umac *)cp->channel_config,
                                            scanActive, bgscan);
    cp->num_of_aps_override = IWX_SCAN_ADWELL_N_APS_GO_FRIENDLY;

    err = iwx_send_cmd(sc, &hcmd);
    ::free(req);
    return err;
}

int ItlIwx::
iwx_umac_scan_v14_and_above(struct iwx_softc *sc, int bgscan, uint8_t scan_ver)
{
    static_assert(sizeof(struct iwx_scan_req_umac_v14) ==
                  sizeof(struct iwx_scan_req_umac_v17),
                  "scan v14/v17 packed request sizes differ");
    static_assert(sizeof(struct iwx_scan_channel_cfg_umac) == 8,
                  "scan channel configuration must be eight bytes");
    static_assert(__builtin_offsetof(struct iwx_scan_channel_cfg_umac, v2.band) == 5,
                  "scan v17 PSD byte must follow channel number");
    static_assert(sizeof(struct iwx_scan_general_params_v10) == 36,
                  "scan general parameters v10/v11 must be 36 bytes");
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    int scanSSIDLength = ic->ic_des_esslen;
    const uint8_t *scanSSID = ic->ic_des_essid;
    bool scanActive = scanSSIDLength != 0 && !bgscan;
    int err = 0, async = bgscan;
    struct iwx_scan_req_umac_v14 *req;
    size_t req_len;
    uint16_t gen_flags = 0;
    struct iwx_scan_general_params_v10 *general_params;
    struct iwx_scan_channel_params_v6 *cp;
    uint32_t suspendTU = bgscan ? 120 : 0;
    const uint32_t timeout = htole32(suspendTU);
    const uint32_t maxOut = timeout;
    struct iwx_host_cmd hcmd = {
        .id = iwx_cmd_id(IWX_SCAN_REQ_UMAC, IWX_LONG_GROUP, 0),
        .len = { 0, },
        .data = { NULL, },
        .flags = 0,
    };

    req_len = iwx_umac_scan_size(sc);
    req = (struct iwx_scan_req_umac_v14 *)malloc(req_len, 0,
                                             (async ? M_NOWAIT : M_WAITOK) | M_ZERO);
    if (req == NULL)
        return ENOMEM;

    hcmd.len[0] = (uint16_t)req_len;
    hcmd.data[0] = (void *)req;
    hcmd.flags |= async ? IWX_CMD_ASYNC : 0;

    general_params = &req->scan_params.general_params;

    req->ooc_priority = htole32(IWX_SCAN_PRIORITY_EXT_6);

    if (!scanActive)
        gen_flags |= IWX_UMAC_SCAN_GEN_FLAGS_V2_FORCE_PASSIVE;

    gen_flags |= IWX_UMAC_SCAN_GEN_FLAGS_V2_PASS_ALL |
    IWX_UMAC_SCAN_GEN_FLAGS_V2_ADAPTIVE_DWELL;

    req->scan_params.general_params.flags = htole16(gen_flags);
    /* v15+ names this byte flags2. The current scan uses none of its
     * optional P2P GO, 6 GHz antenna, or channel-survey features. */
    if (scan_ver >= 15)
        general_params->reserved1 = 0;

    general_params->adwell_default_social_chn =
        IWX_SCAN_ADWELL_DEFAULT_N_APS_SOCIAL;
    general_params->adwell_default_2g = IWX_SCAN_ADWELL_DEFAULT_LB_N_APS;
    general_params->adwell_default_5g = IWX_SCAN_ADWELL_DEFAULT_HB_N_APS;
    if (scanActive && scanSSIDLength != 0)
        general_params->adwell_max_budget =
            cpu_to_le16(IWX_SCAN_ADWELL_MAX_BUDGET_DIRECTED_SCAN);
    else
        general_params->adwell_max_budget =
            cpu_to_le16(IWX_SCAN_ADWELL_MAX_BUDGET_FULL_SCAN);

    general_params->scan_priority = htole32(IWX_SCAN_PRIORITY_EXT_6);
    general_params->max_out_of_time[IWX_SCAN_LB_LMAC_IDX] =
        maxOut;
    general_params->suspend_time[IWX_SCAN_LB_LMAC_IDX] =
        timeout;

    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CDB_SUPPORT)) {
        general_params->max_out_of_time[IWX_SCAN_HB_LMAC_IDX] = maxOut;
        general_params->suspend_time[IWX_SCAN_HB_LMAC_IDX] = timeout;
    }

    general_params->active_dwell[IWX_SCAN_LB_LMAC_IDX] = IWL_SCAN_DWELL_ACTIVE;
    general_params->passive_dwell[IWX_SCAN_LB_LMAC_IDX] = IWL_SCAN_DWELL_PASSIVE;
    general_params->active_dwell[IWX_SCAN_HB_LMAC_IDX] = IWL_SCAN_DWELL_ACTIVE;
    general_params->passive_dwell[IWX_SCAN_HB_LMAC_IDX] = IWL_SCAN_DWELL_PASSIVE;

    /* Specify the scan plan: We'll do one iteration. */
    req->scan_params.periodic_params.schedule[0].interval = 0;
    req->scan_params.periodic_params.schedule[0].iter_count = 1;

    err = iwx_fill_probe_req(sc, &req->scan_params.probe_params.preq);
    if (err) {
        ::free(req);
        return err;
    }
    if (scanActive) {
        req->scan_params.probe_params.direct_scan[0].id = IEEE80211_ELEMID_SSID;
        req->scan_params.probe_params.direct_scan[0].len = scanSSIDLength;
        memcpy(req->scan_params.probe_params.direct_scan[0].ssid, scanSSID,
               scanSSIDLength);
    }

    cp = &req->scan_params.channel_params;

    cp->flags = IWX_SCAN_CHANNEL_FLAG_ENABLE_CHAN_ORDER;
    cp->count = iwx_umac_scan_fill_channels(sc,
                                            (struct iwx_scan_channel_cfg_umac *)cp->channel_config,
                                            scanActive, bgscan);
    sc->sc_pcitag->setProperty("ItlwmScanChannels", static_cast<UInt32>(cp->count), 32);
    sc->sc_pcitag->setProperty("ItlwmScanBuilds", ++sc->sc_scan_builds, 32);
    sc->sc_pcitag->setProperty("ItlwmScanActiveFlag",
        static_cast<UInt32>(scanActive), 32);
    sc->sc_pcitag->setProperty("ItlwmScanSsidLen",
        static_cast<UInt32>(scanSSIDLength), 32);
    cp->n_aps_override[0] = IWX_SCAN_ADWELL_N_APS_GO_FRIENDLY;
    cp->n_aps_override[1] = IWX_SCAN_ADWELL_N_APS_SOCIAL_CHS;

    err = iwx_send_cmd(sc, &hcmd);
    ::free(req);
    return err;
}

int ItlIwx::
iwx_bz_port_scan(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    struct _ifnet *ifp = IC2IFP(ic);
    int err;

    /* Diagnostics: net80211 reached the driver scan path. */
    sc->sc_scan_start_ns = iwx_boot_time_ns();
    sc->sc_pcitag->setProperty("ItlwmScanCalled", static_cast<UInt32>(1), 32);
    sc->sc_pcitag->setProperty("ItlwmScanIffFlags",
        static_cast<UInt32>(ifp->if_flags), 32);

    if (sc->sc_flags & IWX_FLAG_BGSCAN) {
        err = iwx_scan_abort(sc);
        if (err) {
            XYLog("%s: could not abort background scan\n",
                  DEVNAME(sc));
            return err;
        }
    }

    err = iwx_umac_scan(sc, 0);
    if (err) {
        sc->sc_pcitag->setProperty("ItlwmScanError",
            static_cast<UInt32>(err < 0 ? -err : err), 32);
        XYLog("%s: could not initiate scan\n", DEVNAME(sc));
        return err;
    }

    /*
     * The current mode might have been fixed during association.
     * Ensure all channels get scanned.
     */
    if (IFM_MODE(ic->ic_media.ifm_cur->ifm_media) == IFM_AUTO)
        ieee80211_setmode(ic, IEEE80211_MODE_AUTO);

    sc->sc_flags |= IWX_FLAG_SCANNING;
    if (ifp->if_flags & IFF_DEBUG)
        XYLog("%s: %s -> %s\n", ifp->if_xname,
              ieee80211_state_name[ic->ic_state],
              ieee80211_state_name[IEEE80211_S_SCAN]);
    if ((sc->sc_flags & IWX_FLAG_BGSCAN) == 0) {
        ieee80211_set_link_state(ic, LINK_STATE_DOWN);
        ieee80211_node_cleanup(ic, ic->ic_bss);
    }
    ic->ic_state = IEEE80211_S_SCAN;
    getMainCommandGate()->commandWakeup(&ic->ic_state); /* wake iwx_init() */

    return 0;
}

int ItlIwx::
iwx_bz_port_scan_abort(struct iwx_softc *sc)
{
    XYLog("%s flags=0x%x\n", __FUNCTION__, sc->sc_flags);
    if ((sc->sc_flags & (IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN)) == 0)
        return 0;

    int err = iwx_umac_scan_abort(sc);
    if (err) {
        XYLog("%s: iwx_umac_scan_abort failed %d\n", DEVNAME(sc), err);
        return err;
    }

    uint32_t budget = 1000;
    int generation = sc->sc_generation;
    while (generation == sc->sc_generation &&
           (sc->sc_flags & (IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN))) {
        err = iwx_gate_sleep(sc, &sc->sc_flags, &budget, IWX_GATE_WAIT_NOTIF);
        if (generation != sc->sc_generation)
            return ENXIO;
        if (err && (sc->sc_flags & (IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN))) {
            XYLog("%s: scan abort timed out, clearing flags\n", DEVNAME(sc));
            sc->sc_flags &= ~(IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN);
            return err;
        }
    }
    return 0;
}

int ItlIwx::
iwx_bz_port_enable_mgmt_queue(struct iwx_softc *sc)
{
    int err;
    int cmdver;

    cmdver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP, IWX_ADD_STA);
    /* MLD firmware has no ADD_STA entry and, like ADD_STA v12+, no aux queue. */
    if (iwx_use_mld_api(sc) ||
        (cmdver != IWX_FW_CMD_VER_UNKNOWN && cmdver >= 12))
        sc->first_data_qid = IWX_QID_MGMT - 1;
    else
        sc->first_data_qid = IWX_QID_MGMT;
    err = iwx_enable_txq(sc, IWX_STATION_ID,
                         sc->first_data_qid, IWX_MGMT_TID,
                         sc->txq[sc->first_data_qid].ring_count);
    if (err) {
        XYLog("%s: could not enable MGMT Tx queue %d (error %d)\n",
              DEVNAME(sc), sc->first_data_qid, err);
        return err;
    }

    return 0;
}

uint16_t ItlIwx::
iwx_bz_port_rs_fw_get_config_flags(struct iwx_softc *sc)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni = ic->ic_bss;
    uint16_t flags = 0;

    if (ic->ic_state < IEEE80211_S_ASSOC) {
        return flags;
    }

    if (iwx_num_of_ant(iwx_fw_valid_tx_ant(sc)) > 1) {
        if (ni->ni_flags & IEEE80211_NODE_HE) {
            if (ni->ni_he_cap_elem.phy_cap_info[2] & IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ)
                flags |= IWX_TLC_MNG_CFG_FLAGS_STBC_MSK;
        } else if (ni->ni_vhtcaps & IEEE80211_VHTCAP_RXSTBC_MASK)
            flags |= IWX_TLC_MNG_CFG_FLAGS_STBC_MSK;
        else if (ni->ni_htcaps & IEEE80211_HTCAP_RXSTBC_MASK)
            flags |= IWX_TLC_MNG_CFG_FLAGS_STBC_MSK;
    }

    if (((ni->ni_htcaps & IEEE80211_HTCAP_LDPC) ||
         ((ic->ic_flags & IEEE80211_F_VHTON) && (ni->ni_vhtcaps & IEEE80211_VHTCAP_RXLDPC))))
        flags |= IWX_TLC_MNG_CFG_FLAGS_LDPC_MSK;

    /* consider LDPC support in case of HE */
    if ((ni->ni_flags & IEEE80211_NODE_HE) &&
        (ni->ni_he_cap_elem.phy_cap_info[1] &
        IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD))
        flags |= IWX_TLC_MNG_CFG_FLAGS_LDPC_MSK;

    if ((ni->ni_flags & IEEE80211_NODE_HE) &&
        !(ni->ni_he_cap_elem.phy_cap_info[1] &
         IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD))
        flags &= ~IWX_TLC_MNG_CFG_FLAGS_LDPC_MSK;

    if ((ni->ni_flags & IEEE80211_NODE_HE) &&
        (ic->ic_he_cap_elem.phy_cap_info[3] & IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_MASK) &&
        (ni->ni_he_cap_elem.phy_cap_info[3] &
         IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_MASK))
        flags |= IWX_TLC_MNG_CFG_FLAGS_HE_DCM_NSS_1_MSK;

    return flags;
}

int ItlIwx::
iwx_bz_port_rs_init(struct iwx_softc *sc, struct iwx_node *in, bool update)
{
    struct ieee80211_node *ni = &in->in_ni;
    struct ieee80211_rateset *rs = &ni->ni_rates;
    struct iwx_tlc_config_cmd_v4 cfg_cmd;
    uint32_t cmd_id;
    int i;
    uint16_t cmd_size;
    uint8_t cmdver;

    memset(&cfg_cmd, 0, sizeof(cfg_cmd));

    for (i = 0; i < rs->rs_nrates; i++) {
        uint8_t rval = rs->rs_rates[i] & IEEE80211_RATE_VAL;
        int idx = iwx_rs_rval2idx(rval);
        if (idx == -1)
            return EINVAL;
        cfg_cmd.non_ht_rates |= (1 << idx);
    }
    if (ni->ni_flags & IEEE80211_NODE_HE) {
        cfg_cmd.mode = IWX_TLC_MNG_MODE_HE;
        iwx_rs_fw_he_set_enabled_rates(sc, &cfg_cmd);
    } else if (ni->ni_flags & IEEE80211_NODE_VHT) {
        cfg_cmd.mode = IWX_TLC_MNG_MODE_VHT;
        iwx_rs_fw_vht_set_enabled_rates(sc, &cfg_cmd);
    } else if (ni->ni_flags & IEEE80211_NODE_HT) {
        cfg_cmd.mode = IWX_TLC_MNG_MODE_HT;
        cfg_cmd.ht_rates[IWX_TLC_NSS_1][IWX_TLC_MCS_PER_BW_80] = htole16(iwx_rs_ht_rates(sc, ni, ni->ni_chw == IEEE80211_CHAN_WIDTH_40 ? IEEE80211_HT_RATESET_CBW40_SISO : IEEE80211_HT_RATESET_SISO));
        if (ni->ni_rx_nss > 1)
            cfg_cmd.ht_rates[IWX_TLC_NSS_2][IWX_TLC_MCS_PER_BW_80] = htole16(iwx_rs_ht_rates(sc, ni, ni->ni_chw == IEEE80211_CHAN_WIDTH_40 ? IEEE80211_HT_RATESET_CBW40_MIMO2 : IEEE80211_HT_RATESET_MIMO2));
    } else
        cfg_cmd.mode = IWX_TLC_MNG_MODE_NON_HT;

    if (ni->ni_flags & IEEE80211_NODE_HE)
        XYLog("HE TLC: mode=%u width=%s nss=%u rates80=%04x/%04x rates160=%04x/%04x\n",
            cfg_cmd.mode, ieee80211_chan_width_name[ni->ni_chw], ni->ni_rx_nss,
            le16toh(cfg_cmd.ht_rates[0][0]), le16toh(cfg_cmd.ht_rates[1][0]),
            le16toh(cfg_cmd.ht_rates[0][1]), le16toh(cfg_cmd.ht_rates[1][1]));
    cfg_cmd.sta_id = IWX_STATION_ID;
    cfg_cmd.max_ch_width = update ? iwx_rs_fw_bw_from_sta_bw(ni->ni_chw) : IWX_RATE_MCS_CHAN_WIDTH_20;
    cfg_cmd.chains = iwx_rs_fw_set_active_chains(iwx_fw_valid_tx_ant(sc));
    cfg_cmd.flags = iwx_rs_fw_get_config_flags(sc);
    if ((ni->ni_flags & IEEE80211_NODE_HE) == 0) {
        if (ieee80211_node_supports_ht_sgi20(ni))
            cfg_cmd.sgi_ch_width_supp = (1 << IWX_TLC_MNG_CH_WIDTH_20MHZ);

        if (ieee80211_node_supports_ht_sgi40(ni))
            cfg_cmd.sgi_ch_width_supp |= (1 << IWX_TLC_MNG_CH_WIDTH_40MHZ);

        if (ieee80211_node_supports_vht_sgi80(ni)) {
            cfg_cmd.sgi_ch_width_supp |= (1 << IWX_TLC_MNG_CH_WIDTH_80MHZ);
        }

        if (ieee80211_node_supports_vht_sgi160(ni)) {
            cfg_cmd.sgi_ch_width_supp |= (1 << IWX_TLC_MNG_CH_WIDTH_160MHZ);
        }
    }

    cmd_id = iwx_cmd_id(IWX_TLC_MNG_CONFIG_CMD, IWX_DATA_PATH_GROUP, 0);
    cmdver = iwx_lookup_cmd_ver(sc, IWX_DATA_PATH_GROUP, IWX_TLC_MNG_CONFIG_CMD);
    XYLog("%s cmdver %d\n", __FUNCTION__, cmdver);
    if (cmdver == 4) {
        return iwx_send_cmd_pdu(sc, cmd_id, IWX_CMD_ASYNC,
                                sizeof(cfg_cmd), &cfg_cmd);
    } else if (cmdver < 4 || cmdver == IWX_FW_CMD_VER_UNKNOWN) {
        struct iwl_tlc_config_cmd_v3 cfg_cmd_v3 = {
            .sta_id = cfg_cmd.sta_id,
            .max_ch_width = cfg_cmd.max_ch_width,
            .mode = cfg_cmd.mode,
            .chains = cfg_cmd.chains,
            .amsdu = !!cfg_cmd.max_mpdu_len,
            .flags = cfg_cmd.flags,
            .non_ht_rates = cfg_cmd.non_ht_rates,
            .ht_rates[0][0] = cfg_cmd.ht_rates[0][0],
            .ht_rates[0][1] = cfg_cmd.ht_rates[0][1],
            .ht_rates[1][0] = cfg_cmd.ht_rates[1][0],
            .ht_rates[1][1] = cfg_cmd.ht_rates[1][1],
            .sgi_ch_width_supp = cfg_cmd.sgi_ch_width_supp,
            .max_mpdu_len = cfg_cmd.max_mpdu_len,
        };

        cmd_size = sizeof(cfg_cmd_v3);

        /* In old versions of the API the struct is 4 bytes smaller */
        if (cmdver == IWX_FW_CMD_VER_UNKNOWN || cmdver < 3)
            cmd_size -= 4;

        return iwx_send_cmd_pdu(sc, cmd_id, IWX_CMD_ASYNC, cmd_size,
                                &cfg_cmd_v3);
    } else {
        return -EINVAL;
    }
}

void ItlIwx::
iwx_bz_port_rs_update(struct iwx_softc *sc, struct iwx_tlc_update_notif *notif)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni = ic->ic_bss;
    struct ieee80211_rateset *rs = &ni->ni_rates;
    uint32_t rate_n_flags;
    int i;
    char pretty_rate[100];
    uint8_t notifver;
    uint32_t format;

    if (notif->sta_id != IWX_STATION_ID ||
        (le32toh(notif->flags) & IWX_TLC_NOTIF_FLAG_RATE) == 0)
        return;

    rate_n_flags = le32toh(notif->rate);
    notifver = iwx_lookup_notif_ver(sc, IWX_DATA_PATH_GROUP, IWX_TLC_MNG_UPDATE_NOTIF);
    if(notifver < 3 || notifver==IWX_FW_CMD_VER_UNKNOWN) rate_n_flags=iwx_new_rate_from_v1(rate_n_flags);
    format = rate_n_flags & IWX_RATE_MCS_MOD_TYPE_MSK;
    if (format == IWX_RATE_MCS_VHT_MSK ||
        format == IWX_RATE_MCS_HE_MSK ||
        format == IWX_RATE_MCS_EHT_MSK) {
        ni->ni_txmcs = rate_n_flags & IWX_RATE_MCS_CODE_MSK;
    } else if (format == IWX_RATE_MCS_HT_MSK) {
        ni->ni_txmcs = IWX_RATE_HT_MCS_INDEX(rate_n_flags);
    } else {
        uint8_t plcp = (rate_n_flags & IWX_RATE_LEGACY_RATE_MSK_V1);
        uint8_t rval = 0;
        for (i = IWX_RATE_1M_INDEX; i < nitems(iwx_rates); i++) {
            if (iwx_rates[i].plcp == plcp) {
                rval = iwx_rates[i].rate;
                break;
            }
        }
        if (rval) {
            uint8_t rv;
            for (i = 0; i < rs->rs_nrates; i++) {
                rv = rs->rs_rates[i] & IEEE80211_RATE_VAL;
                if (rv == rval) {
                    ni->ni_txrate = i;
                    break;
                }
            }
        }
    }
}

int ItlIwx::
iwx_bz_port_auth(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_node *in = (struct iwx_node *)ic->ic_bss;
    uint32_t duration;
    int generation = sc->sc_generation, err = 0;

    splassert(IPL_NET);

    /* The MLD path implements station mode only. */
    if (iwx_use_mld_api(sc) && ic->ic_opmode != IEEE80211_M_STA)
        return EOPNOTSUPP;

    in->in_ni.ni_chw = IEEE80211_CHAN_WIDTH_20_NOHT;
    in->in_ni.ni_flags &= ~(IEEE80211_NODE_HT |
                            IEEE80211_NODE_QOS |
                            IEEE80211_NODE_HT_SGI20 |
                            IEEE80211_NODE_HT_SGI40 |
                            IEEE80211_NODE_VHT |
                            IEEE80211_NODE_VHT_SGI80 |
                            IEEE80211_NODE_VHT_SGI160 |
                            IEEE80211_NODE_HE);
    if (ic->ic_opmode == IEEE80211_M_MONITOR) {
        err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                                  ic->ic_ibss_chan, 1, 1, 0);
        if (err)
            return err;
    } else {
        err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                                  in->in_ni.ni_chan, 1, 1, 0);
        if (err)
            return err;
    }
    in->in_phyctxt = &sc->sc_phyctxt[0];
    IEEE80211_ADDR_COPY(in->in_macaddr, in->in_ni.ni_macaddr);

    if (iwx_use_mld_api(sc))
        return iwx_mld_auth(sc, in);

    err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_ADD, 0);
    if (err) {
        XYLog("%s: could not add MAC context (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }
    sc->sc_flags |= IWX_FLAG_MAC_ACTIVE;

    err = iwx_binding_cmd(sc, in, IWX_FW_CTXT_ACTION_ADD);
    if (err) {
        XYLog("%s: could not add binding (error %d)\n",
              DEVNAME(sc), err);
        goto rm_mac_ctxt;
    }
    sc->sc_flags |= IWX_FLAG_BINDING_ACTIVE;

    err = iwx_add_sta_cmd(sc, in, 0);
    if (err) {
        XYLog("%s: could not add sta (error %d)\n",
              DEVNAME(sc), err);
        goto rm_binding;
    }

    if (ic->ic_opmode == IEEE80211_M_MONITOR) {
        err = iwx_enable_txq(sc, IWX_MONITOR_STA_ID,
                             IWX_DQA_INJECT_MONITOR_QUEUE, IWX_MGMT_TID,
                             sc->txq[IWX_DQA_INJECT_MONITOR_QUEUE].ring_count);
        if (err)
            goto rm_sta;
        return 0;
    }

    err = iwx_enable_mgmt_queue(sc);
    if (err)
        goto rm_sta;

    err = iwx_clear_statistics(sc);
    if (err)
        goto rm_sta;

    /*
     * Prevent the FW from wandering off channel during association
     * by "protecting" the session with a time event.
     */
    if (in->in_ni.ni_intval)
        duration = in->in_ni.ni_intval * 2;
    else
        duration = IEEE80211_DUR_TU;

    /* Try really hard to protect the session and hear a beacon
     * The new session protection command allows us to protect the
     * session for a much longer time since the firmware will internally
     * create two events: a 300TU one with a very high priority that
     * won't be fragmented which should be enough for 99% of the cases,
     * and another one (which we configure here to be 900TU long) which
     * will have a slightly lower priority, but more importantly, can be
     * fragmented so that it'll allow other activities to run.
     */
    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_SESSION_PROT_CMD))
        err = iwx_schedule_protect_session(sc, in, duration);
    else
        iwx_protect_session(sc, in, duration, in->in_ni.ni_intval / 2);

    return err;

rm_sta:
    if (generation == sc->sc_generation)
        iwx_rm_sta_cmd(sc, in);
rm_binding:
    if (generation == sc->sc_generation) {
        iwx_binding_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE);
        sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
    }
rm_mac_ctxt:
    if (generation == sc->sc_generation) {
        iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0);
        sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
    }
    return err;
}

int ItlIwx::
iwx_bz_port_deauth(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_node *in = (struct iwx_node *)ic->ic_bss;
    int err, generation = sc->sc_generation;

    splassert(IPL_NET);

    if (iwx_use_mld_api(sc)) {
        err = iwx_mld_deauth(sc, in);
        if (in != NULL) {
            in->in_ni.ni_chw = IEEE80211_CHAN_WIDTH_20_NOHT;
            in->in_ni.ni_flags &= ~(IEEE80211_NODE_HT |
                                    IEEE80211_NODE_QOS |
                                    IEEE80211_NODE_HT_SGI20 |
                                    IEEE80211_NODE_HT_SGI40 |
                                    IEEE80211_NODE_VHT |
                                    IEEE80211_NODE_VHT_SGI80 |
                                    IEEE80211_NODE_VHT_SGI160 |
                                    IEEE80211_NODE_HE);
        }
        return err;
    }

    if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_SESSION_PROT_CMD))
        iwx_unprotect_session(sc, in);
    else {
        err = iwx_cancel_session_protection(sc, in);
        if (err)
            return err;
    }
    if (generation != sc->sc_generation)
        return ENXIO;

    if (sc->sc_flags & IWX_FLAG_STA_ACTIVE) {
        err = iwx_rm_sta(sc, in);
        if (err)
            return err;
    }

    if (sc->sc_flags & IWX_FLAG_BINDING_ACTIVE) {
        err = iwx_binding_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE);
        if (err) {
            XYLog("%s: could not remove binding (error %d)\n",
                  DEVNAME(sc), err);
            return err;
        }
        sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
    }

    if (sc->sc_flags & IWX_FLAG_MAC_ACTIVE) {
        err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0);
        if (err) {
            XYLog("%s: could not remove MAC context (error %d)\n",
                  DEVNAME(sc), err);
            return err;
        }
        sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
    }

reset_node:
    in->in_ni.ni_chw = IEEE80211_CHAN_WIDTH_20_NOHT;
    in->in_ni.ni_flags &= ~(IEEE80211_NODE_HT |
                            IEEE80211_NODE_QOS |
                            IEEE80211_NODE_HT_SGI20 |
                            IEEE80211_NODE_HT_SGI40 |
                            IEEE80211_NODE_VHT |
                            IEEE80211_NODE_VHT_SGI80 |
                            IEEE80211_NODE_VHT_SGI160 |
                            IEEE80211_NODE_HE);
    /* Move unused PHY context to a default channel. */
    err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                              &ic->ic_channels[1], 1, 1, 0);
    if (err)
        return err;

    return 0;
}

void ItlIwx::
iwx_mld_probe(struct iwx_softc *sc, uint32_t step, int err)
{
    sc->sc_pcitag->setProperty("ItlwmMldAssocStep", step, 32);
    sc->sc_pcitag->setProperty("ItlwmMldAssocErr",
        static_cast<UInt32>(err < 0 ? -err : err), 32);
}

int ItlIwx::
iwx_mld_mac_config_cmd(struct iwx_softc *sc, struct iwx_node *in,
                       uint32_t action, int assoc)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_mac_config_cmd cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.id_and_color = htole32(IWX_MLD_MAC_ID);
    cmd.action = htole32(action);
    if (action != IWX_FW_CTXT_ACTION_REMOVE) {
        cmd.mac_type = htole32(IWX_FW_MAC_TYPE_BSS_STA);
        IEEE80211_ADDR_COPY(cmd.local_mld_addr, ic->ic_myaddr);
        cmd.filter_flags = htole32(IWX_MAC_CFG_FILTER_ACCEPT_GRP);
        /* Beacons must reach the host until association completes. */
        if (!assoc)
            cmd.filter_flags |= htole32(IWX_MAC_CFG_FILTER_ACCEPT_BEACON);
        if (in->in_ni.ni_flags & IEEE80211_NODE_HE)
            cmd.he_support = htole16(1);
        if (assoc) {
            cmd.client.is_assoc = 1;
            cmd.client.assoc_id = htole16(in->in_ni.ni_associd & 0x3fff);
        }
    }

    return iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_MAC_CONFIG_CMD,
        IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
}

int ItlIwx::
iwx_mld_link_config_cmd(struct iwx_softc *sc, struct iwx_node *in,
                        uint32_t action, uint32_t modify_mask, int active)
{
#define IWX_EXP2(x)    ((1 << (x)) - 1)    /* CWmin = 2^ECWmin - 1 */
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni = &in->in_ni;
    struct iwx_link_config_cmd cmd;
    int cck_ack_rates, ofdm_ack_rates;
    int i;

    memset(&cmd, 0, sizeof(cmd));
    cmd.action = htole32(action);
    cmd.link_id = htole32(IWX_MLD_LINK_ID);
    cmd.mac_id = htole32(IWX_MLD_MAC_ID);
    if (action == IWX_FW_CTXT_ACTION_REMOVE || in->in_phyctxt == NULL) {
        cmd.phy_id = htole32(IWX_FW_CTXT_INVALID);
        if (action == IWX_FW_CTXT_ACTION_REMOVE)
            return iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_LINK_CONFIG_CMD,
                IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
    } else
        cmd.phy_id = htole32(in->in_phyctxt->id);

    IEEE80211_ADDR_COPY(cmd.local_link_addr, ic->ic_myaddr);
    cmd.modify_mask = htole32(modify_mask);
    cmd.active = htole32(active ? 1 : 0);

    /*
     * Fill every BSS field; on MODIFY the firmware applies only the groups
     * named in modify_mask, as Linux iwl_mvm_link_changed() does.
     */
    iwx_ack_rates(sc, in, &cck_ack_rates, &ofdm_ack_rates);
    cmd.cck_rates = htole32(cck_ack_rates);
    cmd.ofdm_rates = htole32(ofdm_ack_rates);
    cmd.cck_short_preamble =
        htole32((ic->ic_flags & IEEE80211_F_SHPREAMBLE) ? 1 : 0);
    cmd.short_slot = htole32((ic->ic_flags & IEEE80211_F_SHSLOT) ? 1 : 0);

    for (i = 0; i < EDCA_NUM_AC; i++) {
        struct ieee80211_edca_ac_params *ac = &ic->ic_edca_ac[i];
        int txf = iwx_ac_to_tx_fifo[i];
        uint8_t ucode_ac = iwx_mvm_mac80211_ac_to_ucode_ac((enum ieee80211_edca_ac)i);

        cmd.ac[ucode_ac].cw_min = htole16(IWX_EXP2(ac->ac_ecwmin));
        cmd.ac[ucode_ac].cw_max = htole16(IWX_EXP2(ac->ac_ecwmax));
        cmd.ac[ucode_ac].aifsn = ac->ac_aifsn;
        cmd.ac[ucode_ac].fifos_mask = (1 << txf);
        cmd.ac[ucode_ac].edca_txop = htole16(ac->ac_txoplimit * 32);
    }
    if (ni->ni_flags & IEEE80211_NODE_QOS)
        cmd.qos_flags |= htole32(IWX_MAC_QOS_FLG_UPDATE_EDCA);

    if (ni->ni_flags & IEEE80211_NODE_HT) {
        enum ieee80211_htprot htprot =
        (enum ieee80211_htprot)(ni->ni_htop1 & IEEE80211_HTOP1_PROT_MASK);
        /* The fw does not distinguish between ht and fat */
        uint32_t ht_flag = IWX_LINK_PROT_FLG_HT_PROT | IWX_LINK_PROT_FLG_FAT_PROT;

        switch (htprot) {
            case IEEE80211_HTPROT_NONE:
                break;
            case IEEE80211_HTPROT_NONMEMBER:
            case IEEE80211_HTPROT_NONHT_MIXED:
                cmd.protection_flags = htole32(ht_flag);
                break;
            case IEEE80211_HTPROT_20MHZ:
                /* Protect when channel wider than 20MHz */
                if (ni->ni_chw > IEEE80211_CHAN_WIDTH_20)
                    cmd.protection_flags = htole32(ht_flag);
                break;
            default:
                XYLog("Illegal protection mode %d\n", htprot);
                break;
        }

        cmd.qos_flags |= htole32(IWX_MAC_QOS_FLG_TGN);
    }
    if (ic->ic_flags & IEEE80211_F_USEPROT)
        cmd.protection_flags |= htole32(IWX_LINK_PROT_FLG_TGG_PROTECT);

    cmd.bi = htole32(ni->ni_intval);
    cmd.dtim_interval = htole32(ni->ni_intval * ni->ni_dtimperiod);

    return iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_LINK_CONFIG_CMD,
        IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
#undef IWX_EXP2
}

int ItlIwx::
iwx_mld_sta_config_cmd(struct iwx_softc *sc, struct iwx_node *in, int assoc)
{
    struct ieee80211_node *ni = &in->in_ni;
    struct iwx_sta_cfg_cmd cmd;
    uint32_t agg_size = 0, mpdu_dens = 0;

    /*
     * Station entry for the AP, following Linux iwl_mvm_mld_cfg_sta().
     * Before association only the addresses and MFP are set; the run path
     * adds the association ID, MIMO and A-MPDU parameters.
     */
    memset(&cmd, 0, sizeof(cmd));
    cmd.sta_id = htole32(IWX_STATION_ID);
    cmd.link_id = htole32(IWX_MLD_LINK_ID);
    IEEE80211_ADDR_COPY(cmd.peer_mld_address, in->in_macaddr);
    IEEE80211_ADDR_COPY(cmd.peer_link_address, in->in_macaddr);
    cmd.station_type = htole32(IWX_STATION_TYPE_PEER);
    /*
     * Linux sets mfp while the station is not yet authorized when the
     * firmware has STA_EXP_MFP_SUPPORT, which covers every STA_CONFIG it
     * sends in client mode; otherwise only for MFP associations.
     */
    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_STA_EXP_MFP_SUPPORT) ||
        (ni->ni_flags & IEEE80211_NODE_MFP))
        cmd.mfp = htole32(1);

    if (assoc) {
        cmd.assoc_id = htole32(ni->ni_associd & 0x3fff);
        if ((ni->ni_flags & (IEEE80211_NODE_HT | IEEE80211_NODE_VHT |
            IEEE80211_NODE_HE)) && iwx_mimo_enabled(sc) && ni->ni_rx_nss > 1)
            cmd.mimo = htole32(1);
        if (ni->ni_flags & IEEE80211_NODE_HT) {
            agg_size = ni->ni_ampdu_param & IEEE80211_AMPDU_PARAM_LE;
            mpdu_dens = (ni->ni_ampdu_param & IEEE80211_AMPDU_PARAM_SS) >> 2;
        }
        if (ni->ni_flags & IEEE80211_NODE_VHT)
            agg_size = (ni->ni_vhtcaps &
                IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK) >>
                IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT;
        if (agg_size > (IWX_STA_FLG_MAX_AGG_SIZE_4M >>
            IWX_STA_FLG_MAX_AGG_SIZE_SHIFT))
            agg_size = IWX_STA_FLG_MAX_AGG_SIZE_4M >>
                IWX_STA_FLG_MAX_AGG_SIZE_SHIFT;
        cmd.tx_ampdu_spacing = htole32(mpdu_dens);
        cmd.tx_ampdu_max_size = htole32(agg_size);
        if (ni->ni_flags & IEEE80211_NODE_QOS)
            cmd.sp_length = htole32(128);
    }

    return iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_STA_CONFIG_CMD,
        IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
}

int ItlIwx::
iwx_mld_sta_remove_cmd(struct iwx_softc *sc)
{
    struct iwx_sta_remove_cmd cmd = {
        .sta_id = htole32(IWX_STATION_ID),
    };

    return iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_STA_REMOVE_CMD,
        IWX_MAC_CONF_GROUP, 0), 0, sizeof(cmd), &cmd);
}

int ItlIwx::
iwx_mld_disable_txq(struct iwx_softc *sc, int sta_id, int tid)
{
    struct iwx_scd_queue_cfg_cmd cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.operation = htole32(IWX_SCD_QUEUE_REMOVE);
    cmd.u.remove.sta_mask = htole32(1 << sta_id);
    cmd.u.remove.tid = htole32(tid);

    return iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_SCD_QUEUE_CONFIG_CMD,
        IWX_DATA_PATH_GROUP, 0), 0, sizeof(cmd), &cmd);
}

int ItlIwx::
iwx_mld_rm_sta(struct iwx_softc *sc, struct iwx_node *in)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni = &in->in_ni;
    int err, i, generation = sc->sc_generation;

    /*
     * ADD_STA-based draining is not available on MLD firmware, so flush
     * directly. SCD_QUEUE_CONFIG v3 requires queues to be removed before
     * their station.
     */
    sc->sc_flags |= IWX_FLAG_TXFLUSH;
    err = iwx_flush_sta_tids(sc, IWX_STATION_ID, 0xffff);
    if (generation == sc->sc_generation)
        sc->sc_flags &= ~IWX_FLAG_TXFLUSH;
    if (err) {
        XYLog("%s: could not flush Tx path (error %d)\n",
              __FUNCTION__, err);
        return err;
    }

    /*
     * Keys go before the station, as in Linux: mac80211 frees the pairwise
     * key first and iwl_mvm_sec_key_remove_ap() removes the group keys
     * ahead of STA_REMOVE_CMD.
     */
    for (i = 0; i < nitems(sc->sc_mld_key_flags); i++) {
        struct iwx_sec_key_cmd kcmd;

        if (!sc->sc_mld_key_flags[i])
            continue;
        memset(&kcmd, 0, sizeof(kcmd));
        kcmd.action = htole32(IWX_FW_CTXT_ACTION_REMOVE);
        kcmd.u.remove.sta_mask = htole32(1 << IWX_STATION_ID);
        kcmd.u.remove.key_id = htole32(i);
        kcmd.u.remove.key_flags = htole32(sc->sc_mld_key_flags[i]);
        err = iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_SEC_KEY_CMD,
            IWX_DATA_PATH_GROUP, 0), 0, sizeof(kcmd), &kcmd);
        if (generation != sc->sc_generation)
            return ENXIO;
        if (err) {
            XYLog("%s: could not remove key %d (error %d)\n",
                  DEVNAME(sc), i, err);
            return err;
        }
        sc->sc_mld_key_flags[i] = 0;
        ++sc->sc_mld_key_removed;
        sc->sc_pcitag->setProperty("ItlwmMldKeyRemoved",
            sc->sc_mld_key_removed, 32);
    }

    for (i = 0; i < IWX_MAX_TID_COUNT; i++) {
        if (sc->sc_tid_data[i].qid <= 0 ||
            sc->sc_tid_data[i].qid == IWX_INVALID_QUEUE)
            continue;
        err = iwx_mld_disable_txq(sc, IWX_STATION_ID, i);
        if (err) {
            XYLog("%s: could not remove TX queue for tid %d (error %d)\n",
                  DEVNAME(sc), i, err);
            return err;
        }
        sc->sc_tid_data[i].qid = IWX_INVALID_QUEUE;
    }
    sc->sc_mld_txq_pending = 0;
    sc->sc_mld_txq_failed = 0;
    sc->sc_pcitag->setProperty("ItlwmMldDataQueues", 0ULL, 32);

    if (sc->sc_flags & IWX_FLAG_MLD_TXQ_ACTIVE) {
        err = iwx_mld_disable_txq(sc, IWX_STATION_ID, IWX_MGMT_TID);
        if (err) {
            XYLog("%s: could not remove MGMT Tx queue (error %d)\n",
                  DEVNAME(sc), err);
            return err;
        }
        sc->sc_flags &= ~IWX_FLAG_MLD_TXQ_ACTIVE;
    }

    err = iwx_mld_sta_remove_cmd(sc);
    if (err) {
        XYLog("%s: could not remove STA (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }
    sc->sc_flags &= ~IWX_FLAG_STA_ACTIVE;

    sc->sc_rx_ba_sessions = 0;
    sc->ba_rx.start_tidmask = 0;
    sc->ba_rx.stop_tidmask = 0;
    sc->ba_tx.start_tidmask = 0;
    sc->ba_tx.stop_tidmask = 0;
    for (i = 0; i < IEEE80211_NUM_TID; i++) {
        struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[i];
        if (ba->ba_state != IEEE80211_BA_AGREED)
            continue;
        ieee80211_delba_request(ic, ni, 0, 1, i);
    }

    return 0;
}

int ItlIwx::
iwx_mld_auth(struct iwx_softc *sc, struct iwx_node *in)
{
    uint32_t step, duration;
    int generation = sc->sc_generation, err;

    /* Channel 44 was flagged passive in an earlier run; record what we join. */
    if (in->in_ni.ni_chan && in->in_ni.ni_chan != IEEE80211_CHAN_ANYC) {
        sc->sc_pcitag->setProperty("ItlwmMldAuthChan",
            ieee80211_chan2ieee(&sc->sc_ic, in->in_ni.ni_chan), 32);
        sc->sc_pcitag->setProperty("ItlwmMldAuthChanFlags",
            in->in_ni.ni_chan->ic_flags, 32);
    }

    step = IWX_MLD_STEP_MAC_ADD;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_mac_config_cmd(sc, in, IWX_FW_CTXT_ACTION_ADD, 0);
    if (err) {
        XYLog("%s: could not add MLD MAC (error %d)\n", DEVNAME(sc), err);
        goto fail;
    }
    sc->sc_flags |= IWX_FLAG_MAC_ACTIVE;

    /* Linux add_interface disables beacon filtering for a new MAC. */
    err = iwx_disable_beacon_filter(sc);
    if (err)
        XYLog("%s: could not disable beacon filter (error %d)\n",
              DEVNAME(sc), err);

    step = IWX_MLD_STEP_LINK_ADD;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_ADD, 0, 0);
    if (err) {
        XYLog("%s: could not add link (error %d)\n", DEVNAME(sc), err);
        goto rm_mac;
    }
    /* On MLD firmware the link takes the place of the binding. */
    sc->sc_flags |= IWX_FLAG_BINDING_ACTIVE;

    step = IWX_MLD_STEP_LINK_ACTIVATE;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY,
        IWX_LINK_CONTEXT_MODIFY_ACTIVE | IWX_LINK_CONTEXT_MODIFY_RATES_INFO, 1);
    if (err) {
        XYLog("%s: could not activate link (error %d)\n", DEVNAME(sc), err);
        goto rm_link;
    }
    /* Power state is updated after activation, as in Linux. */
    err = iwx_power_mac_update_mode(sc, in);
    if (err)
        XYLog("%s: could not update MAC power (error %d)\n",
              DEVNAME(sc), err);

    step = IWX_MLD_STEP_STA_ADD;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_sta_config_cmd(sc, in, 0);
    if (err) {
        XYLog("%s: could not add sta (error %d)\n", DEVNAME(sc), err);
        goto deactivate_link;
    }
    sc->sc_flags |= IWX_FLAG_STA_ACTIVE;

    /*
     * Linux configures rate scaling as soon as the AP station exists so
     * the firmware can send auth/assoc frames at a rate the AP supports;
     * BZ firmware selects management frame rates itself.
     */
    step = IWX_MLD_STEP_AUTH_TLC;
    iwx_mld_probe(sc, step, 0);
    err = iwx_rs_init(sc, in, false);
    if (err) {
        XYLog("%s: could not init rate scaling (error %d)\n",
              DEVNAME(sc), err);
        goto rm_sta;
    }

    step = IWX_MLD_STEP_MGMT_QUEUE;
    iwx_mld_probe(sc, step, 0);
    err = iwx_enable_mgmt_queue(sc);
    if (err)
        goto rm_sta;
    sc->sc_flags |= IWX_FLAG_MLD_TXQ_ACTIVE;

    /* Keep the firmware on channel while authenticating. */
    if (in->in_ni.ni_intval)
        duration = in->in_ni.ni_intval * 2;
    else
        duration = IEEE80211_DUR_TU;
    step = IWX_MLD_STEP_SESSION_PROT;
    iwx_mld_probe(sc, step, 0);
    err = iwx_schedule_protect_session(sc, in, duration);
    if (err)
        goto rm_sta;

    iwx_mld_probe(sc, IWX_MLD_STEP_AUTH_READY, 0);
    return 0;

rm_sta:
    if (generation == sc->sc_generation)
        iwx_mld_rm_sta(sc, in);
deactivate_link:
    if (generation == sc->sc_generation)
        iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY,
            IWX_LINK_CONTEXT_MODIFY_ACTIVE, 0);
rm_link:
    if (generation == sc->sc_generation) {
        iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0, 0);
        sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
    }
rm_mac:
    if (generation == sc->sc_generation) {
        iwx_mld_mac_config_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0);
        sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
    }
fail:
    iwx_mld_probe(sc, step, err);
    return err;
}

int ItlIwx::
iwx_mld_deauth(struct iwx_softc *sc, struct iwx_node *in)
{
    int err, generation = sc->sc_generation;

    iwx_mld_probe(sc, IWX_MLD_STEP_DEAUTH, 0);

    if (sc->sc_flags & IWX_FLAG_MAC_ACTIVE) {
        err = iwx_cancel_session_protection(sc, in);
        if (err)
            goto fail;
    }
    if (generation != sc->sc_generation)
        return ENXIO;

    if (sc->sc_flags & IWX_FLAG_STA_ACTIVE) {
        err = iwx_mld_rm_sta(sc, in);
        if (err)
            goto fail;
    }

    if (sc->sc_flags & IWX_FLAG_BINDING_ACTIVE) {
        err = iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY,
            IWX_LINK_CONTEXT_MODIFY_ACTIVE, 0);
        if (err)
            goto fail;
        err = iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0, 0);
        if (err)
            goto fail;
        sc->sc_flags &= ~IWX_FLAG_BINDING_ACTIVE;
    }

    if (sc->sc_flags & IWX_FLAG_MAC_ACTIVE) {
        err = iwx_mld_mac_config_cmd(sc, in, IWX_FW_CTXT_ACTION_REMOVE, 0);
        if (err)
            goto fail;
        sc->sc_flags &= ~IWX_FLAG_MAC_ACTIVE;
    }

    iwx_mld_probe(sc, IWX_MLD_STEP_DEAUTH_DONE, 0);
    return 0;

fail:
    XYLog("%s: MLD teardown failed (error %d)\n", DEVNAME(sc), err);
    iwx_mld_probe(sc, IWX_MLD_STEP_DEAUTH, err);
    return err;
}

int ItlIwx::
iwx_wait_txq_empty(struct iwx_softc *sc, int qid)
{
    struct iwx_tx_ring *ring = &sc->txq[qid];
    uint32_t budget = IWX_FLUSH_WAIT_MS;
    int err = 0, generation = sc->sc_generation;

    while (ring->queued > 0 && generation == sc->sc_generation) {
        err = iwx_gate_sleep(sc, ring, &budget, IWX_GATE_WAIT_TXQ_EMPTY);
        if (err)
            break;
    }
    if (generation != sc->sc_generation)
        return ENXIO;
    return ring->queued > 0 ? ETIMEDOUT : 0;
}

void ItlIwx::
iwx_mld_send_leave(struct iwx_softc *sc, enum ieee80211_state ostate)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni = ic->ic_bss;
    uint32_t mgmt_before = sc->sc_tx_mgmt;
    mbuf_t m;
    int err;

    if (ni == NULL || !(sc->sc_flags & IWX_FLAG_MLD_TXQ_ACTIVE))
        return;

    /* The frames net80211 sends from RUN and ASSOC, in its order. */
    if (ostate == IEEE80211_S_RUN) {
        ieee80211_stop_ampdu_tx(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH);
        IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DISASSOC,
            IEEE80211_REASON_ASSOC_LEAVE);
    }
    IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
        IEEE80211_REASON_AUTH_LEAVE);

    /* iwx_start() holds frames while the RUN transition is pending. */
    while ((m = mq_dequeue(&ic->ic_mgtq)) != NULL) {
        ni = (struct ieee80211_node *)mbuf_pkthdr_rcvif(m);
        if (iwx_tx(sc, m, ni, EDCA_AC_BE) != 0)
            ieee80211_release_node(ic, ni);
    }
    /* From ASSOC, iwx_start() has already sent them; count both paths. */
    sc->sc_mld_leave_frames += sc->sc_tx_mgmt - mgmt_before;
    sc->sc_pcitag->setProperty("ItlwmMldLeaveFrames",
        sc->sc_mld_leave_frames, 32);

    err = iwx_wait_txq_empty(sc, sc->first_data_qid);
    if (err)
        XYLog("%s: leave frames still queued (error %d)\n", DEVNAME(sc), err);
}

int ItlIwx::
iwx_mld_run(struct iwx_softc *sc, struct iwx_node *in)
{
    int chains = iwx_mimo_enabled(sc) ? 2 : 1;
    uint32_t step;
    int err;
    const int generation=sc->sc_generation;

    if (in->in_ni.ni_chw == IEEE80211_CHAN_WIDTH_80P80)
        in->in_ni.ni_chw = IEEE80211_CHAN_WIDTH_20;

    /* Configure Rx chains for MIMO. */
    if (in->in_ni.ni_flags & (IEEE80211_NODE_HT | IEEE80211_NODE_VHT |
        IEEE80211_NODE_HE)) {
        err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                                  in->in_ni.ni_chan, chains, chains, 0);
        if (err) {
            XYLog("%s: failed to update PHY\n", DEVNAME(sc));
            return err;
        }
    }

    step = IWX_MLD_STEP_RUN_LINK;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_link_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY,
        IWX_LINK_CONTEXT_MODIFY_RATES_INFO |
        IWX_LINK_CONTEXT_MODIFY_PROTECT_FLAGS |
        IWX_LINK_CONTEXT_MODIFY_QOS_PARAMS |
        IWX_LINK_CONTEXT_MODIFY_BEACON_TIMING, 1);
    if (err)
        goto fail;

    /* Start at lowest available bit-rate. Firmware will raise. */
    in->in_ni.ni_txrate = 0;
    in->in_ni.ni_txmcs = 0;
    step = IWX_MLD_STEP_RUN_TLC;
    iwx_mld_probe(sc, step, 0);
    err = iwx_rs_init(sc, in, true);
    if (err)
        goto fail;

    step = IWX_MLD_STEP_RUN_STA;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_sta_config_cmd(sc, in, 1);
    if (err)
        goto fail;

    step = IWX_MLD_STEP_RUN_MAC;
    iwx_mld_probe(sc, step, 0);
    err = iwx_mld_mac_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 1);
    if (err)
        goto fail;

    step = IWX_MLD_STEP_RUN_READY;
    err = iwx_sf_config(sc, IWX_SF_FULL_ON);
    if (err)
        goto fail;
    err = iwx_allow_mcast(sc);
    if (err)
        goto fail;
    err = iwx_power_update_device(sc);
    if (err)
        goto fail;
    err = iwx_power_mac_update_mode(sc, in);
    if (err)
        goto fail;

    /*
     * Give every data TID its own queue now: the firmware runs TX block ack
     * itself but only aggregates frames on a per-TID queue. The lazy path
     * in iwx_tx() never produced one on hardware, so allocate up front.
     * A failure is fatal to this association: KGP retains uncertain DMA
     * ownership, so management-queue fallback cannot certify success.
     */
    {
        uint32_t allocated = 0;
        for (int t = 0; t < IWX_MAX_TID_COUNT; t++) {
            if (sc->sc_tid_data[t].qid > 0 &&
                sc->sc_tid_data[t].qid != IWX_INVALID_QUEUE) {
                allocated |= 1 << t;
                continue;
            }
            if (!iwx_nic_lock(sc)) {err=EBUSY;goto fail;}
            int q = iwx_tvqm_alloc_txq(sc, t, 0);
            if(generation!=sc->sc_generation) {err=ENXIO;goto fail;}
            iwx_nic_unlock(sc);
            if (q < 0) {
                sc->sc_mld_txq_failed |= 1 << t;
                sc->sc_pcitag->setProperty("ItlwmMldTxqErr",
                    static_cast<UInt32>(-q), 32);
                XYLog("%s: could not allocate TX queue for tid %d (error %d)\n",
                      DEVNAME(sc), t, q);
                err=-q;goto fail; // KGP ambiguous queue failure closes admission; never publish RUN.
            }
            allocated |= 1 << t;
        }
        sc->sc_pcitag->setProperty("ItlwmMldDataQueues", allocated, 32);
    }

    iwx_mld_probe(sc, step, 0);
    return 0;

fail:
    XYLog("%s: MLD association step %u failed (error %d)\n",
          DEVNAME(sc), step, err);
    iwx_mld_probe(sc, step, err);
    return err;
}

int ItlIwx::
iwx_mld_run_stop(struct iwx_softc *sc, struct iwx_node *in)
{
    int err, i, generation = sc->sc_generation;

    iwx_mld_probe(sc, IWX_MLD_STEP_RUN_STOP, 0);

    sc->sc_flags |= IWX_FLAG_TXFLUSH;
    err = iwx_flush_sta_tids(sc, IWX_STATION_ID, 0xffff);
    if (generation == sc->sc_generation)
        sc->sc_flags &= ~IWX_FLAG_TXFLUSH;
    if (err)
        goto fail;

    /*
     * Remove RX block ack sessions while the station still exists, as
     * mac80211 tears them down before the driver removes the station. On
     * hardware the firmware asserted in STA_REMOVE_CMD (UMAC 0x20000072)
     * with BAIDs still allocated, so every disconnect restarted it.
     */
    for (i = 0; i < nitems(sc->sc_rxba_data); i++) {
        struct iwx_rxba_data *rxba = &sc->sc_rxba_data[i];
        if (rxba->baid == IWX_RX_REORDER_DATA_INVALID_BAID)
            continue;
        err = iwx_bz_port_sta_rx_agg(sc, &in->in_ni, rxba->tid, 0, 0, 0, 0);
        if (err)
            goto fail;
        ++sc->sc_mld_rx_ba_removed;
        sc->sc_pcitag->setProperty("ItlwmMldRxBaRemoved",
            sc->sc_mld_rx_ba_removed, 32);
    }

    err = iwx_sf_config(sc, IWX_SF_INIT_OFF);
    if (err)
        goto fail;
    err = iwx_disable_beacon_filter(sc);
    if (err)
        goto fail;
    err = iwx_mld_mac_config_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 0);
    if (err)
        goto fail;

    /* Reset Tx chains in case MIMO was enabled. */
    if ((in->in_ni.ni_flags & IEEE80211_NODE_HT) && iwx_mimo_enabled(sc)) {
        err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                                  in->in_ni.ni_chan, 1, 1, 0);
        if (err)
            goto fail;
    }
    return 0;

fail:
    XYLog("%s: MLD run stop failed (error %d)\n", DEVNAME(sc), err);
    iwx_mld_probe(sc, IWX_MLD_STEP_RUN_STOP, err);
    return err;
}

int ItlIwx::
iwx_mld_sec_key_cmd(struct iwx_softc *sc, struct ieee80211_node *ni,
                    struct ieee80211_key *k, uint32_t action)
{
    struct iwx_sec_key_cmd cmd;
    uint32_t flags = IWX_SEC_KEY_FLAG_CIPHER_CCMP;
    int err;

    if (k->k_flags & IEEE80211_KEY_GROUP)
        flags |= IWX_SEC_KEY_FLAG_MCAST_KEY;
    else if (ni->ni_flags & IEEE80211_NODE_MFP)
        flags |= IWX_SEC_KEY_FLAG_MFP;

    memset(&cmd, 0, sizeof(cmd));
    cmd.action = htole32(action);
    if (action == IWX_FW_CTXT_ACTION_REMOVE) {
        cmd.u.remove.sta_mask = htole32(1 << IWX_STATION_ID);
        cmd.u.remove.key_id = htole32(k->k_id);
        cmd.u.remove.key_flags = htole32(flags);
    } else {
        cmd.u.add.sta_mask = htole32(1 << IWX_STATION_ID);
        cmd.u.add.key_id = htole32(k->k_id);
        cmd.u.add.key_flags = htole32(flags);
        memcpy(cmd.u.add.key, k->k_key, MIN(sizeof(cmd.u.add.key), k->k_len));
        cmd.u.add.tx_seq = htole64(k->k_tsc);
    }

    err = iwx_send_cmd_pdu(sc, iwx_cmd_id(IWX_SEC_KEY_CMD,
        IWX_DATA_PATH_GROUP, 0), IWX_CMD_ASYNC, sizeof(cmd), &cmd);
    /* Track submitted keys for teardown; async enqueue is NOT firmware acceptance proof. */
    if (!err && k->k_id < nitems(sc->sc_mld_key_flags))
        sc->sc_mld_key_flags[k->k_id] =
            action == IWX_FW_CTXT_ACTION_REMOVE ? 0 : flags;
    if (!err && action != IWX_FW_CTXT_ACTION_REMOVE) {
        ++sc->sc_mld_keys;
        sc->sc_pcitag->setProperty("ItlwmMldKeys", sc->sc_mld_keys, 32);
    }
    sc->sc_pcitag->setProperty("ItlwmMldKeyErr",
        static_cast<UInt32>(err < 0 ? -err : err), 32);
    return err;
}

int ItlIwx::
iwx_bz_port_run(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_node *in = (struct iwx_node *)ic->ic_bss;
    int err;
    int chains = iwx_mimo_enabled(sc) ? 2 : 1;

    splassert(IPL_NET);

    if (iwx_use_mld_api(sc))
        return iwx_mld_run(sc, in);

    if (ic->ic_opmode == IEEE80211_M_MONITOR) {
        /* Add a MAC context and a sniffing STA. */
        err = iwx_auth(sc);
        if (err)
            return err;
    }

    if (in->in_ni.ni_chw == IEEE80211_CHAN_WIDTH_80P80) {
        /* Fallback to 20mhz VHT */
        in->in_ni.ni_chw = IEEE80211_CHAN_WIDTH_20;
    }

    /* Configure Rx chains for MIMO. */
    if ((ic->ic_opmode == IEEE80211_M_MONITOR ||
         (in->in_ni.ni_flags & IEEE80211_NODE_HT) ||
         (in->in_ni.ni_flags & IEEE80211_NODE_VHT) ||
         (in->in_ni.ni_flags & IEEE80211_NODE_HE))) {
        err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                                  in->in_ni.ni_chan, chains, chains, 0);
        if (err) {
            XYLog("%s: failed to update PHY\n",
                  DEVNAME(sc));
            return err;
        }
    }

    /* We have now been assigned an associd by the AP. */
    err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 1);
    if (err) {
        XYLog("%s: failed to update MAC\n", DEVNAME(sc));
        return err;
    }

    err = iwx_sf_config(sc, IWX_SF_FULL_ON);
    if (err) {
        XYLog("%s: could not set sf full on (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }

    err = iwx_allow_mcast(sc);
    if (err) {
        XYLog("%s: could not allow mcast (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }

    err = iwx_power_update_device(sc);
    if (err) {
        XYLog("%s: could not send power command (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }
#ifdef notyet
    /*
     * Disabled for now. Default beacon filter settings
     * prevent net80211 from getting ERP and HT protection
     * updates from beacons.
     */
    err = iwx_enable_beacon_filter(sc, in);
    if (err) {
        XYLog("%s: could not enable beacon filter\n",
              DEVNAME(sc));
        return err;
    }
#endif
    err = iwx_power_mac_update_mode(sc, in);
    if (err) {
        XYLog("%s: could not update MAC power (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }

    if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_DYNAMIC_QUOTA)) {
        err = iwx_update_quotas(sc, in, 1);
        if (err) {
            XYLog("%s: could not update quotas (error %d)\n",
                  DEVNAME(sc), err);
            return err;
        }
    }

    if (ic->ic_opmode == IEEE80211_M_MONITOR)
        return 0;

    /* Start at lowest available bit-rate. Firmware will raise. */
    in->in_ni.ni_txrate = 0;
    in->in_ni.ni_txmcs = 0;

    err = iwx_add_sta_cmd(sc, in, 1);
    if (err) {
        XYLog("%s: could not update sta (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }

    err = 0; /* HE unsupported in initial legacy profile. */
    if (err)
        return err;
    err = iwx_rs_init(sc, in, true);
    if (err) {
        XYLog("%s: could not update rate scaling (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }
    return 0;
}

int ItlIwx::
iwx_bz_port_run_stop(struct iwx_softc *sc)
{
    struct ieee80211com *ic = &sc->sc_ic;
    struct iwx_node *in = (struct iwx_node *)ic->ic_bss;
    int err, i;

    splassert(IPL_NET);

    if (iwx_use_mld_api(sc))
        return iwx_mld_run_stop(sc, in);

    err = iwx_flush_sta(sc, in);
    if (err) {
        XYLog("%s: could not flush Tx path (error %d)\n",
              __FUNCTION__, err);
        return err;
    }

    /*
     * Stop Rx BA sessions now. We cannot rely on the BA task
     * for this when moving out of RUN state since it runs in a
     * separate thread.
     * Note that in->in_ni (struct ieee80211_node) already represents
     * our new access point in case we are roaming between APs.
     * This means we cannot rely on struct ieee802111_node to tell
     * us which BA sessions exist.
     */
    for (i = 0; i < nitems(sc->sc_rxba_data); i++) {
        struct iwx_rxba_data *rxba = &sc->sc_rxba_data[i];
        if (rxba->baid == IWX_RX_REORDER_DATA_INVALID_BAID)
            continue;
        err = iwx_bz_port_sta_rx_agg(sc, ic->ic_bss, rxba->tid, 0, 0, 0, 0);
        if (err)
            return err;
    }

    err = iwx_sf_config(sc, IWX_SF_INIT_OFF);
    if (err)
        return err;

    err = iwx_disable_beacon_filter(sc);
    if (err) {
        XYLog("%s: could not disable beacon filter (error %d)\n",
              DEVNAME(sc), err);
        return err;
    }

    if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_DYNAMIC_QUOTA)) {
        err = iwx_update_quotas(sc, in, 0);
        if (err) {
            XYLog("%s: could not update quotas (error %d)\n",
                  DEVNAME(sc), err);
            return err;
        }
    }

    err = iwx_mac_ctxt_cmd(sc, in, IWX_FW_CTXT_ACTION_MODIFY, 0);
    if (err) {
        XYLog("%s: failed to update MAC\n", DEVNAME(sc));
        return err;
    }

    /* Reset Tx chains in case MIMO was enabled. */
    if ((in->in_ni.ni_flags & IEEE80211_NODE_HT) &&
        iwx_mimo_enabled(sc)) {
        err = iwx_phy_ctxt_update(sc, &sc->sc_phyctxt[0],
                                  in->in_ni.ni_chan, 1, 1, 0);
        if (err) {
            XYLog("%s: failed to update PHY\n", DEVNAME(sc));
            return err;
        }
    }

    return 0;
}

int ItlIwx::
iwx_bz_port_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
            struct ieee80211_key *k)
{
    struct iwx_softc *sc = (struct iwx_softc *)ic->ic_softc;
    struct iwx_add_sta_key_cmd cmd;
    ItlIwx *that = container_of(sc, ItlIwx, com);

    if (k->k_cipher != IEEE80211_CIPHER_CCMP) {
        /* Fallback to software crypto for other ciphers. */
        return (ieee80211_set_key(ic, ni, k));
    }

    if (that->iwx_use_mld_api(sc))
        return that->iwx_mld_sec_key_cmd(sc, ni, k, IWX_FW_CTXT_ACTION_ADD);

    memset(&cmd, 0, sizeof(cmd));

    cmd.common.key_flags = htole16(IWX_STA_KEY_FLG_CCM |
                                   IWX_STA_KEY_FLG_WEP_KEY_MAP |
                                   ((k->k_id << IWX_STA_KEY_FLG_KEYID_POS) &
                                    IWX_STA_KEY_FLG_KEYID_MSK));
    if (k->k_flags & IEEE80211_KEY_GROUP) {
        cmd.common.key_offset = 1;
        cmd.common.key_flags |= htole16(IWX_STA_KEY_MULTICAST);
    } else
        cmd.common.key_offset = 0;

    memcpy(cmd.common.key, k->k_key, MIN(sizeof(cmd.common.key), k->k_len));
    cmd.common.sta_id = IWX_STATION_ID;

    cmd.transmit_seq_cnt = htole64(k->k_tsc);

    return that->iwx_send_cmd_pdu(sc, IWX_ADD_STA_KEY, IWX_CMD_ASYNC,
                            sizeof(cmd), &cmd);
}

void ItlIwx::
iwx_bz_port_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
               struct ieee80211_key *k)
{
    struct iwx_softc *sc = (struct iwx_softc *)ic->ic_softc;
    struct iwx_add_sta_key_cmd cmd;
    ItlIwx *that = container_of(sc, ItlIwx, com);

    if (k->k_cipher != IEEE80211_CIPHER_CCMP) {
        /* Fallback to software crypto for other ciphers. */
        ieee80211_delete_key(ic, ni, k);
        return;
    }

    /* Keys go away with the station; only remove them while it exists. */
    if (that->iwx_use_mld_api(sc)) {
        if (sc->sc_flags & IWX_FLAG_STA_ACTIVE)
            that->iwx_mld_sec_key_cmd(sc, ni, k, IWX_FW_CTXT_ACTION_REMOVE);
        return;
    }

    memset(&cmd, 0, sizeof(cmd));

    cmd.common.key_flags = htole16(IWX_STA_KEY_NOT_VALID |
                                   IWX_STA_KEY_FLG_NO_ENC | IWX_STA_KEY_FLG_WEP_KEY_MAP |
                                   ((k->k_id << IWX_STA_KEY_FLG_KEYID_POS) &
                                    IWX_STA_KEY_FLG_KEYID_MSK));
    memcpy(cmd.common.key, k->k_key, MIN(sizeof(cmd.common.key), k->k_len));
    if (k->k_flags & IEEE80211_KEY_GROUP)
        cmd.common.key_offset = 1;
    else
        cmd.common.key_offset = 0;
    cmd.common.sta_id = IWX_STATION_ID;

    that->iwx_send_cmd_pdu(sc, IWX_ADD_STA_KEY, IWX_CMD_ASYNC, sizeof(cmd), &cmd);
}

int ItlIwx::
iwx_bz_port_media_change(struct _ifnet *ifp)
{
    struct iwx_softc *sc = (struct iwx_softc *)ifp->if_softc;
    struct ieee80211com *ic = &sc->sc_ic;
    uint8_t rate, ridx;
    int err;

    err = ieee80211_media_change(ifp);
    if (err != ENETRESET)
        return err;

    if (ic->ic_fixed_mcs != -1)
        sc->sc_fixed_ridx = iwx_mcs2ridx[ic->ic_fixed_mcs];
    else if (ic->ic_fixed_rate != -1) {
        rate = ic->ic_sup_rates[ic->ic_curmode].
        rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
        /* Map 802.11 rate to HW rate index. */
        for (ridx = 0; ridx <= IWX_RIDX_MAX; ridx++)
            if (iwx_rates[ridx].rate == rate)
                break;
        sc->sc_fixed_ridx = ridx;
    }

    if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
        (IFF_UP | IFF_RUNNING)) {
        iwx_stop(ifp);
        err = iwx_init(ifp);
    }
    return err;
}

void ItlIwx::
iwx_bz_port_endscan(struct iwx_softc *sc)
{
    struct ieee80211com *ic = &sc->sc_ic;

    struct ieee80211_node *ni, *nextbs;

//    ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
//    for (; ni != NULL; ni = nextbs) {
//        nextbs = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
//        XYLog("%s scan_result ssid=%s, bssid=%s, ni_rsnciphers=%d, ni_rsncipher=%d, ni_rsngroupmgmtcipher=%d, ni_rsngroupcipher=%d, ni_rssi=%d,  ni_capinfo=%d, ni_intval=%d, ni_rsnakms=%d, ni_supported_rsnakms=%d, ni_rsnprotos=%d, ni_supported_rsnprotos=%d, ni_rstamp=%d\n", __FUNCTION__, ni->ni_essid, ether_sprintf(ni->ni_bssid), ni->ni_rsnciphers, ni->ni_rsncipher, ni->ni_rsngroupmgmtcipher, ni->ni_rsngroupcipher, ni->ni_rssi, ni->ni_capinfo, ni->ni_intval, ni->ni_rsnakms, ni->ni_supported_rsnakms, ni->ni_rsnprotos, ni->ni_supported_rsnprotos, ni->ni_rstamp);
//    }

    if ((sc->sc_flags & (IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN)) == 0)
        return;

    {
        uint32_t nodes = 0;
        struct ieee80211_node *scan_ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
        while (scan_ni != NULL) {
            ++nodes;
            scan_ni = RB_NEXT(ieee80211_tree, &ic->ic_tree, scan_ni);
        }
        sc->sc_pcitag->setProperty("ItlwmScanNodes", nodes, 32);
        if (sc->sc_scan_start_ns != 0) {
            uint64_t elapsed = iwx_boot_time_ns() - sc->sc_scan_start_ns;
            sc->sc_pcitag->setProperty("ItlwmScanMs",
                static_cast<UInt32>(elapsed / 1000000ULL), 32);
        }
    }

    sc->sc_flags &= ~(IWX_FLAG_SCANNING | IWX_FLAG_BGSCAN);
    getMainCommandGate()->commandWakeup(&sc->sc_flags);
    ieee80211_end_scan(&ic->ic_if);
}

int ItlIwx::
iwx_bz_port_send_soc_conf(struct iwx_softc *sc)
{
   struct iwx_soc_configuration_cmd cmd;
   int err;
   uint32_t cmd_id, flags = 0;

   memset(&cmd, 0, sizeof(cmd));

   /*
    * In VER_1 of this command, the discrete value is considered
    * an integer; In VER_2, it's a bitmask.  Since we have only 2
    * values in VER_1, this is backwards-compatible with VER_2,
    * as long as we don't set any other flag bits.
    */
   if (!sc->sc_integrated) { /* VER_1 */
       flags = IWX_SOC_CONFIG_CMD_FLAGS_DISCRETE;
   } else { /* VER_2 */
       uint8_t scan_cmd_ver;
       if (sc->sc_ltr_delay != IWX_SOC_FLAGS_LTR_APPLY_DELAY_NONE)
           flags |= ((uint32_t)sc->sc_ltr_delay <<
               IWX_SOC_FLAGS_LTR_APPLY_DELAY_SHIFT) &
               IWX_SOC_FLAGS_LTR_APPLY_DELAY_MASK;
       scan_cmd_ver = iwx_lookup_cmd_ver(sc, IWX_LONG_GROUP,
           IWX_SCAN_REQ_UMAC);
       if (scan_cmd_ver != IWX_FW_CMD_VER_UNKNOWN &&
           scan_cmd_ver >= 2 && sc->sc_low_latency_xtal)
           flags |= IWX_SOC_CONFIG_CMD_FLAGS_LOW_LATENCY;
   }
   cmd.flags = htole32(flags);

   cmd.latency = htole32(sc->sc_xtal_latency);

   cmd_id = iwx_cmd_id(IWX_SOC_CONFIGURATION_CMD, IWX_SYSTEM_GROUP, 0);
   err = iwx_send_cmd_pdu(sc, cmd_id, 0, sizeof(cmd), &cmd);
   if (err)
       XYLog("%s: failed to set soc latency: %d\n", DEVNAME(sc), err);
   return err;
}

int ItlIwx::
iwx_bz_port_send_update_mcc_cmd(struct iwx_softc *sc, const char *alpha2)
{
    XYLog("%s\n", __FUNCTION__);
    struct iwx_mcc_update_cmd mcc_cmd;
    struct iwx_host_cmd hcmd = {
        .id = IWX_MCC_UPDATE_CMD,
        .flags = IWX_CMD_WANT_RESP,
        .data = { &mcc_cmd },
    };
    struct iwx_rx_packet *pkt;
    struct iwx_mcc_update_resp *resp;
    struct iwx_mcc_update_resp_v8 *resp_v8;
    size_t resp_len;
    uint32_t n_channels;
    uint8_t notif_ver;
    int err;

    memset(&mcc_cmd, 0, sizeof(mcc_cmd));
    mcc_cmd.mcc = htole16(alpha2[0] << 8 | alpha2[1]);
    if (isset(sc->sc_ucode_api, IWX_UCODE_TLV_API_WIFI_MCC_UPDATE) ||
        isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_LAR_MULTI_MCC))
        mcc_cmd.source_id = IWX_MCC_SOURCE_GET_CURRENT;
    else
        mcc_cmd.source_id = IWX_MCC_SOURCE_OLD_FW;

    hcmd.len[0] = sizeof(struct iwx_mcc_update_cmd);
    hcmd.resp_pkt_len = IWX_CMD_RESP_MAX;

    err = iwx_send_cmd(sc, &hcmd);
    if (err)
        return err;

    pkt = hcmd.resp_pkt;
    if (!pkt || (pkt->hdr.group_id & IWX_CMD_FAILED_MSK)) {
        err = EIO;
        goto out;
    }

    resp_len = iwx_rx_packet_payload_len(pkt);
    notif_ver = iwx_lookup_notif_ver(sc, IWX_LONG_GROUP,
                                    IWX_MCC_UPDATE_CMD);
    if (notif_ver != IWX_FW_CMD_VER_UNKNOWN && notif_ver >= 8) {
        if (resp_len < sizeof(*resp_v8)) {
            err = EIO;
            goto out;
        }
        resp_v8 = (struct iwx_mcc_update_resp_v8 *)pkt->data;
        n_channels = le32toh(resp_v8->n_channels);
        if ((resp_len - sizeof(*resp_v8)) %
                sizeof(resp_v8->channels[0]) != 0 ||
            n_channels != (resp_len - sizeof(*resp_v8)) /
                sizeof(resp_v8->channels[0])) {
            err = EIO;
            goto out;
        }
        /* Regulatory state decides which channels allow active TX. */
        sc->sc_pcitag->setProperty("ItlwmMccCode",
            le16toh(resp_v8->mcc), 32);
        sc->sc_pcitag->setProperty("ItlwmMccStatus",
            le32toh(resp_v8->status), 32);
        sc->sc_pcitag->setProperty("ItlwmMccSource",
            resp_v8->source_id, 32);
        sc->sc_pcitag->setProperty("ItlwmMccChannels", n_channels, 32);
        iwx_init_channel_map(sc, NULL, resp_v8->channels, n_channels);
        goto out;
    }
    if (resp_len < sizeof(*resp)) {
        err = EIO;
        goto out;
    }

    resp = (struct iwx_mcc_update_resp *)pkt->data;
    if (resp_len != sizeof(*resp) +
        resp->n_channels * sizeof(resp->channels[0])) {
        err = EIO;
        goto out;
    }

    DPRINTF(("MCC status=0x%x mcc=0x%x cap=0x%x time=0x%x geo_info=0x%x source_id=0x%d n_channels=%u\n",
             resp->status, resp->mcc, resp->cap, resp->time, resp->geo_info, resp->source_id, resp->n_channels));

    /* Update channel map for net80211 and our scan configuration. */
    iwx_init_channel_map(sc, NULL, resp->channels, resp->n_channels);

out:
    iwx_free_resp(sc, &hcmd);

    return err;
}

int ItlIwx::
iwx_bz_port_init_hw(struct iwx_softc *sc)
{
    XYLog("%s\n", __FUNCTION__);
    struct ieee80211com *ic = &sc->sc_ic;
    int err, i, generation = sc->sc_generation;
    int step = 0;

    step = 1;
    err = iwx_preinit(sc);
    if (err)
        goto err;

    step = 2;
    err = iwx_start_hw(sc);
    if (err) {
        XYLog("%s: could not initialize hardware\n", DEVNAME(sc));
        goto err;
    }

    step = 3;
    err = iwx_run_init_mvm_ucode(sc, 0);
    if (err)
        goto err;

    if (!iwx_nic_lock(sc))
        return EBUSY;

    step = 4;
    err = iwx_send_tx_ant_cfg(sc, iwx_fw_valid_tx_ant(sc));
    if (err) {
        XYLog("%s: could not init tx ant config (error %d)\n",
               DEVNAME(sc), err);
        goto err;
    }

    iwx_toggle_tx_ant(sc, &sc->sc_mgmt_last_antenna_idx);

    if (sc->sc_tx_with_siso_diversity) {
        err = iwx_send_phy_cfg_cmd(sc);
        if (err) {
            XYLog("%s: could not send phy config (error %d)\n",
                   DEVNAME(sc), err);
            goto err;
        }
    }

    step = 5;
    err = iwx_send_bt_init_conf(sc);
    if (err) {
        XYLog("%s: could not init bt coex (error %d)\n",
               DEVNAME(sc), err);
        goto err;
    }

    step = 6;
    err = iwx_send_soc_conf(sc);
    if (err)
        goto err;

    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_DQA_SUPPORT)){
        step = 7;
        err = iwx_send_dqa_cmd(sc);
        if (err)
            goto err;
    }

    /* Add auxiliary station for scanning */
    step = 8;
    err = iwx_add_aux_sta(sc);
    if (err) {
        XYLog("%s: could not add aux station (error %d)\n",
              DEVNAME(sc), err);
        goto err;
    }

    for (i = 0; i < IWX_NUM_PHY_CTX; i++) {
        /*
         * The channel used here isn't relevant as it's
         * going to be overwritten in the other flows.
         * For now use the first channel we have.
         */
        sc->sc_phyctxt[i].id = i;
        sc->sc_phyctxt[i].channel = &ic->ic_channels[1];
        step = 9;
        err = iwx_phy_ctxt_cmd(sc, &sc->sc_phyctxt[i], 1, 1,
                               IWX_FW_CTXT_ACTION_ADD, 0);
        if (err) {
            XYLog("%s: could not add phy context %d (error %d)\n",
                   DEVNAME(sc), i, err);
            goto err;
        }
    }

    if (!isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_SET_LTR_GEN2)) {
        step = 10;
        err = iwx_config_ltr(sc);
        if (err) {
            XYLog("%s: PCIe LTR configuration failed (error %d)\n",
                  DEVNAME(sc), err);
            goto err;
        }
    }

    if (isset(sc->sc_enabled_capa, IWX_UCODE_TLV_CAPA_CT_KILL_BY_FW)) {
        step = 11;
        err = iwx_send_temp_report_ths_cmd(sc);
        if (err)
            goto err;
    }

    step = 12;
    err = iwx_power_update_device(sc);
    if (err) {
        XYLog("%s: could not send power command (error %d)\n",
               DEVNAME(sc), err);
        goto err;
    }

    if (sc->sc_nvm.lar_enabled) {
        step = 13;
        err = iwx_send_update_mcc_cmd(sc, "ZZ");
        if (err) {
            XYLog("%s: could not init LAR (error %d)\n",
                   DEVNAME(sc), err);
            goto err;
        }
    }

    step = 14;
    err = iwx_config_umac_scan(sc);
    if (err) {
        XYLog("%s: could not configure scan (error %d)\n",
               DEVNAME(sc), err);
        goto err;
    }

    step = 15;
    err = iwx_disable_beacon_filter(sc);
    if (err) {
        XYLog("%s: could not disable beacon filter (error %d)\n",
               DEVNAME(sc), err);
        goto err;
    }

err:
    sc->sc_pcitag->setProperty("ItlwmInitStep", static_cast<UInt32>(step), 32);
    if (err)
        sc->sc_pcitag->setProperty("ItlwmInitErr", static_cast<UInt32>(err < 0 ? -err : err), 32);
    if (generation == sc->sc_generation)
        iwx_nic_unlock(sc);
    return err;
}

int ItlIwx::
iwx_read_error_table(struct iwx_softc *sc, uint32_t base, void *table,
                     size_t size, bool supplied_by_tlv)
{
    const bool bz = sc->sc_device_family == IWX_DEVICE_FAMILY_BZ;
    if (!iwx_error_table_address_valid(base, size, bz,
            supplied_by_tlv || (sc->sc_uc.uc_intr && sc->sc_uc.uc_ok)))
        return EINVAL;
    int err = iwx_read_mem(sc, base, table, (int)(size / sizeof(uint32_t)));
    if (err)
        return err;
    /* An inaccessible SRAM aperture must not become a valid firmware assert. */
    return iwx_boot_io_invalid(*(uint32_t *)table) ? EIO : 0;
}

void ItlIwx::
iwx_bz_capture_error_tables(struct iwx_softc *sc)
{
    /* Called by the waiting startup thread before reset/free, never the filter.
     * Preserve both cores independently even if one address/read/valid fails. */
    const uint32_t bases[] = {
        sc->sc_uc.uc_lmac_error_event_table[0] & ~IWX_FW_ADDR_CACHE_CONTROL,
        sc->sc_uc.uc_umac_error_event_table & ~IWX_FW_ADDR_CACHE_CONTROL
    };
    const uint32_t flags[] = { IWX_ERROR_EVENT_TABLE_LMAC1, IWX_ERROR_EVENT_TABLE_UMAC };
    const size_t sizes[] = { sizeof(struct iwx_error_event_table), sizeof(struct iwx_umac_error_event_table) };
    const char *names[][5] = {
        { "ItlwmLmacErrorTableAddress", "ItlwmLmacErrorTableReadError", "ItlwmLmacErrorTableValid",
          "ItlwmLmacErrorId", "ItlwmLmacErrorTableRaw" },
        { "ItlwmUmacErrorTableAddress", "ItlwmUmacErrorTableReadError", "ItlwmUmacErrorTableValid",
          "ItlwmUmacErrorId", "ItlwmUmacErrorTableRaw" }
    };
    for (unsigned i = 0; i < 2; i++) {
        uint32_t raw[sizeof(struct iwx_error_event_table) / sizeof(uint32_t)] = {};
        int err = iwx_read_error_table(sc, bases[i], raw, sizes[i],
            (sc->sc_uc.error_event_table_tlv_status & flags[i]) != 0);
        const bool valid = !err && raw[0] != 0;
        sc->sc_pcitag->setProperty(names[i][0], bases[i], 32);
        sc->sc_pcitag->setProperty(names[i][1], err, 32);
        sc->sc_pcitag->setProperty(names[i][2], valid);
        sc->sc_pcitag->setProperty(names[i][3], valid ? raw[1] : 0, 32);
        if (!err)
            sc->sc_pcitag->setProperty(names[i][4], raw, (unsigned int)sizes[i]);
        else
            sc->sc_pcitag->removeProperty(names[i][4]);
    }
    sc->sc_pcitag->setProperty("ItlwmBootErrorTablesCaptured", true);
}

void ItlIwx::
iwx_bz_port_nic_umac_error(struct iwx_softc *sc)
{
    struct iwx_umac_error_event_table table;
    uint32_t base;

    base = sc->sc_uc.uc_umac_error_event_table;
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ)
        base &= ~IWX_FW_ADDR_CACHE_CONTROL;
    int err = iwx_read_error_table(sc, base, &table, sizeof(table),
        (sc->sc_uc.error_event_table_tlv_status & IWX_ERROR_EVENT_TABLE_UMAC) != 0);
    if (err) {
        XYLog("%s: UMAC error table 0x%08x read failed: %d\n", DEVNAME(sc), base, err);
        return;
    }
    if (!table.valid) {
        XYLog("%s: UMAC errlog empty\n", DEVNAME(sc));
        return;
    }

    if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
        XYLog("%s: Start UMAC Error Log Dump:\n", DEVNAME(sc));
        XYLog("%s: Status: 0x%x, count: %d\n", DEVNAME(sc),
              sc->sc_flags, table.valid);
    }

    XYLog("%s: 0x%08X | %s\n", DEVNAME(sc), table.error_id,
          iwx_desc_lookup(table.error_id));
    XYLog("%s: 0x%08X | umac branchlink1\n", DEVNAME(sc), table.blink1);
    XYLog("%s: 0x%08X | umac branchlink2\n", DEVNAME(sc), table.blink2);
    XYLog("%s: 0x%08X | umac interruptlink1\n", DEVNAME(sc), table.ilink1);
    XYLog("%s: 0x%08X | umac interruptlink2\n", DEVNAME(sc), table.ilink2);
    XYLog("%s: 0x%08X | umac data1\n", DEVNAME(sc), table.data1);
    XYLog("%s: 0x%08X | umac data2\n", DEVNAME(sc), table.data2);
    XYLog("%s: 0x%08X | umac data3\n", DEVNAME(sc), table.data3);
    XYLog("%s: 0x%08X | umac major\n", DEVNAME(sc), table.umac_major);
    XYLog("%s: 0x%08X | umac minor\n", DEVNAME(sc), table.umac_minor);
    XYLog("%s: 0x%08X | frame pointer\n", DEVNAME(sc),
          table.frame_pointer);
    XYLog("%s: 0x%08X | stack pointer\n", DEVNAME(sc),
          table.stack_pointer);
    XYLog("%s: 0x%08X | last host cmd\n", DEVNAME(sc), table.cmd_header);
    XYLog("%s: 0x%08X | isr status reg\n", DEVNAME(sc),
          table.nic_isr_pref);
    /* The kernel log is overwritten quickly on this machine; keep the assert. */
    sc->sc_pcitag->setProperty("ItlwmFwErrorId", table.error_id, 32);
    sc->sc_pcitag->setProperty("ItlwmFwErrorData1", table.data1, 32);
    sc->sc_pcitag->setProperty("ItlwmFwErrorCmd", table.cmd_header, 32);
}

void ItlIwx::
iwx_nic_lmac_error(struct iwx_softc *sc)
{
    struct iwx_error_event_table table;
    uint32_t base;

    XYLog("%s: dumping device error log\n", DEVNAME(sc));
    base = sc->sc_uc.uc_lmac_error_event_table[0];
    if (sc->sc_device_family == IWX_DEVICE_FAMILY_BZ)
        base &= ~IWX_FW_ADDR_CACHE_CONTROL;
    int err = iwx_read_error_table(sc, base, &table, sizeof(table),
        (sc->sc_uc.error_event_table_tlv_status & IWX_ERROR_EVENT_TABLE_LMAC1) != 0);
    if (err) {
        XYLog("%s: LMAC error table 0x%08x read failed: %d\n", DEVNAME(sc), base, err);
        return;
    }

    if (!table.valid) {
        XYLog("%s: errlog not found, skipping\n", DEVNAME(sc));
        return;
    }

    if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
        XYLog("%s: Start Error Log Dump:\n", DEVNAME(sc));
        XYLog("%s: Status: 0x%x, count: %d\n", DEVNAME(sc),
              sc->sc_flags, table.valid);
    }

    XYLog("%s: 0x%08X | %-28s\n", DEVNAME(sc), table.error_id,
          iwx_desc_lookup(table.error_id));
    XYLog("%s: %08X | trm_hw_status0\n", DEVNAME(sc),
          table.trm_hw_status0);
    XYLog("%s: %08X | trm_hw_status1\n", DEVNAME(sc),
          table.trm_hw_status1);
    XYLog("%s: %08X | branchlink2\n", DEVNAME(sc), table.blink2);
    XYLog("%s: %08X | interruptlink1\n", DEVNAME(sc), table.ilink1);
    XYLog("%s: %08X | interruptlink2\n", DEVNAME(sc), table.ilink2);
    XYLog("%s: %08X | data1\n", DEVNAME(sc), table.data1);
    XYLog("%s: %08X | data2\n", DEVNAME(sc), table.data2);
    XYLog("%s: %08X | data3\n", DEVNAME(sc), table.data3);
    XYLog("%s: %08X | beacon time\n", DEVNAME(sc), table.bcon_time);
    XYLog("%s: %08X | tsf low\n", DEVNAME(sc), table.tsf_low);
    XYLog("%s: %08X | tsf hi\n", DEVNAME(sc), table.tsf_hi);
    XYLog("%s: %08X | time gp1\n", DEVNAME(sc), table.gp1);
    XYLog("%s: %08X | time gp2\n", DEVNAME(sc), table.gp2);
    XYLog("%s: %08X | uCode revision type\n", DEVNAME(sc),
          table.fw_rev_type);
    XYLog("%s: %08X | uCode version major\n", DEVNAME(sc),
          table.major);
    XYLog("%s: %08X | uCode version minor\n", DEVNAME(sc),
          table.minor);
    XYLog("%s: %08X | hw version\n", DEVNAME(sc), table.hw_ver);
    XYLog("%s: %08X | board version\n", DEVNAME(sc), table.brd_ver);
    XYLog("%s: %08X | hcmd\n", DEVNAME(sc), table.hcmd);
    XYLog("%s: %08X | isr0\n", DEVNAME(sc), table.isr0);
    XYLog("%s: %08X | isr1\n", DEVNAME(sc), table.isr1);
    XYLog("%s: %08X | isr2\n", DEVNAME(sc), table.isr2);
    XYLog("%s: %08X | isr3\n", DEVNAME(sc), table.isr3);
    XYLog("%s: %08X | isr4\n", DEVNAME(sc), table.isr4);
    XYLog("%s: %08X | last cmd Id\n", DEVNAME(sc), table.last_cmd_id);
    XYLog("%s: %08X | wait_event\n", DEVNAME(sc), table.wait_event);
    XYLog("%s: %08X | l2p_control\n", DEVNAME(sc), table.l2p_control);
    XYLog("%s: %08X | l2p_duration\n", DEVNAME(sc), table.l2p_duration);
    XYLog("%s: %08X | l2p_mhvalid\n", DEVNAME(sc), table.l2p_mhvalid);
    XYLog("%s: %08X | l2p_addr_match\n", DEVNAME(sc), table.l2p_addr_match);
    XYLog("%s: %08X | lmpm_pmg_sel\n", DEVNAME(sc), table.lmpm_pmg_sel);
    XYLog("%s: %08X | timestamp\n", DEVNAME(sc), table.u_timestamp);
    XYLog("%s: %08X | flow_handler\n", DEVNAME(sc), table.flow_handler);

}

void ItlIwx::
iwx_bz_port_nic_error(struct iwx_softc *sc)
{
    ++sc->sc_fw_errors;
    sc->sc_pcitag->setProperty("ItlwmFwErrors", sc->sc_fw_errors, 32);
    iwx_nic_lmac_error(sc);
    if (sc->sc_uc.uc_umac_error_event_table)
        iwx_nic_umac_error(sc);
}
