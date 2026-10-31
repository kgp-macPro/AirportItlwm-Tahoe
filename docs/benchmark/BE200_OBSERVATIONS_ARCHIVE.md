# Archived BE200 initial observations and publication context

Preserved from README at a9b32f747efedbfd283ea54673a25807472d126f; current version details are in the releases. This is historical, not the current tool guide.



### AirportItlwm-Tahoe 1.2.0 BE200 Alpha 2 — Performance Hotfix

**Required: Retro-OCLP-CustoMac root patching.** This BE200 alpha uses the restored Ventura legacy wireless frameworks and requires the corresponding root patches provided by [Retro-OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac). Keep the matching framework/Skywalk and AMFIPass/`-amfipassbeta` boot configuration; this is not a native, unpatched Wi-Fi driver.

**Test environment: macOS Tahoe 26.7.2 (build 25G332), x86_64, with AppleVTD enabled.**

**Further performance optimization is planned.** The current alpha focuses on initial functionality and recovery testing; its performance results are preliminary.

**Alpha 2 performance hotfix:** One complete three-round local BE200 benchmark improved average TCP TX from approximately 29 to 480 Mbit/s and TCP RX P1 from 195 to 482 Mbit/s. UDP RX600 reached approximately 600 Mbit/s with 0.08–0.20% reported loss. **KGP also reports successful P8R1 Wi-Fi OFF/ON and manual Sleep/Wake tests (USER-OBSERVED). Find My remains unresolved.** AX201/AX210/AX211 remain part of shared 1.2.0 development; this measurement qualifies the BE200 environment. TCP retransmissions, UDP losses, remaining RX variation and long-term qualification remain open; further optimization is planned.

### Download, installation and rollback

[**Download AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2.zip**](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v1.2.0-be200-alpha.2/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2.zip) · [Alpha prerelease](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.2) · [Installation and first checks](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.2.0-be200-alpha.2/docs/be200-alpha/INSTALL_AND_FIRST_TEST.md)

Extract the ZIP, back up the entire existing AirportItlwm bundle and boot configuration, then manually replace the existing bundle with the complete supplied `AirportItlwm.kext` without merging. Retain the required root-patched environment and reboot. To roll back, restore the complete previous bundle and its matching boot configuration and reboot. No supplied tool installs/loads a kext, changes EFI/root patches or starts traffic automatically.

Use this **separate experimental Alpha** only for BE200 testing. The qualified **1.0.0** remains the latest stable release; the **1.1.0 Beta comparison campaign** and its frozen artifacts are unchanged. The main branch retains the 1.0.0 driver source; the experimental implementation is confined to the separate Alpha source branch.

**Alpha ZIP SHA-256:** `4bc3ab56f754b88d9bf83087eaa1a6c5cde791c79eab373fd4008eb27cf2bcbf`.

### Observed functionality and scope

The following initial-functionality/recovery observations and screenshots are retained from **Alpha 1**, which remains [available unchanged](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.1). They do not establish Alpha 2 recovery qualification.

On KGP's Intel BE200 (`8086:272B`) system, initial testing reached scanning, infrastructure authentication/association, key installation, DHCP/IPv4 and bidirectional Internet traffic in **802.11ac** operation. The Alpha 1 candidate was **connected immediately after login**. KGP reports **four successful Wi-Fi OFF → ON cycles**, each with auto-join, and **immediate reconnection after one manual system Sleep/Wake**. These are **user-observed results**, not automatically repeated tests or long-term qualification.

A subsequent read-only capture confirms the selected executable UUID, the BE200 → AirportItlwm → infrastructure interface binding, actual **DMA Mode 2**, active MSI-X, RUN state and zero sampled transport fault/error fields. Its bounded history does not independently reconstruct every manual cycle; zero boundary faults do not exclude all brief transients. The earlier sporadic OFF/ON failure did not recur in the four reported cycles. Longer repeated-cycle, duration and broader hardware/mode testing remain open.

KGP's Internet connection is nominally **80/30 Mbps**. The illustrated Internet Speedtest reaches **79.5/28.5 Mbps**, ping 8 ms and jitter 1 ms; packet loss is not reported in that result. This is a single WAN measurement, not a stable LAN performance reference. **Local LAN throughput is being investigated separately.** A displayed **867 Mbps PHY rate is not measured data throughput**.

This Alpha does not establish BE201 support, Wi-Fi 7/EHT, MLO, 6 GHz, full AWDL, AirDrop or Auto Unlock. Existing AX201/AX210/AX211 operation is not newly qualified by this BE200 test. Bluetooth is provided by the separately identified stack below, not by the Wi-Fi kext.

