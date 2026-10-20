<p align="center">
  <img src="Assets/AirportItlwm-Tahoe.png"
       alt="AirportItlwm-Tahoe"
       width="100%">
</p>

# AirportItlwm-Tahoe

[Current 1.1.0 Beta comparison campaign](#current-comparison-campaign-airportitlwm-tahoe-110-beta)

## Project overview and qualified 1.0.0 release

**Intel Wi-Fi on macOS Tahoe with AppleVTD/system IOMapper support, active AWDL/P2P integration, bidirectional AirPlay and Screen Mirroring — physically qualified on Intel AX210.**

AirportItlwm-Tahoe is a separate Intel Wi-Fi driver based on AirportItlwm. It provides the project’s qualified OpenCore configuration with AppleVTD available, using the restored Ventura legacy wireless environment supplied by **[OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac)**. The installed bundle remains **`AirportItlwm.kext`**.

**AirportItlwm-Tahoe 1.0.0 is release-qualified.** The same frozen binary passed both release gates on Tahoe 26.6.2. No rebuild occurred between AppleVTD, conventional comparison, and additional 26.7 tests. Intel AX210 / PCI `8086:2725` is the physically qualified reference adapter; other Intel devices and platforms are not qualified merely because upstream source contains support for them.

[Installation](INSTALL.md) · [Release notes](RELEASE_NOTES_1.0.0.md) · [Qualification](docs/QUALIFICATION.md) · [Build](docs/BUILD.md) · [Source repository](https://github.com/kgp-macPro/AirportItlwm-Tahoe)

## Project and framework responsibilities

OCLP-CustoMac supplies/restores the Tahoe Modern Wireless / Apple framework environment used by KGP’s validated setup. AirportItlwm-Tahoe supplies the separate Intel driver, mapper-backed packet/command handling, reset recovery and narrow AWDL interface baseline.

AirportItlwm-Tahoe is not built into OCLP-CustoMac and does not replace required root patches. OCLP-CustoMac is not the author of the AirportItlwm AppleVTD changes. This is a Ventura legacy `IO80211FamilyLegacy` driver target, not a merger with the Sonoma V2/Skywalk implementation.

## AppleVTD and conventional configurations

The AppleVTD-capable qualification used OpenCore `Kernel → Quirks → DisableIoMapper=false`, together with captured active AppleVTD runtime state. The conventional comparison used the established mapper-disabling configuration (`DisableIoMapper=true`), referred to internally as “Normal Mode.” That is project shorthand, not an official Apple/macOS/OpenCore mode. Configuration settings are KGP-controlled test inputs; `false` alone does not prove AppleVTD is active, and `true` does not prove that all devices in the machine operate without IOMMU translation.

The mapper-aware driver path uses prepared DMA backing and generated device addresses. Its purpose is to allow the qualified Intel setup while keeping the macOS AppleVTD/system-IOMapper environment available. It does not turn an OpenCore quirk into a machine-wide IOMMU guarantee. See [configuration and verification](INSTALL.md) and [OpenCore’s quirk documentation](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/Configuration.tex).

## Physically validated configuration

| Tested environment | Result | Evidence basis |
|---|---|---|
| Tahoe 26.6.2 / 25G83, AppleVTD, `DisableIoMapper=false` | FULL PHYSICAL + RUNTIME PASS; release gate | Retained runtime captures plus KGP physical qualification |
| Tahoe 26.6.2 / 25G83, conventional comparison, `DisableIoMapper=true` | FULL PHYSICAL PASS; release gate | KGP-controlled / USER-OBSERVED qualification |
| Tahoe 26.7 / 25G229, AppleVTD | Additional scoped compatibility PASS: Wi-Fi, power cycles, reconnect, AWDL/P2P integration, AirPlay and Screen Mirroring | Retained captures plus KGP observations; **not** a second full release qualification |

Reference hardware: **Intel AX210, PCI 8086:2725**, x86_64 Custom Mac, with the restored Ventura legacy wireless ABI under Tahoe. Qualification applies to this hardware/framework/configuration combination, not every Hackintosh, Intel adapter, or upstream target. The [qualification record](docs/QUALIFICATION.md) separates captured facts from KGP’s physical observations.

## What was tested

- Cold boot and infrastructure authentication/association; DHCP/IPv4 and source/interface-bound traffic.
- Repeated Wi-Fi OFF → ON recovery through System Settings and the menu bar.
- Forget Network → reconnect and Sleep/Wake within the qualified Tahoe 26.6.2 scope.
- Active `awdl0`; registered/matched/active `IO80211P2PInterface`; AirLink role and `wifip2pd` integration.
- Sampled real IPv6/mDNS traffic on `awdl0`, including AirPlay/RAOP/P2P service records.
- Bidirectional AirPlay and bidirectional Screen Mirroring.

**RUNTIME-PROVEN:** retained AppleVTD captures establish loaded identity, registry/interface state, the first controlled power-cycle recovery and bound traffic. **USER-OBSERVED:** the conventional comparison, additional cycles, reconnect/Sleep/Wake, bidirectional features and separately reported packet samples. This distinction does not reopen the completed release gates.

**AirPlay / Screen Mirroring:** On some Tahoe systems, availability of these Apple integration features can also depend on SMBIOS, graphics configuration and framework-level feature gating rather than the Wi-Fi driver alone. If these features do not work despite otherwise functional Wi-Fi/AWDL, see [FeatureUnlock-Tahoe](https://github.com/kgp-macPro/FeatureUnlock-Tahoe). It is a separate project, not a Wi-Fi requirement or an AirportItlwm-Tahoe dependency; feature unavailability alone does not establish a Wi-Fi/AWDL or AppleVTD failure.

## AWDL/P2P scope and future work

Active `awdl0`, Apple P2P integration and the tested features do not establish complete AWDL or Apple Continuity restoration. Reported packet samples had no kernel drops; that is a sample result, not an unrestricted packet-loss guarantee.

**Important scope note:** This kext does **not** claim to restore AirDrop, Continuity Camera, or Personal Hotspot. These features, and Intel BE200 support, remain future development gates. Broader post-1.0.0 development resumed in October 2026; the current AirportItlwm-Tahoe 1.1.0 Beta preparation focuses on independent AX201/AX210/AX211 comparisons, sustained-load behavior and the TX/RX balance. This does not imply a promised feature order or completion date for other future work.

## Current KGP EFI distribution

The [current public KGP EFI distribution](https://www.insanelymac.com/forum/files/file/1076-universal-efi-for-all-recent-macos-versions-including-sequoia-and-tahoe/) remains unchanged and continues to contain/recommend the established conventional Intel AirportItlwm configuration, including `DisableIoMapper=true` guidance where applicable. AirportItlwm-Tahoe is a separate, optional installation; it is not bundled into that EFI or OCLP-CustoMac. Existing users are not required to migrate.

## Installation with OpenCore

For the frozen **AirportItlwm-Tahoe 1.0.0 release**, use the official release asset’s **`AirportItlwm.kext`** with the framework environment described in [INSTALL.md](INSTALL.md).

For **AirportItlwm-Tahoe 1.1.0 Beta**, use the proposed single comparison ZIP once it has been reviewed and published. Follow [README_TESTS.txt](README_TESTS.txt) for the verified Desktop setup action, manual replacement of an existing installation, user-controlled reboot and rollback. The older ZC4 forum binary is historical.

Keep the bundle, executable and identifier unchanged; use only one AirportItlwm variant for the device. Preserve a working recovery setup. This project does not install itself, configure the bootloader, or provide a substitute for required OCLP root patches.

## Build from this source

This repository contains the complete source corresponding to the frozen **AirportItlwm-Tahoe 1.0.0 release**. The beta implementations are separate frozen source snapshots in the comparison package, including the exact vendored kernel SDK and generated firmware source used for the KGP binaries. They are not merged into this root tree. Use their included source/build maps; rebuilding this root tree does not produce ZC28. The earlier ZC4 source offer remains in the historical announcement.

Acquire the pinned standalone MacKernelSDK dependency, then build **only** `AirportItlwm-Tahoe / Release / x86_64` using [BUILD.md](docs/BUILD.md). The supported `fw_gen` dependency generates firmware source during a source build.

High Sierra, Mojave, Catalina, Big Sur, Monterey, Ventura, Sonoma 14.0 and Sonoma 14.4 targets remain preserved. They are not additional 1.0.0 qualified products.

## Frozen 1.0.0 release identity

| Field | Frozen 1.0.0 release identity |
|---|---|
| Bundle / executable | `AirportItlwm.kext` / `AirportItlwm` |
| Identifier | `com.zxystd.AirportItlwm` |
| Short / bundle version | `1.0.0` / `1.0.0` |
| Target / configuration / architecture | `AirportItlwm-Tahoe` / `Release` / `x86_64` |
| LC_UUID | `7039DFF1-232B-3697-8E6E-8EE505044F28` |
| Executable SHA-256 | `1202ec9bf855e944f59cc3742a90c0cf1a0a292642dfe7ed8d091a1a9c435811` |
| Info.plist SHA-256 | `05f027cc5978593c3542942488f27537fe5fb04c48b3817d7ec5025d3bc1c288` |

The same bytes serve the AppleVTD and conventional configurations. Verify the release executable using `shasum -a 256` and `dwarfdump --uuid`; the bundle name or displayed version alone does not establish loaded identity. The release package is unsigned, matching the qualified artifact.

## CI and the official release

CI checks source/target/dependency consistency, host ownership models and a hosted Xcode build. Every uploaded artifact is labeled **CI BUILD — NOT PHYSICALLY VALIDATED**. A green CI result is not physical qualification and a CI-built kext is not a substitute for the frozen 1.0.0 release binary. The qualified release used Xcode 26.5; the separately defined hosted validation toolchain is documented in [BUILD.md](docs/BUILD.md).

## Lineage and documentation

OpenIntelWireless / itlwm → pinned Dexter source → mapper-backed RX/TX/commands and deferred queue drain → narrow NKN direct VIF attach/publication → checked PC1 power-cycle recovery → dedicated legacy Tahoe target. [Technical lineage](docs/TECHNICAL_LINEAGE.md) records exact pins and evidence boundaries.

- [Installation and runtime identity](INSTALL.md)
- [Release notes](RELEASE_NOTES_1.0.0.md)
- [Qualification and limitations of the evidence](docs/QUALIFICATION.md)
- [Build and CI](docs/BUILD.md)
- [Credits and AI assistance](CREDITS.md)
- [Preserved upstream README](docs/UPSTREAM_README.md) and [license](LICENSE)

## Current comparison campaign: AirportItlwm-Tahoe 1.1.0 Beta

**Experimental comparison prerelease.** The qualified 1.0.0 release remains separate; stable 1.1.0 is not yet released.

This is **one public comparison campaign** for AX201, AX210 and AX211 owners on Intel-based macOS Tahoe 26.x. **ZC28 is the starting candidate.** Frozen ZC8, ZC19, ZC22 and ZC26 are optional historical comparisons within the same campaign; unchanged **laobamac53 is a separately attributed external control**, not KGP code.

The prerelease tag is **[`v1.1.0-beta.1`](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.1.0-beta.1)**. Its one complete download is **`AirportItlwm-Tahoe-1.1.0-Beta.zip`**: ready bundles, exact corresponding source/dependency/license enclosures, Desktop setup, the unchanged physically used r7 runner and seven original reference ZIPs. **[Download the complete beta ZIP](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v1.1.0-beta.1/AirportItlwm-Tahoe-1.1.0-Beta.zip).**

## Why this comparison beta

Earlier strong AX201 reports from **[@netiave on tonymacx86](https://www.tonymacx86.com/members/1548699/) / @funcio on InsanelyMac** (one tester) and AX211 results from **Luiz ([@LuizMeliga on tonymacx86](https://www.tonymacx86.com/members/1658846/))** motivated the attempt to combine fast TCP TX with roughly 600 Mbps TCP RX on KGP's AX210. Their earlier reports are valuable historical evidence, **not ZC28 qualification**.

TX-command/memory work brought normal AX210 TCP TX to roughly 480–490 Mbps; later work addressed publication, RX ownership, queues and lifecycle safety. A System Profiler interaction affected TX measurements, and replacing the wired M1 peer's Realtek RTL8156 adapter with ASIX AX88179B substantially changed RX results. Those cohorts stay separate; neither finding proves a universal card/adapter defect. ZC28 builds on ZC26, grouping software-capacity wakes during IWX TX service while retaining hardware-completion wakes. Useful progress and remaining slow/lossy states are both documented.

This is a deliberate pause in local optimisation to gather independent system/AP/network comparisons. The [full campaign narrative](docs/beta/FORUM_IM_TM_DRAFT.txt), with [IM](docs/beta/FORUM_IM_DRAFT.txt) and [TM](docs/beta/FORUM_TM_DRAFT.txt) variants, uses selected coherent three-round campaign means. The tables below instead show historical best **individual whole-test** results; neither presentation combines different attempts into a claimed stable configuration.

## Hardware coverage — qualification is narrower than source matching

| Group | Current evidence and scope |
| --- | --- |
| AX210 / 8086:2725 | ZC28 physically tested in actual mapped Mode2 and conventional Mode1; frozen 1.0.0 has its separate AX210 qualification. Repeat/long-run/sleep-wake limits remain |
| AX201; AX211 | Earlier @netiave/@funcio AX201 and Luiz AX211 results motivated this beta. AX201 remains conventional-only. Luiz's historical AppleVTD-active result is not positive proof of numerical Mode2. Neither is yet ZC28-qualified |
| Additional IWX source candidates | AX200, selected AX101/AX203/AX411 and named Killer 1650/1675/1690 variants; selected AC 9461/9462/9560/Killer 1550 variants. Actual PCI/MAC/RF/subdevice selection and enclosed firmware are required; these are **awaiting physical confirmation**, not blanket mapped support |
| Separate upstream backend paths | IWM: 3160/3165/3168, 7260/7265, 8260/8265, 9260 and selected 9461/9462/9560. IWN: 4965,5100/5150/5300/5350,100/105/130/135/1000/1030,2200/2230,6005/6030/6050/6150/6200/6235/6300. They do not enter the IWX-specific service/DMA paths; no same-fix or new qualification claim |
| Table-only / excluded or future | AX221/AX231 and other MA/SnJ/QnJ entries can select firmware absent from this frozen package; table presence is not usable support. BE200 remains future work |

The [source-grounded hardware overview](docs/beta/HARDWARE_COVERAGE.md) and [exact 208-row source ledger](docs/beta/HARDWARE_SOURCE_LEDGER.json) trace frozen ZC28, not the repository root's 1.0.0 code. Some changes are shared across IWX, others depend on family/mode; not every upstream card received the same implementation. This list **does not expand the unchanged AX201/AX210/AX211 comparison runner** or silently authorise another DMA route.

## Different prerequisites — choose the correct driver environment

**KGP AirportItlwm-Tahoe 1.1.0 Beta (ZC28 and KGP historical controls) requires all of:**
- **OCLP-CustoMac 3.0.3**, including **its Skywalk replacement** and restored legacy wireless frameworks.
- **AMFIPass.kext with `-amfipassbeta`**.
- The matching supported framework/boot configuration and a known-working recovery path.

**External laobamac53 uses a native Wi-Fi approach. For its Wi-Fi implementation it does not require OCLP, Skywalk replacement or `-amfipassbeta`.** Those KGP prerequisites must not be applied to this external control. Other hardware on a tester's system may have independent patch requirements.

**Replacing a kext does not undo existing root patches.** Record the ACTUAL installed/loaded frameworks, Skywalk replacement, root-patch/OCLP version and AMFIPass/boot configuration for every historical and new comparison. A native-capable driver is not proof that a particular run used an unpatched system. Historical saved loaded-framework evidence and unknowns are listed in the [environment register](docs/beta/FRAMEWORK_ENVIRONMENT_REGISTER.txt).

**Separate installation paths:** for KGP, update only the existing KGP bundle in the already matching patched environment; for external53, use its separately documented native-framework environment and ready `Laobamac/53/AirportItlwm.kext` bundle. See the two manual paths and rollback instructions in [README_TESTS.txt](README_TESTS.txt). Desktop setup applies/removes no patches, edits no EFI and changes no boot settings. Do not treat an external-driver swap on a patched KGP system as a clean native comparison.

**Start here: [README_TESTS.txt](README_TESTS.txt).** Download/extract the ZIP, run its one clearly named setup action, then follow the short existing-installation update, user-controlled reboot, ZC28 test and rollback procedure. Setup only creates Desktop files and opens the guide; it does not install/load a kext, edit EFI or change root patches/boot settings. All individual comparison commands, actual card/interface/server prompts, prerequisites, Ookla and return instructions are in that single guide; a complete historical matrix is optional.

Recent ZC28 conventional campaigns retained roughly 483–485Mbps TCP TX with useful TCP RX, but mapped TX dips, UDP loss bursts, upload-loaded latency tails and long-run/sleep-wake questions remain open. This is not a universal 600-Mbps RX or permanent-fix claim. See [reference results and limitations](docs/beta/REFERENCE_NOTES.txt) and the [complete three-conventional-campaign analysis](docs/beta/ZC28_CONVENTIONAL_ANALYSIS.txt), including all adverse observations.

**AX201 remains conventional-only even with system AppleVTD active.** Qualified AX210/AX211 mapped tests must report actual Mode2; conventional tests use Mode1. The runner's UUID/marker/mode/fault/epoch checks remain strict. External53 mapped/conventional labels describe the tester's declared boot configuration, not verified KGP numerical DMA/fault telemetry; unavailable is not zero. Its framework environment differs from the KGP legacy target.

**1.1.0 Beta names the public package, not a changed binary version.** KGP bundles retain their frozen internal 1.0.0 fields and individual UUID/hashes; the external bundle retains its own development fields. [Source/license mapping](docs/beta/SOURCE_LICENSE_MAP.txt) documents every snapshot and build. Root source still represents the frozen 1.0.0 implementation; beta source snapshots are supplied separately in the ZIP, not merged or rebuilt for packaging.

After independent qualification, **stable 1.1.0 is planned to become the main recommendation**, with this beta material retained in release history. Stable 1.1.0 is not yet released or qualified. Earlier [ZC4 testing and its existing source offer](docs/beta/ZC4_PUBLIC_BETA_HISTORY.md) are historical.

Discussion/results: [InsanelyMac project thread](https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/) and [tonymacx86 project thread](https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/). **New campaign-post permalinks will be added after KGP posts in both forums.**

## Submit beta results

Use ONE central GitHub results thread: **“AirportItlwm-Tahoe 1.1.0 Beta — Test results”**. **[Open the central results thread](https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2).** All testers submit each campaign as a comment in that single thread; start every comment with **card | candidate | AppleVTD configuration | actual DMA route (or unavailable)**. Use the short copyable comment template in [README_TESTS.txt](README_TESTS.txt). Do not open a results issue per tester or build; separate issues are for concrete bugs needing independent investigation and should link the relevant result comment.

For **each campaign**, attach **ONE Return-ZIP** containing the untouched runner-result ZIP, Ookla screenshot/result URL and short configuration notes. Include failures and slow runs. If Ookla could not run, say why. Record the actual patch/framework environment as well as card/system, macOS, candidate, AP and wired-server details. Keep archives as issue/forum attachments, not repository changes.

**IM and TM are equivalent alternatives:** use the respective shared beta discussion in the [InsanelyMac project thread](https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/) or [tonymacx86 project thread](https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/). New beta-post permalinks will be added after KGP posts there. No GitHub account is required for forum submission. Link an existing report instead of duplicating uploads; the same one-Return-ZIP-per-campaign rule and identifying first line apply there.

## Selected historical best results — original whole-test measurements

**Definition:** “best” is the highest whole-test receiver goodput from a completed 8-second TCP or 5-second UDP measurement, never a one-second peak or a short final interval. Internet download/upload use the highest recorded respective networkQuality result, with directional RPM from that SAME run/phase. Different rows may come from different runs; **these tables are not one combined campaign or a stability claim**.

**Cohort:** KGP's spare AX210, AC channel 36/80MHz, wired M1 MacBook with UGREEN ASIX AX88179B (`.33`) and the existing comparable protocol. Earlier Realtek/`.161` and Luiz's AX211/other-AP results are not pooled into these best values. The external53 native-capable implementation is distinct from KGP's legacy target; the actual saved framework evidence and unverified patch state are recorded separately in the environment register. Completed TCP tests in an interrupted campaign may qualify; interrupted measurements and the known source-mutated 43-result attempt do not. ZC19 conventional has no qualifying original here and is shown as N/A.

KGP mode comes from saved actual route proof; ZC8's legacy opaque record verifies the route but does not supply later numerical fault counters. **laobamac53 is the unchanged external control**: its mode labels are DECLARED AppleVTD boot configurations, not KGP numerical mode verification. N/A means unavailable, never zero. TCP retransmissions are sender counts, **not packet-loss percentages**. UDP rates and losses below belong to the same selected measurement; reported loss percentage is retained verbatim rather than recomputed from iperf's differing accounting windows/counters.

### TCP — local iperf3

**Conventional / external declared AppleVTD-off**

| Build | Card | Mode | TX/RX | P1/P4 | Best receiver Mbps | Retransmissions | Run |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ZC8 | AX210 spare | conventional (verified route) | TX | P1 | 255.404 | 0 | R29 R01 |
| ZC8 | AX210 spare | conventional (verified route) | RX | P1 | 589.551 | 1545 | R29 R01 |
| ZC8 | AX210 spare | conventional (verified route) | TX | P4 | 254.726 | 5028 | R29 R01 |
| ZC8 | AX210 spare | conventional (verified route) | RX | P4 | 568.187 | 2466 | R30 R03 |
| ZC19 | AX210 spare | conventional (no measurement) | TX | P1 | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | P1 | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | TX | P4 | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | P4 | N/A | N/A | N/A |
| ZC22 | AX210 spare | conventional (verified route) | TX | P1 | 486.941 | 155 | R05 R02 |
| ZC22 | AX210 spare | conventional (verified route) | RX | P1 | 527.025 | 2102 | R05 R02 |
| ZC22 | AX210 spare | conventional (verified route) | TX | P4 | 485.802 | 7774 | R05 R03 |
| ZC22 | AX210 spare | conventional (verified route) | RX | P4 | 508.776 | 2219 | R05 R01 |
| ZC26 | AX210 spare | conventional (verified route) | TX | P1 | 480.945 | 61 | R17 R01 |
| ZC26 | AX210 spare | conventional (verified route) | RX | P1 | 576.313 | 1958 | R13 R02 |
| ZC26 | AX210 spare | conventional (verified route) | TX | P4 | 481.688 | 3320 | R13 R02 |
| ZC26 | AX210 spare | conventional (verified route) | RX | P4 | 533.055 | 3645 | R12 R02 |
| ZC28 | AX210 spare | conventional (verified route) | TX | P1 | 493.843 | 150 | R24 R01 |
| ZC28 | AX210 spare | conventional (verified route) | RX | P1 | 561.162 | 1421 | R23 R03 |
| ZC28 | AX210 spare | conventional (verified route) | TX | P4 | 487.204 | 4970 | R23 R03 |
| ZC28 | AX210 spare | conventional (verified route) | RX | P4 | 524.779 | 3841 | R23 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | P1 | 481.578 | 315 | R01 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | P1 | 495.920 | 1651 | R01 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | P4 | 478.503 | 1504 | R01 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | P4 | 446.281 | 2516 | R01 R03 |

**Mapped / external declared AppleVTD-on**

| Build | Card | Mode | TX/RX | P1/P4 | Best receiver Mbps | Retransmissions | Run |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ZC8 | AX210 spare | mapped (verified route) | TX | P1 | 267.493 | 2494 | R32 R02 |
| ZC8 | AX210 spare | mapped (verified route) | RX | P1 | 595.845 | 2263 | R31 R03 |
| ZC8 | AX210 spare | mapped (verified route) | TX | P4 | 261.771 | 4367 | R32 R02 |
| ZC8 | AX210 spare | mapped (verified route) | RX | P4 | 575.116 | 2186 | R31 R02 |
| ZC19 | AX210 spare | mapped (verified route) | TX | P1 | 484.840 | 281 | R04 R01 |
| ZC19 | AX210 spare | mapped (verified route) | RX | P1 | 496.485 | 2144 | R04 R02 |
| ZC19 | AX210 spare | mapped (verified route) | TX | P4 | 487.820 | 1329 | R03 R03 |
| ZC19 | AX210 spare | mapped (verified route) | RX | P4 | 462.904 | 1828 | R03 R01 |
| ZC22 | AX210 spare | mapped (verified route) | TX | P1 | 482.435 | 117 | R08 R01 |
| ZC22 | AX210 spare | mapped (verified route) | RX | P1 | 566.089 | 1544 | R08 R01 |
| ZC22 | AX210 spare | mapped (verified route) | TX | P4 | 482.585 | 4498 | R08 R02 |
| ZC22 | AX210 spare | mapped (verified route) | RX | P4 | 527.258 | 1968 | R08 R01 |
| ZC26 | AX210 spare | mapped (verified route) | TX | P1 | 484.272 | 40 | R21 R01 |
| ZC26 | AX210 spare | mapped (verified route) | RX | P1 | 566.353 | 1833 | R19 R03 |
| ZC26 | AX210 spare | mapped (verified route) | TX | P4 | 484.220 | 4996 | R21 R02 |
| ZC26 | AX210 spare | mapped (verified route) | RX | P4 | 548.665 | 4201 | R19 R03 |
| ZC28 | AX210 spare | mapped (verified route) | TX | P1 | 483.201 | 422 | R28 R03 |
| ZC28 | AX210 spare | mapped (verified route) | RX | P1 | 557.562 | 1931 | R26 R02 |
| ZC28 | AX210 spare | mapped (verified route) | TX | P4 | 479.338 | 5395 | R28 R01 |
| ZC28 | AX210 spare | mapped (verified route) | RX | P4 | 505.145 | 2542 | R27 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | P1 | 485.027 | 178 | R02 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | P1 | 485.213 | 1387 | R02 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | P4 | 489.657 | 3310 | R02 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | P4 | 447.329 | 1166 | R02 R01 |

### UDP — local iperf3

**Conventional / external declared AppleVTD-off**

| Build | Card | Mode | TX/RX | Requested Mbps | Actual sender Mbps | Best receiver Mbps | Lost/total datagrams | Loss % | Run |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ZC8 | AX210 spare | conventional (verified route) | TX | 100 | 100.001 | 99.652 | 0/52086 | 0.000000 | R30 R02 |
| ZC8 | AX210 spare | conventional (verified route) | RX | 100 | 100.000 | 100.311 | 0/52246 | 0.000000 | R30 R03 |
| ZC8 | AX210 spare | conventional (verified route) | TX | 250 | 226.695 | 217.655 | 236/116155 | 0.199711 | R30 R02 |
| ZC8 | AX210 spare | conventional (verified route) | RX | 250 | 250.000 | 250.755 | 0/130641 | 0.000000 | R29 R01 |
| ZC8 | AX210 spare | conventional (verified route) | TX | 400 | 225.640 | 216.516 | 309/115576 | 0.262836 | R29 R03 |
| ZC8 | AX210 spare | conventional (verified route) | RX | 400 | 400.000 | 401.155 | 5/208947 | 0.002391 | R30 R02 |
| ZC8 | AX210 spare | conventional (verified route) | TX | 500 | 225.698 | 216.629 | 301/115580 | 0.256050 | R29 R03 |
| ZC8 | AX210 spare | conventional (verified route) | RX | 500 | 499.999 | 501.415 | 0/261259 | 0.000000 | R29 R01 |
| ZC8 | AX210 spare | conventional (verified route) | TX | 600 | 223.530 | 216.462 | 319/115512 | 0.273987 | R30 R03 |
| ZC8 | AX210 spare | conventional (verified route) | RX | 600 | 600.000 | 601.737 | 2/313616 | 0.000637 | R29 R02 |
| ZC19 | AX210 spare | conventional (no measurement) | TX | 100 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | 100 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | TX | 250 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | 250 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | TX | 400 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | 400 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | TX | 500 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | 500 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | TX | 600 | N/A | N/A | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | RX | 600 | N/A | N/A | N/A | N/A | N/A |
| ZC22 | AX210 spare | conventional (verified route) | TX | 100 | 99.998 | 99.648 | 0/52100 | 0.000000 | R05 R01 |
| ZC22 | AX210 spare | conventional (verified route) | RX | 100 | 100.001 | 100.321 | 0/52274 | 0.000000 | R05 R01 |
| ZC22 | AX210 spare | conventional (verified route) | TX | 250 | 249.996 | 249.103 | 0/130214 | 0.000000 | R05 R03 |
| ZC22 | AX210 spare | conventional (verified route) | RX | 250 | 250.000 | 250.712 | 18/130620 | 0.013772 | R05 R01 |
| ZC22 | AX210 spare | conventional (verified route) | TX | 400 | 399.998 | 398.481 | 0/208334 | 0.000000 | R05 R01 |
| ZC22 | AX210 spare | conventional (verified route) | RX | 400 | 400.000 | 401.139 | 0/208966 | 0.000000 | R05 R02 |
| ZC22 | AX210 spare | conventional (verified route) | TX | 500 | 463.401 | 452.451 | 485/239522 | 0.200796 | R05 R01 |
| ZC22 | AX210 spare | conventional (verified route) | RX | 500 | 499.999 | 501.474 | 0/261356 | 0.000000 | R05 R02 |
| ZC22 | AX210 spare | conventional (verified route) | TX | 600 | 464.907 | 453.784 | 545/240224 | 0.225059 | R05 R03 |
| ZC22 | AX210 spare | conventional (verified route) | RX | 600 | 599.999 | 601.661 | 12/313546 | 0.003822 | R05 R03 |
| ZC26 | AX210 spare | conventional (verified route) | TX | 100 | 100.000 | 99.663 | 0/52084 | 0.000000 | R12 R03 |
| ZC26 | AX210 spare | conventional (verified route) | RX | 100 | 100.000 | 100.327 | 0/52269 | 0.000000 | R11 R03 |
| ZC26 | AX210 spare | conventional (verified route) | TX | 250 | 250.000 | 249.116 | 0/130259 | 0.000000 | R14 R02 |
| ZC26 | AX210 spare | conventional (verified route) | RX | 250 | 250.000 | 250.793 | 0/130707 | 0.000000 | R12 R01 |
| ZC26 | AX210 spare | conventional (verified route) | TX | 400 | 399.995 | 398.550 | 0/208336 | 0.000000 | R12 R03 |
| ZC26 | AX210 spare | conventional (verified route) | RX | 400 | 400.000 | 401.180 | 0/208957 | 0.000000 | R16 R03 |
| ZC26 | AX210 spare | conventional (verified route) | TX | 500 | 459.451 | 450.833 | 470/237439 | 0.196295 | R13 R01 |
| ZC26 | AX210 spare | conventional (verified route) | RX | 500 | 500.000 | 501.588 | 0/261246 | 0.000000 | R10 R01 |
| ZC26 | AX210 spare | conventional (verified route) | TX | 600 | 457.969 | 446.988 | 587/236687 | 0.246084 | R16 R03 |
| ZC26 | AX210 spare | conventional (verified route) | RX | 600 | 600.000 | 601.768 | 0/313588 | 0.000000 | R18 R01 |
| ZC28 | AX210 spare | conventional (verified route) | TX | 100 | 100.000 | 99.652 | 0/52084 | 0.000000 | R23 R02 |
| ZC28 | AX210 spare | conventional (verified route) | RX | 100 | 100.001 | 100.333 | 0/52257 | 0.000000 | R25 R01 |
| ZC28 | AX210 spare | conventional (verified route) | TX | 250 | 250.000 | 249.108 | 0/130313 | 0.000000 | R24 R03 |
| ZC28 | AX210 spare | conventional (verified route) | RX | 250 | 250.001 | 250.727 | 0/130686 | 0.000000 | R25 R01 |
| ZC28 | AX210 spare | conventional (verified route) | TX | 400 | 399.998 | 398.474 | 0/208349 | 0.000000 | R24 R02 |
| ZC28 | AX210 spare | conventional (verified route) | RX | 400 | 400.000 | 401.185 | 0/209110 | 0.000000 | R24 R01 |
| ZC28 | AX210 spare | conventional (verified route) | TX | 500 | 466.349 | 456.915 | 67/241604 | 0.027580 | R25 R03 |
| ZC28 | AX210 spare | conventional (verified route) | RX | 500 | 499.998 | 501.438 | 10/261332 | 0.003824 | R25 R03 |
| ZC28 | AX210 spare | conventional (verified route) | TX | 600 | 467.147 | 457.357 | 89/241314 | 0.036570 | R24 R02 |
| ZC28 | AX210 spare | conventional (verified route) | RX | 600 | 600.000 | 601.847 | 0/313537 | 0.000000 | R23 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | 100 | 100.001 | 99.651 | 0/52088 | 0.000000 | R01 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | 100 | 100.001 | 100.310 | 0/52285 | 0.000000 | R01 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | 250 | 249.996 | 249.144 | 0/130273 | 0.000000 | R01 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | 250 | 250.000 | 250.728 | 0/130603 | 0.000000 | R01 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | 400 | 399.998 | 398.521 | 0/208350 | 0.000000 | R01 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | 400 | 400.001 | 401.075 | 0/208908 | 0.000000 | R01 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | 500 | 445.534 | 441.579 | 559/231575 | 0.240892 | R01 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | 500 | 499.998 | 501.221 | 16/261204 | 0.006120 | R01 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | TX | 600 | 453.126 | 445.482 | 609/234071 | 0.258020 | R01 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | RX | 600 | 600.000 | 601.534 | 32/313373 | 0.010195 | R01 R01 |

**Mapped / external declared AppleVTD-on**

| Build | Card | Mode | TX/RX | Requested Mbps | Actual sender Mbps | Best receiver Mbps | Lost/total datagrams | Loss % | Run |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ZC8 | AX210 spare | mapped (verified route) | TX | 100 | 100.001 | 99.650 | 0/52090 | 0.000000 | R31 R03 |
| ZC8 | AX210 spare | mapped (verified route) | RX | 100 | 100.000 | 100.333 | 0/52257 | 0.000000 | R33 R03 |
| ZC8 | AX210 spare | mapped (verified route) | TX | 250 | 231.984 | 223.238 | 239/119021 | 0.197718 | R31 R03 |
| ZC8 | AX210 spare | mapped (verified route) | RX | 250 | 250.000 | 250.738 | 0/130596 | 0.000000 | R31 R02 |
| ZC8 | AX210 spare | mapped (verified route) | TX | 400 | 231.685 | 222.614 | 327/118706 | 0.270971 | R31 R03 |
| ZC8 | AX210 spare | mapped (verified route) | RX | 400 | 400.000 | 401.130 | 0/208936 | 0.000000 | R33 R02 |
| ZC8 | AX210 spare | mapped (verified route) | TX | 500 | 230.997 | 222.044 | 333/118456 | 0.276778 | R31 R01 |
| ZC8 | AX210 spare | mapped (verified route) | RX | 500 | 500.000 | 501.427 | 0/261180 | 0.000000 | R32 R02 |
| ZC8 | AX210 spare | mapped (verified route) | TX | 600 | 231.036 | 221.784 | 328/118348 | 0.272504 | R33 R02 |
| ZC8 | AX210 spare | mapped (verified route) | RX | 600 | 599.997 | 601.715 | 0/313697 | 0.000000 | R31 R02 |
| ZC19 | AX210 spare | mapped (verified route) | TX | 100 | 99.999 | 99.650 | 0/52093 | 0.000000 | R03 R03 |
| ZC19 | AX210 spare | mapped (verified route) | RX | 100 | 100.000 | 100.323 | 0/52260 | 0.000000 | R03 R03 |
| ZC19 | AX210 spare | mapped (verified route) | TX | 250 | 249.997 | 249.116 | 0/130318 | 0.000000 | R04 R03 |
| ZC19 | AX210 spare | mapped (verified route) | RX | 250 | 249.999 | 250.797 | 0/130632 | 0.000000 | R03 R02 |
| ZC19 | AX210 spare | mapped (verified route) | TX | 400 | 399.998 | 398.481 | 0/208330 | 0.000000 | R03 R01 |
| ZC19 | AX210 spare | mapped (verified route) | RX | 400 | 400.000 | 401.151 | 0/208938 | 0.000000 | R04 R01 |
| ZC19 | AX210 spare | mapped (verified route) | TX | 500 | 460.203 | 452.758 | 198/237822 | 0.082543 | R04 R01 |
| ZC19 | AX210 spare | mapped (verified route) | RX | 500 | 499.998 | 501.472 | 0/261200 | 0.000000 | R04 R02 |
| ZC19 | AX210 spare | mapped (verified route) | TX | 600 | 458.338 | 451.506 | 211/236987 | 0.088328 | R04 R02 |
| ZC19 | AX210 spare | mapped (verified route) | RX | 600 | 599.998 | 601.681 | 0/313391 | 0.000000 | R03 R03 |
| ZC22 | AX210 spare | mapped (verified route) | TX | 100 | 100.000 | 99.661 | 0/52106 | 0.000000 | R06 R03 |
| ZC22 | AX210 spare | mapped (verified route) | RX | 100 | 100.001 | 100.315 | 0/52248 | 0.000000 | R09 R02 |
| ZC22 | AX210 spare | mapped (verified route) | TX | 250 | 249.996 | 249.116 | 0/130230 | 0.000000 | R09 R02 |
| ZC22 | AX210 spare | mapped (verified route) | RX | 250 | 250.000 | 250.806 | 0/130702 | 0.000000 | R08 R01 |
| ZC22 | AX210 spare | mapped (verified route) | TX | 400 | 399.998 | 398.541 | 0/208375 | 0.000000 | R06 R02 |
| ZC22 | AX210 spare | mapped (verified route) | RX | 400 | 399.999 | 401.193 | 0/209160 | 0.000000 | R07 R02 |
| ZC22 | AX210 spare | mapped (verified route) | TX | 500 | 462.193 | 451.086 | 493/238855 | 0.204765 | R06 R01 |
| ZC22 | AX210 spare | mapped (verified route) | RX | 500 | 499.998 | 501.500 | 0/261338 | 0.000000 | R07 R02 |
| ZC22 | AX210 spare | mapped (verified route) | TX | 600 | 463.845 | 452.891 | 551/239888 | 0.227888 | R06 R01 |
| ZC22 | AX210 spare | mapped (verified route) | RX | 600 | 600.000 | 601.702 | 0/313390 | 0.000000 | R08 R03 |
| ZC26 | AX210 spare | mapped (verified route) | TX | 100 | 99.999 | 99.661 | 0/52085 | 0.000000 | R22 R01 |
| ZC26 | AX210 spare | mapped (verified route) | RX | 100 | 100.001 | 100.337 | 0/52261 | 0.000000 | R20 R01 |
| ZC26 | AX210 spare | mapped (verified route) | TX | 250 | 250.000 | 249.113 | 0/130320 | 0.000000 | R20 R02 |
| ZC26 | AX210 spare | mapped (verified route) | RX | 250 | 250.000 | 250.809 | 0/130692 | 0.000000 | R21 R02 |
| ZC26 | AX210 spare | mapped (verified route) | TX | 400 | 399.998 | 398.529 | 0/208354 | 0.000000 | R22 R03 |
| ZC26 | AX210 spare | mapped (verified route) | RX | 400 | 400.000 | 401.170 | 0/209028 | 0.000000 | R20 R02 |
| ZC26 | AX210 spare | mapped (verified route) | TX | 500 | 464.716 | 453.459 | 509/240176 | 0.210211 | R19 R02 |
| ZC26 | AX210 spare | mapped (verified route) | RX | 500 | 500.001 | 501.469 | 0/261193 | 0.000000 | R20 R01 |
| ZC26 | AX210 spare | mapped (verified route) | TX | 600 | 463.027 | 451.421 | 595/239250 | 0.246695 | R21 R02 |
| ZC26 | AX210 spare | mapped (verified route) | RX | 600 | 599.999 | 601.741 | 0/313617 | 0.000000 | R19 R03 |
| ZC28 | AX210 spare | mapped (verified route) | TX | 100 | 100.000 | 99.651 | 0/52106 | 0.000000 | R28 R02 |
| ZC28 | AX210 spare | mapped (verified route) | RX | 100 | 100.001 | 100.322 | 0/52259 | 0.000000 | R26 R01 |
| ZC28 | AX210 spare | mapped (verified route) | TX | 250 | 249.999 | 249.112 | 0/130330 | 0.000000 | R26 R02 |
| ZC28 | AX210 spare | mapped (verified route) | RX | 250 | 250.001 | 250.730 | 0/130657 | 0.000000 | R28 R01 |
| ZC28 | AX210 spare | mapped (verified route) | TX | 400 | 399.998 | 398.545 | 0/208334 | 0.000000 | R28 R03 |
| ZC28 | AX210 spare | mapped (verified route) | RX | 400 | 399.999 | 401.166 | 0/209087 | 0.000000 | R28 R03 |
| ZC28 | AX210 spare | mapped (verified route) | TX | 500 | 451.454 | 445.565 | 84/233292 | 0.035722 | R28 R01 |
| ZC28 | AX210 spare | mapped (verified route) | RX | 500 | 500.000 | 501.431 | 13/261374 | 0.004970 | R27 R01 |
| ZC28 | AX210 spare | mapped (verified route) | TX | 600 | 457.105 | 450.357 | 88/236173 | 0.036945 | R27 R03 |
| ZC28 | AX210 spare | mapped (verified route) | RX | 600 | 600.000 | 601.662 | 6/313608 | 0.001910 | R26 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | 100 | 99.999 | 99.649 | 0/52091 | 0.000000 | R02 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | 100 | 100.001 | 100.314 | 0/52252 | 0.000000 | R02 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | 250 | 249.996 | 249.112 | 0/130256 | 0.000000 | R02 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | 250 | 250.001 | 250.727 | 0/130683 | 0.000000 | R02 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | 400 | 399.993 | 398.442 | 0/208327 | 0.000000 | R02 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | 400 | 399.999 | 401.076 | 0/208900 | 0.000000 | R02 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | 500 | 455.998 | 448.221 | 575/235745 | 0.241879 | R02 R02 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | 500 | 500.001 | 501.378 | 22/261372 | 0.008410 | R02 R01 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | TX | 600 | 457.817 | 450.155 | 636/236762 | 0.266534 | R02 R03 |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | RX | 600 | 599.998 | 601.617 | 52/313410 | 0.016578 | R02 R01 |

### Online networkQuality — Internet, separate from LAN iperf3

**Conventional / external declared AppleVTD-off**

| Build | Card | Mode | Download/upload | Best measured Mbps | Associated responsiveness RPM | Run |
| --- | --- | --- | --- | --- | --- | --- |
| ZC8 | AX210 spare | conventional (verified route) | Download | 76.557 | 475.124 | R29 online |
| ZC8 | AX210 spare | conventional (verified route) | Upload | 42.385 | 38.932 | R30 online |
| ZC19 | AX210 spare | conventional (no measurement) | Download | N/A | N/A | N/A |
| ZC19 | AX210 spare | conventional (no measurement) | Upload | N/A | N/A | N/A |
| ZC22 | AX210 spare | conventional (verified route) | Download | 74.789 | 271.959 | R05 online |
| ZC22 | AX210 spare | conventional (verified route) | Upload | 38.630 | 72.217 | R05 online |
| ZC26 | AX210 spare | conventional (verified route) | Download | 78.582 | 417.191 | R11 online |
| ZC26 | AX210 spare | conventional (verified route) | Upload | 44.608 | 50.806 | R15 online |
| ZC28 | AX210 spare | conventional (verified route) | Download | 75.765 | 250.143 | R24 online |
| ZC28 | AX210 spare | conventional (verified route) | Upload | 40.301 | 19.066 | R24 online |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | Download | 76.638 | 351.566 | R01 online |
| laobamac53 (external) | AX210 spare | declared AppleVTD-off/conventional | Upload | 34.301 | 26.089 | R01 online |

**Mapped / external declared AppleVTD-on**

| Build | Card | Mode | Download/upload | Best measured Mbps | Associated responsiveness RPM | Run |
| --- | --- | --- | --- | --- | --- | --- |
| ZC8 | AX210 spare | mapped (verified route) | Download | 75.334 | 263.348 | R31 online |
| ZC8 | AX210 spare | mapped (verified route) | Upload | 37.116 | 47.763 | R32 online |
| ZC19 | AX210 spare | mapped (verified route) | Download | 76.972 | 426.060 | R03 online |
| ZC19 | AX210 spare | mapped (verified route) | Upload | 38.537 | 27.501 | R03 online |
| ZC22 | AX210 spare | mapped (verified route) | Download | 74.907 | 342.113 | R09 online |
| ZC22 | AX210 spare | mapped (verified route) | Upload | 43.863 | 57.626 | R06 online |
| ZC26 | AX210 spare | mapped (verified route) | Download | 76.734 | 393.726 | R21 online |
| ZC26 | AX210 spare | mapped (verified route) | Upload | 40.813 | 65.341 | R20 online |
| ZC28 | AX210 spare | mapped (verified route) | Download | 78.521 | 216.613 | R28 online |
| ZC28 | AX210 spare | mapped (verified route) | Upload | 41.072 | 33.092 | R28 online |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | Download | 76.594 | 391.895 | R02 online |
| laobamac53 (external) | AX210 spare | declared AppleVTD-on/mapped | Upload | 37.091 | 37.571 | R02 online |

**Adverse findings remain:** the severe earlier ZC26 conventional mixed TCP/UDP collapse, ZC28 mapped TX P1 247.018Mbps (one full interval 32.494Mbps), UDP loss bursts and sender-limited TX500/600 are not erased by maximum rows. ZC28's three conventional campaigns share one boot and do not prove permanent repair or long-run/sleep-wake stability. Online endpoints/times differ and upload-loaded tails remain long; best Internet capacity cannot qualify LAN performance or responsiveness. See the linked complete analysis/reference notes for the counterevidence. Selected raw-origin ZIPs and per-row source fields accompany the package; no individual logs are reproduced here.

#### Compact run references for the selected rows

Dates/times are from the original archive names (local CEST); `R01`/`R02`/`R03` after a reference means the round, and “online” means that archive's separate networkQuality stage. Only origins actually selected above are listed.

- **R01** — `LAOBAMAC_53_738D4C6_CONVENTIONAL_AX210_3X14_unverified_20261020_123708_tPD1WD.zip` (unchanged external control; declared DMA configuration only; actual framework evidence in the environment register).
- **R02** — `LAOBAMAC_53_738D4C6_MAPPED_AX210_3X14_unverified_20261020_122534_n2sdvy.zip` (unchanged external control; declared DMA configuration only; actual framework evidence in the environment register).
- **R03** — `ZC19_AX210_3X14_mapped_20261018_122836_sQ4ACw.zip`.
- **R04** — `ZC19_AX210_3X14_mapped_20261018_124218_w7JoER.zip`.
- **R05** — `ZC22_AX210_3X14_conventional_20261020_104551_E5jO9B.zip`.
- **R06** — `ZC22_AX210_3X14_mapped_20261019_164056_Jkx7Fy.zip`.
- **R07** — `ZC22_AX210_3X14_mapped_20261019_165553_mg0zW6.zip`.
- **R08** — `ZC22_AX210_3X14_mapped_20261019_171501_x79oza.zip`.
- **R09** — `ZC22_AX210_3X14_mapped_20261020_103118_D2jqOx.zip`.
- **R10** — `ZC26_AX210_3X14_conventional_20261020_005919_gYhHiF.zip`.
- **R11** — `ZC26_AX210_3X14_conventional_20261020_010624_GjpQ6c.zip`.
- **R12** — `ZC26_AX210_3X14_conventional_20261020_011439_shm8ZC.zip`.
- **R13** — `ZC26_AX210_3X14_conventional_20261020_020338_9q3Qg9.zip`.
- **R14** — `ZC26_AX210_3X14_conventional_20261020_021647_0AqQaa.zip`.
- **R15** — `ZC26_AX210_3X14_conventional_20261020_091811_BGiPKz.zip`.
- **R16** — `ZC26_AX210_3X14_conventional_20261020_095351_FdAI70.zip`.
- **R17** — `ZC26_AX210_3X14_conventional_20261020_100133_TKv0oy.zip`.
- **R18** — `ZC26_AX210_3X14_conventional_20261020_101357_L0KO0B.zip`.
- **R19** — `ZC26_AX210_3X14_mapped_20261020_002422_nwtUQN.zip`.
- **R20** — `ZC26_AX210_3X14_mapped_20261020_003212_oG8Lqp.zip`.
- **R21** — `ZC26_AX210_3X14_mapped_20261020_003906_qkntNm.zip`.
- **R22** — `ZC26_AX210_3X14_mapped_20261020_110027_MfOLxX.zip`.
- **R23** — `ZC28_AX210_3X14_conventional_20261020_164527_EJOvrp.zip`.
- **R24** — `ZC28_AX210_3X14_conventional_20261020_165222_nUUFOr.zip`.
- **R25** — `ZC28_AX210_3X14_conventional_20261020_170008_QsHMcG.zip`.
- **R26** — `ZC28_AX210_3X14_mapped_20261020_144057_7zXbzs.zip`.
- **R27** — `ZC28_AX210_3X14_mapped_20261020_144921_5lIpTl.zip`.
- **R28** — `ZC28_AX210_3X14_mapped_20261020_145825_7drjMn.zip`.
- **R29** — `ZC8_AX210_3X14_conventional_20261018_032335_brBhDj.zip`.
- **R30** — `ZC8_AX210_3X14_conventional_20261020_113857_n0Cnrb.zip`.
- **R31** — `ZC8_AX210_3X14_mapped_20261018_025805_S2ZwkD.zip`.
- **R32** — `ZC8_AX210_3X14_mapped_20261019_150738_kx2VLl.zip`.
- **R33** — `ZC8_AX210_3X14_mapped_20261020_112715_nRtmGO.zip`.

## Credits

### [KGP / kgp-macPro](https://github.com/kgp-macPro)

Project concept and direction, Tahoe integration, Intel Wi-Fi / AppleVTD / AWDL research, system integration, experimental design, physical testing and qualification, release maintenance and documentation.

### [zxystd / OpenIntelWireless](https://github.com/OpenIntelWireless/itlwm)

Original itlwm / AirportItlwm project, Intel Wi-Fi driver foundation and upstream source architecture on which AirportItlwm-Tahoe is based. [zxystd](https://github.com/zxystd) is credited with the original driver development.

### [DexterSLamb](https://github.com/DexterSLamb)

Source baseline and continued AirportItlwm development forming the direct upstream foundation used for the AirportItlwm-Tahoe implementation.

### [NorthKoreanNoodles / maddog860](https://github.com/maddog860/itlwm-Working-Ventura-Airplay)

Independent technical contribution and prior implementation of the Ventura direct AWDL VIF create / attach → alias → registerService approach. This specific approach was independently investigated, validated in the AirportItlwm-Tahoe environment and adopted narrowly for the AWDL/P2P interface-publication path; it is not authorship of AirportItlwm-Tahoe as a whole.

### [Mieze](https://github.com/Mieze) / [IntelLucy](https://github.com/Mieze/IntelLucy)

Important architectural prior art for Tahoe AppleVTD, mapper-aware DMA handling and packet-lifetime management. AirportItlwm-Tahoe does not claim copied IntelLucy source or co-development.

### [laobamac](https://github.com/laobamac/itlwm) — external comparison

The unchanged upstream build53 is a separately authored native-framework comparison. The package includes source at [738d4c61b5a64433b27c1ab09dba94916ab77d8d](https://github.com/laobamac/itlwm/commit/738d4c61b5a64433b27c1ab09dba94916ab77d8d), its pinned MacKernelSDK and original notices. Upstream CI artifact/source linkage is recorded; no laobamac code was merged into ZC28 and no independent rebuild is claimed.

### Earlier physical tester reports

- **[@netiave on tonymacx86](https://www.tonymacx86.com/members/1548699/) / @funcio on InsanelyMac** — the same AX201 tester; earlier conventional-route results helped motivate the comparison beta.
- **Luiz ([@LuizMeliga on tonymacx86](https://www.tonymacx86.com/members/1658846/))** — earlier AX211 campaigns demonstrated strong RX on his own system/network.

These acknowledgements concern historical reports, not testing or qualification of ZC28. No IM or GitHub account for Luiz, or additional tester identity, is implied.

### OpenAI ChatGPT

Research and architecture collaboration, source/evidence analysis, experimental strategy, runtime interpretation, safety review, independent source review and technical/publication documentation.

### OpenAI Codex CLI

Source implementation/integration work, local source/binary analysis, build and validation tooling, regression testing, evidence generation and release preparation under KGP direction and independent ChatGPT review.

### [OpenCore Legacy Patcher developers](https://github.com/dortania/OpenCore-Legacy-Patcher)

Modern Wireless restoration environment used to restore the Apple legacy wireless frameworks required by the validated Tahoe setup. AirportItlwm-Tahoe remains an independent Intel Wi-Fi project.

### [OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac)

Supplies the specific Tahoe Modern Wireless framework environment used for KGP’s physical qualification, but does not contain or implement the AirportItlwm-Tahoe driver changes.

AI-assisted work was performed under continuous human direction, physical testing, review and editorial control. No endorsement by OpenAI or any referenced developer/project is implied. Inherited licenses and upstream attribution remain intact. See [CREDITS.md](CREDITS.md).

## Reporting results

For this beta use the [Submit beta results](#submit-beta-results) procedure: ONE central GitHub results thread, **“AirportItlwm-Tahoe 1.1.0 Beta — Test results”** ([https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2](https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2)), with one campaign per comment; the shared IM/TM beta threads are equivalent alternatives. Link existing reports rather than uploading twice. Include the untouched runner ZIP, Ookla result and notes in one Return-ZIP per campaign, including failures and slow runs.

Separate issues are for concrete bugs needing independent investigation. Record card/PCI identity, macOS, exact candidate UUID/SHA, ACTUAL framework/patch environment, system AppleVTD setting and observed driver DMA route; unavailable is not zero. Existing debugging/forensic procedures are separate from the r7 throughput campaign and must not be run concurrently or used as pre-test preparation.
