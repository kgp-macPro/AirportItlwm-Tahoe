# 2.0.0 technical lineage and scope

## Integration and attribution

The published 1.0.0 mapper-aware foundation, subsequent BE200 Alpha 2/P8R1 development and the final AX210 R9 implementation are integrated in one source tree. The original [gsfront PR #5](https://github.com/kgp-macPro/AirportItlwm-Tahoe/pull/5) was regularly merged into its original **be200-alpha** target as [999d71fafb4788702b527572932dcc9768fb122e](https://github.com/kgp-macPro/AirportItlwm-Tahoe/commit/999d71fafb4788702b527572932dcc9768fb122e). Both original commits, afc3752384355d6fae594fac0e8f9799f32f15bd and 94a9a7fab992646b7a4fb43811f3aef97cd7b530, and authorship are in main's ancestry. Overlapping files were resolved to the reviewed final implementation, not replayed twice.

The public integration transfers the exact protected final R9 source inputs; it does not import private handovers, chat logs or unrelated experimental branches. The release build source is `4b948ae16a7cbe82699dc7078cf4c917456eff89`. The production manifest records every protected input and its R9 hash, including the version-only project difference. [Build identity](BUILD.md) distinguishes this rebuild from the hardware-tested candidates.

AX HE uses the public [laobamac/itlwm v2.4.0-alpha reference](https://github.com/laobamac/itlwm/tree/7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e), commit **7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e**. GPL-2.0 attribution is retained. AX HE capability intersection, firmware-v68 command layouts, HE context, TLC and station sequencing were adapted to our Ventura Legacy frontend, rather than importing native Tahoe framework or DMA assumptions. BE200 instead uses BZ/GL92 station/link command formats. A successful BE200 command is not an AX wire-ABI reference. REA with local Ghidra was used to inspect built paths and legacy handoff behavior; static analysis was correlated with archived diagnostics and hardware measurements, not presented as runtime proof.

Historical mapper/TX/command ownership and PC1 recovery, NKN/maddog860 VIF setup, Mieze/IntelLucy foundations and existing licensing remain in [credits](../CREDITS.md) and [historical documentation](HISTORY.md).

## Framework and DMA

The Tahoe target retains `USE_APPLE_SUPPLICANT`, `AIRPORT`, `__PRIVATE_SPI__`, `__IO80211_TARGET=__MAC_13_0`, `itlwm/PrivateSPI.pch` and IO80211FamilyLegacy. It requires OCLP-CustoMac 3.0.0+ with matching Ventura Legacy root patches. This is not laobamac's native Tahoe frontend or a new Skywalk mixture.

BE200/AX210 campaigns captured AppleVTD enabled and actual mapper-backed DMA. AX201/CNVW deliberately uses conventional DMA even with AppleVTD active. AX211's historical Alpha 2 mapped VHT capture does not establish new HE/BF qualification; follow the actual selected profile and runtime evidence. Firmware-63-dependent AX201/AX211 profiles remain a concrete availability limitation. Firmware-68 code paths and offline TLV checks are not universal firmware or hardware acceptance.

## Rate and MCS: exact device/mode dispatch

The new implementation is in [`ItlIwx::getHeTxReport`](../itlwm/hal_iwx/ItlIwx.cpp), called through [`ItlDriverInfo`](../include/HAL/ItlDriverInfo.hpp) by Legacy [`getRATE`, `getMCS` and `getMCS_VHT`](../AirportItlwm/AirportSTAIOCTL.cpp). It covers **non-BZ ItlIwx devices with an HE BSS**, including the AX201/CNVW, AX210/Ty and AX211/So dispatch paths when their NVM/firmware/association prerequisites succeed. It is not a PCI-2725-only switch. AX201/AX211 remain hardware-unvalidated for this HE/BF implementation.

`axTxRateNote` records AX TLC and eligible compressed BA TX feedback only in the current HE RUN lifetime. `getHeTxReport` selects the newest coherent timestamped tuple matching generation, reset epoch and connection-rate epoch. Rate, MCS, NSS, bandwidth and GI are decoded together; unsupported HE formats remain unknown rather than reusing an older plausible SU rate. The latest supported tuple remains available at idle with its age/freshness diagnostic (freshness threshold five seconds). It is a **last firmware report**, not instantaneous PHY measurement of every packet. Legacy's short/long-GI ABI cannot express the complete HE GI code; diagnostics retain it. No fixed MCS11 or 2401-Mbit/s value is supplied.

**HE and HE with BF opt-in use the same reporting chain. VHT does not enter this new HE report branch:** it keeps its existing TLC/node and VHT-rate-table reporting. BE200's `bzDevice()` guard returns to its established reporting path and different firmware rate representation. Shared Legacy getters query the virtual report; the base/default and BZ result are false, preserving their original fallback. Source review therefore covers shared call sites, not just an unchanged GL92 file.

## RX service: exact device/mode scope

[`ItlIwx::iwx_notif_intr`](../itlwm/hal_iwx/ItlIwx.cpp) dispatches BZ to `bzNotif` first. The R9 service budget runs only in the **leased/mapped non-BZ RX branch**, when `axHeDiagActive` is true: current BSS exists, state RUN, HE node flag set. This includes HE with or without BF; the BF switch is not a service-budget condition. It can apply to eligible mapped AX201/AX210/AX211-family paths, but CNVW's conventional branch does not execute it. **AX VHT and conventional RX are outside this budget. It is not a general VHT performance fix.**

At a completion boundary, after replacement backing passes `rxZcRepost`, ready RFH credits are published with the existing AX cur-minus-one/aligned-to-eight rule. Delivery occurs after **32 completion entries or at least 64 ready packets**. A completion can release several packets, and reordering can atomically release a larger group; the limit is a trigger, not a packet truncation or a 32-packet limit. Large atomic releases remain intact. After upper-stack callbacks, generation/reset/lifetime, HE state and cursor are rechecked before consuming the old snapshot or publishing credits. No old-generation write is issued after a reset/reentrant cursor change. The final handoff also follows safe credit publication.

The measured R8 small-control-packet handoff call returned promptly, yet TCP ACK was delayed seconds. R9 earlier delivery/service intervals correlate with a 0.167-ms result-request ACK, complete result exchange and three complete AX210 campaigns. This supports the targeted service correction; it does not uniquely localize every prior downstream delay or all remaining high-rate packet loss.

## 256-entry BA/reordering is separate

The 256-entry HE RX capacity predates R9 (gsfront's integration). `IEEE80211_BA_MAX_RX_WINSZ` and `iwx_reorder_buf.entries` allow 256 entries; HE ADDBA admission, allocation, negotiated `buf_size`, modulo indexing and free/reset loops agree. `iwx_sta_rx_agg` sends the negotiated window in the 16-bit firmware field and records the returned BAID without a 64-entry HE clamp. A valid smaller AP request remains smaller. Remaining legacy/TX 64-window rules are separate policies.

These are distinct quantities: **256 BA/reordering entries, 512 RX completion-ring slots, 32-completion service trigger, 64-ready-packet delivery trigger**. They are not bytes. R9 does not increase BA capacity or change the reorder algorithm.

## BE200 boundary and BF meaning

BE200 already services its completion path in bounded groups of 32; its GL92 protocols, credit ledger, aggregation, firmware-rate handling and BZ dispatch remain intact. P10-to-R9 review included shared getters/guards, 19 relevant firmware/BA/state/reorder functions and 113 BZ/net80211/interface files, rather than merely comparing the GL92 file. There is no evidenced need to copy the AX correction into BE200. Any future BE200 investigation is a separate evidence-driven task.

For AX, `-itlwmbf` with `-itlwmhe` enables **HE SU-Beamformee capability advertisement** under the HE/NVM/firmware conditions; it does not promise beamformer, MU-BF or actual sounding use. The tested AX210 BF campaign has both boot arguments confirmed by KGP and HE runtime evidence. Association capabilities plus matching AP sounding/feedback telemetry or a suitable monitor capture would be a minimal additional on-air-use proof. No extra capture or campaign was required for this release.

[All per-round results and residual losses](results/2.0.0.md) distinguish HE from BF opt-in and original P10/R9 identities. No AWDL/BLE experimental changes or Wi-Fi 7 implementation are included.
