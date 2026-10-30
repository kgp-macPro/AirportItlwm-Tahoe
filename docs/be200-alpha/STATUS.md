AirportItlwm-Tahoe 1.2.0 BE200 Alpha 1

**Required: Retro-OCLP-CustoMac root patching.** This BE200 alpha uses the restored Ventura legacy wireless frameworks and requires the corresponding root patches provided by [Retro-OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac). Keep the matching framework/Skywalk and AMFIPass/`-amfipassbeta` boot configuration; this is not a native, unpatched Wi-Fi driver.

**Test environment: macOS Tahoe 26.7.2 (build 25G332), x86_64, with AppleVTD enabled.**

**Further performance optimization is planned.** The current alpha focuses on initial functionality and recovery testing; its performance results are preliminary.

### Observed functionality and scope

On KGP's Intel BE200 (`8086:272B`) system, initial testing reached scanning, infrastructure authentication/association, key installation, DHCP/IPv4 and bidirectional Internet traffic in **802.11ac** operation. The current tested candidate was **connected immediately after login**. KGP reports **four successful Wi-Fi OFF → ON cycles**, each with auto-join, and **immediate reconnection after one manual system Sleep/Wake**. These are **user-observed results**, not automatically repeated tests or long-term qualification.

A subsequent read-only capture confirms the selected executable UUID, the BE200 → AirportItlwm → infrastructure interface binding, actual **DMA Mode 2**, active MSI-X, RUN state and zero sampled transport fault/error fields. Its bounded history does not independently reconstruct every manual cycle; zero boundary faults do not exclude all brief transients. The earlier sporadic OFF/ON failure did not recur in the four reported cycles. Longer repeated-cycle, duration and broader hardware/mode testing remain open.

KGP's Internet connection is nominally **80/30 Mbps**. The illustrated Internet Speedtest reaches **79.5/28.5 Mbps**, ping 8 ms and jitter 1 ms; packet loss is not reported in that result. This is a single WAN measurement, not a stable LAN performance reference. **Local LAN throughput is being investigated separately.** A displayed **867 Mbps PHY rate is not measured data throughput**.

This Alpha does not establish BE201 support, Wi-Fi 7/EHT, MLO, 6 GHz, full AWDL, AirDrop or Auto Unlock. Existing AX201/AX210/AX211 operation is not newly qualified by this BE200 test. Bluetooth is provided by the separately identified stack below, not by the Wi-Fi kext.

**Bluetooth setup used during these BE200 tests:** [BlueToolFixup.kext v2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases) by acidanthera, together with [IntelBluetoothFirmware v2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1) from lshbluesky. The Intel USB Bluetooth controller was identified as **8087:0036**, and both Magic Keyboard and Magic Mouse were connected during testing.

### Frozen tested binary and corresponding source

- Source used to build the tested executable: [`e1c50b1bbc2c80f78f4fd93d48e548c0eb58a338`](https://github.com/kgp-macPro/AirportItlwm-Tahoe/commit/e1c50b1bbc2c80f78f4fd93d48e548c0eb58a338).
- Public complete source: [`be200-alpha` branch](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/be200-alpha) / [`v1.2.0-be200-alpha.1` tag](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/v1.2.0-be200-alpha.1), including pinned dependencies, SDK declaration change, licenses and portable validation. Publication-only tools/documentation preserve the tested production inputs; [build instructions](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.2.0-be200-alpha.1/docs/be200-alpha/BUILD.md) explain that correspondence.
- Executable UUID: `3610B51E-B7CD-3822-94BD-61EA7C138D99`.
- Executable SHA-256: `5bfe3548f2aaf10d8d3b34bc979e7c1f8454dd2e32067d42adce45aefb99a880`.
- Frozen internal marker: `AirportItlwm-Tahoe-BE200-GL92-PRIVATE-P7`. The original bundle name `AirportItlwm.kext`, bundle identifier and internal version `1.0.0` are unchanged; **1.2.0 BE200 Alpha 1 is the public release identity**, not rewritten binary metadata.

The port selectively reuses [HuajiFruit/AirportBZ](https://github.com/HuajiFruit/AirportBZ/tree/0ccab1627764406861fb9eb0a06ca36e31b4440f) hardware/firmware implementations, with OpenIntelWireless, Dexter, laobamac, Intel firmware, MacKernelSDK and Apple reference credits/notices retained. AirportBZ's native frontend/complete DMA allocator were not transplanted. See [source and firmware provenance](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.2.0-be200-alpha.1/docs/BE200_PROVENANCE.md).