**Bluetooth setup used during these BE200 tests:** [BlueToolFixup.kext v2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases) by acidanthera, together with [IntelBluetoothFirmware v2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1) from lshbluesky. The Intel USB Bluetooth controller was identified as **8087:0036**, and both Magic Keyboard and Magic Mouse were connected during testing.

### Frozen tested binary and corresponding source

- Source used to build the tested executable: [`a10a32bf296d6b90da1b3390b3ef4459f9d207fc`](https://github.com/kgp-macPro/AirportItlwm-Tahoe/commit/a10a32bf296d6b90da1b3390b3ef4459f9d207fc).
- Public complete source: [`be200-alpha` branch](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/be200-alpha) / [`v1.2.0-be200-alpha.2` tag](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/v1.2.0-be200-alpha.2), including pinned dependencies, SDK declaration change, licenses and portable validation. Publication-only tools/documentation preserve the tested production inputs; [build instructions](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.2.0-be200-alpha.2/docs/be200-alpha/BUILD.md) explain that correspondence.
- Executable UUID: `E4414AAB-1D58-3E40-93A5-6838D3CDCD20`.
- Executable SHA-256: `8e4a38121b06941630afa98728b245cbaeec813c14139c46715dc44723461f63`.
- Frozen internal marker: `AirportItlwm-Tahoe-BE200-GL92-PRIVATE-P8R1`. The original bundle name `AirportItlwm.kext`, bundle identifier and internal version `1.0.0` are unchanged; **1.2.0 BE200 Alpha 2 is the public release identity**, not rewritten binary metadata.

The port selectively reuses [HuajiFruit/AirportBZ](https://github.com/HuajiFruit/AirportBZ/tree/0ccab1627764406861fb9eb0a06ca36e31b4440f) hardware/firmware implementations, with OpenIntelWireless, Dexter, laobamac, Intel firmware, MacKernelSDK and Apple reference credits/notices retained. AirportBZ's native frontend/complete DMA allocator were not transplanted. See [source and firmware provenance](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.2.0-be200-alpha.2/docs/BE200_PROVENANCE.md).

<details>
<summary>BE200 screenshots — device, connection, DHCP, firmware, Internet, Bluetooth and test environment</summary>

1. AppleVTD visible in IORegistryExplorer.

![AppleVTD visible in IORegistryExplorer.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/01-applevtd.png)

2. PCI device 8086:272B bound to AirportItlwm.

![PCI device 8086:272B bound to AirportItlwm.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/02-hardware.png)

3. Wi-Fi menu showing the selected network.

![Wi-Fi menu showing the selected network.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/03-wifi-menu.png)

4. Wi-Fi settings showing Connected.

![Wi-Fi settings showing Connected.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/04-connected.png)

5. DHCP configuration with an IPv4 address and router.

![DHCP configuration with an IPv4 address and router.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/05-dhcp.png)

6. Firmware 92.67ce4588.0 and an 802.11ac connection on 5 GHz / 80 MHz. The displayed 867 Mbps is the PHY rate, not measured payload throughput.

![Firmware 92.67ce4588.0 and an 802.11ac connection on 5 GHz / 80 MHz. The displayed 867 Mbps is the PHY rate, not measured payload throughput.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/06-firmware.png)

7. AirportItlwm infrastructure interface in IORegistryExplorer.

![AirportItlwm infrastructure interface in IORegistryExplorer.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/07-interface.png)

8. Internet Speedtest: 79.5 Mbps download and 28.5 Mbps upload, ping 8 ms, jitter 1 ms. Packet loss is not reported in this result.

![Internet Speedtest: 79.5 Mbps download and 28.5 Mbps upload, ping 8 ms, jitter 1 ms. Packet loss is not reported in this result.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/08-internet-speedtest.png)

9. Magic Keyboard and Magic Mouse connected using BlueToolFixup.kext v2.7.2 by acidanthera and IntelBluetoothFirmware v2.5.1 from lshbluesky; Intel USB Bluetooth controller 8087:0036.

![Magic Keyboard and Magic Mouse connected using BlueToolFixup.kext v2.7.2 by acidanthera and IntelBluetoothFirmware v2.5.1 from lshbluesky; Intel USB Bluetooth controller 8087:0036.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/09-bluetooth.png)

10. macOS Tahoe 26.7.2 (build 25G332), the operating system used during these BE200 tests.

![macOS Tahoe 26.7.2 (build 25G332), the operating system used during these BE200 tests.](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/Assets/BE200-Alpha/10-macos-version.png)

</details>
