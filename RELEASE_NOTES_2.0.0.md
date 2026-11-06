# AirportItlwm-Tahoe 2.0.0

**Intel Wi-Fi for macOS Tahoe with AppleVTD / IOMMU support.** BE200, AX210, AX201 and AX211; VHT, HE and optional HE + BF through the Ventura Legacy stack.

[Download the complete kext](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-2.0.0.zip) · [Installation and rollback](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/INSTALL.md) · [Test guide](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/benchmark/README.md) · [SHA256SUMS](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/SHA256SUMS.txt)

**Requires [OCLP-CustoMac 3.0.0+](https://github.com/kgp-macPro/OCLP-CustoMac/releases) and matching Ventura Legacy wireless root patches.** OCLP supplies the framework/patch environment; this separate Intel driver supplies device-specific DMA handling. BE200 and AX210 campaigns verified AppleVTD enabled and mapped DMA. AX201/CNVW remains conventional even with AppleVTD enabled. AX211 has older mapped VHT evidence; the new AX201/AX211 HE/BF code still needs hardware validation, and some firmware-63-dependent profiles remain unavailable.

## Included changes

- gsfront's original [PR #5](https://github.com/kgp-macPro/AirportItlwm-Tahoe/pull/5) is merged with both commits and authorship preserved: BE200 HE and HE SU-Beamformee opt-in, associated station/link setup and earlier HE 256-entry RX BA/reordering support.
- AX HE adapted from laobamac's pinned [v2.4.0-alpha / 7204c69](https://github.com/laobamac/itlwm/tree/7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e), retaining our Ventura Legacy frontend and AX firmware formats.
- Corrected AX HE firmware-rate reporting: coherent rate/MCS/NSS/width/GI from current-lifetime TLC/BA feedback, including last-known age/freshness diagnostics. The same path covers HE with BF opt-in. Firmware feedback is not a per-packet PHY measurement, and no fixed MCS11/2401 values are forced.
- Bounded **mapped AX-HE** RX service with earlier ready-packet handoff after 32 completions or 64 ready packets, safe credit publication and callback lifetime checks. Larger atomic reorder releases stay intact. This is not a VHT-wide fix or a new BA-window increase: 256-entry HE BA support already existed. BE200 keeps its own bounded BZ/GL92 service and firmware/reporting path.

See [technical scope and lineage](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/TECHNICAL_LINEAGE.md).

## Modes and comparable testing

| Mode | Boot arguments | Required wired second-Mac link for these campaigns |
|---|---|---|
| VHT | No HE/BF opt-in | **1000Base-T / 1 Gbit/s** |
| HE | `-itlwmhe` | **2500Base-T / 2.5 Gbit/s** |
| HE + SU-Beamformee opt-in | `-itlwmhe -itlwmbf` | **2500Base-T / 2.5 Gbit/s** |

No `-novht` for these campaigns. **Do not use 5000Base-T or 10GBase-T** for the comparison methodology, even with a 10-GbE-capable adapter. Record the actual negotiated peer link. These are comparison conditions, not general IEEE requirements. LAN iperf3 is separate from the final Internet test, which was limited by KGP's 80/30-Mbit/s Internet connection.

## Measured results and remaining limits

AX210 R9, receiver means across three rounds, Mbit/s:

| Campaign | TX P1 | TX P4 | RX P1 | RX P4 |
|---|---:|---:|---:|---:|
| HE / UDP1300 | 697.7 | 689.4 | 1668.2 | 1876.6 |
| HE / UDP1700 | 697.2 | 687.4 | 1700.3 | 1861.9 |
| HE + BF opt-in / UDP1700 | 696.9 | 688.4 | 1693.3 | 1887.6 |

All **180/180** tests plus three online tests completed; no complete zero intervals in 288 recorded TCP sum intervals. Seven original BE200 P10 campaigns cover VHT, HE and BF opt-in: **420/420** tests plus seven online tests. Standard and stress matrices remain separate in the [full tables and residuals](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/results/2.0.0.md); older AX210 P10-VHT is separately identified.

TCP-P4 retransmits remain, and complete UDP stress tests are not necessarily loss-free. HE-only RX1700 reported about 35% loss in all rounds, with other HE-only spikes retained. BF-opt-in RX through 1500 reported 0% in all rounds; RX1700 reported about 34%, 35% and 0% (last receiver about 1705 Mbit/s). UDP-TX sender ceilings/loss and all per-round retries remain in the data. There is no universal stability or BF-performance claim.

**AX210 HE160 with optional HE SU-Beamformee support, hardware-tested with the opt-in enabled.** Confirmed boot arguments, screenshots and throughput do not alone prove on-air sounding/feedback or BF gain. AX201/AX211 HE/BF remains hardware-unvalidated. No new Wi-Fi 7/EHT, MLO, 6-GHz, 320-MHz or AirDrop/AWDL/BLE functionality is included. Planned BE200 Wi-Fi 7 work is a future project, dependent on implementation and hardware testing, not this release.

## Downloads and identities

- [Complete kext package](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-2.0.0.zip), including license, identity and provenance.
- [Benchmark tools r13](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-Benchmark-Tools-r13.zip): all five 3×20 runners (standard, UDP1200, UDP1300, UDP1600, UDP1700), resources, profiles, optional AX diagnostics and failure capture. Extended TX remains capped at 600 Mbit/s; UDP1700 is a stress test, not a loss-free guarantee. Identity/binding/DMA guards remain strict.
- [Reproducible result data](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-2.0.0-Results.zip), original JSON with per-file hashes and campaign/source identities.
- [Build identity](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-2.0.0-BUILD-IDENTITY.json) and [checksums](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/SHA256SUMS.txt).

Release build source: `4b948ae16a7cbe82699dc7078cf4c917456eff89`. UUID **E01CC22A-6A2D-3E0D-A8FD-229ED62C1751**; executable SHA-256 **ab8688febf2ccc78984ea6b2b9be933cb14317bc7951983330ffadab32d3a2f8**. This newly versioned 2.0.0 rebuild has the final R9 functional inputs and new version metadata; it is **not byte-identical to the tested P10/R9 binaries**. Original campaigns retain their original UUID/hash. [Build details](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/BUILD.md).

Install the whole bundle manually and retain your working bundle/EFI/boot arguments for rollback. No installer or automatic system change is supplied.

## Tested Bluetooth configuration and credits

Bluetooth is separate: [lshbluesky IntelBluetoothFirmware and IntelBTPatcher 2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1), plus [Acidanthera BlueToolFixup 2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases/tag/2.7.2). These exact versions are recorded in the campaign preflights.

Thanks to OpenIntelWireless/zxystd, DexterSLamb, **gsfront**, **laobamac**, NorthKoreanNoodles/maddog860, Mieze/IntelLucy, OCLP developers/OCLP-CustoMac and the credited community testers. Local **REA/Ghidra** analysis supported source and binary investigation. KGP led integration, human review and repeated hardware validation with AI-assisted development. [Full credits, attribution and licenses](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/CREDITS.md).
