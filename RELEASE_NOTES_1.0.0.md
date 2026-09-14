# AirportItlwm-Tahoe 1.0.0

**Intel Wi-Fi for macOS Tahoe with AppleVTD/system IOMapper support, active AWDL/P2P integration, AirPlay and Screen Mirroring.** Intel AX210 / PCI `8086:2725` is the physically qualified reference adapter for 1.0.0; this does not automatically qualify other Intel adapters. The distributed bundle remains `AirportItlwm.kext`; identifier `com.zxystd.AirportItlwm`; executable `AirportItlwm`; version `1.0.0`.

## Validated functionality

Cold boot and infrastructure association, DHCP/IPv4 and bound traffic, repeated Wi-Fi OFF/ON recovery, Forget Network/reconnect, and Sleep/Wake in the qualified Tahoe 26.6.2 scope. The release also qualifies active awdl0/P2P/AirLink/wifip2pd integration, sampled real awdl0 traffic, bidirectional AirPlay and bidirectional Screen Mirroring.

| Tested environment | Result | Evidence basis |
|---|---|---|
| Tahoe 26.6.2 / 25G83, AppleVTD, `DisableIoMapper=false` | FULL PHYSICAL + RUNTIME PASS; release gate | Retained runtime captures plus KGP physical qualification |
| Tahoe 26.6.2 / 25G83, conventional comparison, `DisableIoMapper=true` | FULL PHYSICAL PASS; release gate | KGP-controlled / USER-OBSERVED qualification |
| Tahoe 26.7 / 25G229, AppleVTD | Additional scoped compatibility PASS: Wi-Fi, power cycles, reconnect, AWDL/P2P integration, AirPlay and Screen Mirroring | Retained captures plus KGP observations; **not** a second full release qualification |

The AppleVTD-capable qualification used OpenCore `Kernel → Quirks → DisableIoMapper=false`, together with captured active AppleVTD runtime state. The conventional comparison used the established mapper-disabling configuration (`DisableIoMapper=true`), referred to internally as “Normal Mode.” That is project shorthand, not an official Apple/macOS/OpenCore mode. Configuration settings are KGP-controlled test inputs; `false` alone does not prove AppleVTD is active, and `true` does not prove that all devices in the machine operate without IOMMU translation.

**The exact same executable and bundle were tested without rebuilding between these configurations.** Retained AppleVTD captures are RUNTIME-PROVEN; the conventional comparison and additional physical feature/packet observations are USER-OBSERVED where separate raw files were not supplied. This is scoped AX210/platform qualification, not universal Intel compatibility.

## Implementation

The dedicated Tahoe target retains the Ventura legacy ABI and `IO80211FamilyLegacy`, with mapper-backed RX, ordinary TX and large commands; serialized deferred queue drain; narrow direct AWDL VIF attachment/publication; and checked power-cycle recovery. Uncertain old TX backing remains quarantined rather than recycled. Quarantine capacity is finite and checks remain fail-closed. Existing upstream target families remain in source.

## Requirements and distribution

[OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac) supplies/restores the Modern Wireless framework environment used in the validated setup. AirportItlwm-Tahoe is the separate Intel driver and does not replace that environment or its required root patches.

KGP’s current public EFI distribution remains unchanged and continues to contain/recommend the established conventional Intel AirportItlwm configuration, including `DisableIoMapper=true` guidance where applicable. AirportItlwm-Tahoe is a separate, optional installation; it is not bundled into that EFI or OCLP-CustoMac. Existing users are not required to migrate.

See [INSTALL.md](INSTALL.md). Use the official frozen release asset; uploaded CI builds are **CI BUILD — NOT PHYSICALLY VALIDATED** and have no physical-release status.

## Scope and future gates

**Important scope note:** This kext does **not** claim to restore AirDrop, Continuity Camera, or Personal Hotspot. These features, and Intel BE200 support, remain future development gates. Broader post-1.0.0 development is planned to resume starting in October 2026; this is a development plan, not a promised completion date or feature order.

awdl0 and tested AirPlay/Screen Mirroring do not establish complete AWDL or Apple Continuity support. The known `networksetup` association-report discrepancy is not a functional failure when contradicted by active interfaces, IP and successful bound traffic.

## Frozen identity

| Field | Frozen 1.0.0 release identity |
|---|---|
| Bundle / executable | `AirportItlwm.kext` / `AirportItlwm` |
| Identifier | `com.zxystd.AirportItlwm` |
| Short / bundle version | `1.0.0` / `1.0.0` |
| Target / configuration / architecture | `AirportItlwm-Tahoe` / `Release` / `x86_64` |
| LC_UUID | `7039DFF1-232B-3697-8E6E-8EE505044F28` |
| Executable SHA-256 | `1202ec9bf855e944f59cc3742a90c0cf1a0a292642dfe7ed8d091a1a9c435811` |
| Info.plist SHA-256 | `05f027cc5978593c3542942488f27537fe5fb04c48b3817d7ec5025d3bc1c288` |

## Attribution

Foundation: zxystd / OpenIntelWireless and DexterSLamb. Project development/research: KGP / kgp-macPro, ChatGPT by OpenAI and OpenAI Codex CLI under continuous human direction, physical testing, review and editorial control. References: NorthKoreanNoodles / maddog860 (independently published direct VIF attach/publication) and [Mieze](https://github.com/Mieze) / [IntelLucy](https://github.com/Mieze/IntelLucy) (mapper-aware DMA/lifetime prior art). The implementation remains source-native; reference authors are not claimed as co-developers or endorsers. See [CREDITS.md](CREDITS.md).
