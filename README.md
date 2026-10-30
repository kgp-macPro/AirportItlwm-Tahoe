# AirportItlwm-Tahoe 1.2.0 BE200 Alpha 1 — source branch

This separate source branch contains the BE200 GL/API92 port with the restored Ventura legacy Apple frontend. It is experimental, not the qualified 1.0.0 release or the separate ZC28 1.1.0 Beta campaign.

**Required: Retro-OCLP-CustoMac root patching.** This BE200 alpha uses the restored Ventura legacy wireless frameworks and requires the corresponding root patches provided by [Retro-OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac).

**Test environment: macOS Tahoe 26.7.2 (build 25G332), x86_64, with AppleVTD enabled.**

[Alpha release and tested download](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.1) · [Build instructions and pinned SDK](docs/be200-alpha/BUILD.md) · [Source/firmware provenance](docs/BE200_PROVENANCE.md) · [Main project and campaign](https://github.com/kgp-macPro/AirportItlwm-Tahoe#current-be200-alpha-testing-campaign)

The tested binary was built from `e1c50b1bbc2c80f78f4fd93d48e548c0eb58a338`; publication-only tool/documentation additions preserve its production inputs, checked by tools/verify_be200_release_source.py. KGP reports connection immediately after login, four successful OFF/ON cycles with autojoin, and immediate reconnection after one manual Sleep/Wake. These are scoped USER-OBSERVED results, not general hardware/duration qualification. Initial scanning, infrastructure connection, DHCP and Internet traffic were observed. Further performance optimization is planned. No Wi-Fi 7/MLO/6 GHz/BE201/full AWDL/Continuity qualification is claimed.

Historical private experiment notes remain as provenance; their old pending-test statements describe their dates, not the current Alpha status. Complete public results, prerequisites and ten screenshots are on the main README/release. Existing upstream, AirportBZ, laobamac, Intel firmware, SDK and Apple source notices remain in their original files. [GPL license](LICENSE).

**Bluetooth setup used during these BE200 tests:** [BlueToolFixup.kext v2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases) by acidanthera, together with [IntelBluetoothFirmware v2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1) from lshbluesky. The Intel USB Bluetooth controller was identified as **8087:0036**, and both Magic Keyboard and Magic Mouse were connected during testing.
