<p align="center">
  <img src="Assets/AirportItlwm-Tahoe.png"
       alt="AirportItlwm-Tahoe"
       width="100%">
</p>

# AirportItlwm-Tahoe

[Current BE200 alpha testing campaign](#current-be200-alpha-testing-campaign) · [Optional 1.1.0 Beta reference comparisons](#optional-reference-comparisons-airportitlwm-tahoe-110-beta)

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

## Current BE200 alpha testing campaign

The current 1.2.0 Alpha is our primary KGP validation candidate for **AX201, AX210, AX211 and BE200** through the restored Ventura legacy wireless frontend. BE200 (`8086:272B`) has physical results; AX201/AX210/AX211 are explicitly invited to validate this shared build, not newly qualified by the BE200 measurements. **[Retro-OCLP-CustoMac root patching](https://github.com/kgp-macPro/OCLP-CustoMac) and the matching supported framework/boot environment are required.** Further performance optimization is planned; this is not Wi-Fi 7/MLO/6 GHz or broad hardware qualification.

[Current BE200 release, downloads, exact source/build identity and known issues](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.2) · [Shared validation testing](#validation-testing-ax201-ax210-ax211-and-be200) · [Central commands and tool setup](docs/benchmark/README.md).

Version-specific changes, measurements and recovery findings belong to the release notes. The main branch retains frozen 1.0.0 driver code; experimental source remains on the separate [be200-alpha branch](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/be200-alpha). Archived [initial observations and gallery context](docs/benchmark/BE200_OBSERVATIONS_ARCHIVE.md) preserve the previous narrative. The following original screenshots are historical functionality evidence, not a new candidate or stability qualification.

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

<a id="current-comparison-campaign-airportitlwm-tahoe-110-beta"></a>

## Optional reference comparisons: AirportItlwm-Tahoe 1.1.0 Beta

The **current 1.2.0 Alpha is the primary KGP test candidate for AX201, AX210, AX211 and BE200**. These frozen beta/ZC builds are optional reference comparisons and fallback versions, not a mandatory matrix. Within that historical campaign ZC28 remains its original starting point; ZC8/ZC19/ZC22/ZC26 are additional controls. **laobamac44/47/53 are optional external controls for AX201/AX210/AX211 only and do not support BE200.** The [frozen beta download](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.1.0-beta.1), Kext bytes, source snapshots, licenses and old r7 tool remain unchanged. Use the newer shared tool setup after beta setup.

[Shared validation instructions and result submission](#validation-testing-ax201-ax210-ax211-and-be200) · [Central test guide](docs/benchmark/README.md) · [Historical results below](#selected-historical-best-results--original-whole-test-measurements).

Earlier AX201/AX211 reports motivated the AX210 TX/RX investigation. The [retained motivation and source-derived hardware overview](docs/beta/COMPARISON_CONTEXT.md) distinguish physical evidence from source candidates and preserved upstream paths.

## Validation testing: AX201, AX210, AX211 and BE200

**[Open the central validation guide](docs/benchmark/README.md)** for all prerequisites, current profiles, complete commands, installation/rollback and return instructions. The common runner retains the original three rounds × 14 cases, 8-second TCP, 5-second UDP/1200-byte payload, 2-second gaps and final sequential networkQuality. Existing forensic/debugging capture remains separate.

### Prerequisites and separate installation paths

- **Current KGP Alpha (AX201/AX210/AX211/BE200):** [Retro-OCLP-CustoMac root patching](https://github.com/kgp-macPro/OCLP-CustoMac), restored Ventura legacy frameworks and the matching Skywalk/AMFIPass/`-amfipassbeta` boot environment.
- **KGP ZC beta/historical controls:** OCLP-CustoMac 3.0.3, its Skywalk replacement, AMFIPass.kext with `-amfipassbeta` and matching supported framework/boot configuration.
- **External laobamac44/47/53 (AX201/AX210/AX211 ONLY, no BE200):** native Wi-Fi implementation, without those OCLP/Skywalk/`-amfipassbeta` Wi-Fi prerequisites. A kext swap does not undo root patches. Record the ACTUAL framework/patch state for every comparison; native-capable does not mean an observed run was unpatched. See the [environment register](docs/beta/FRAMEWORK_ENVIRONMENT_REGISTER.txt).

Update/restore complete Kext bundles and matching boot configurations manually, then reboot. Setup installs only tools; it never changes EFI, root patches or driver loading. Existing source/license mapping and the [ZC4 historical source offer](docs/beta/ZC4_PUBLIC_BETA_HISTORY.md) remain available. Qualified stable 1.0.0 remains the main recommendation; a future stable 1.1.0 requires independent qualification, with beta material retained in release history.

### Current tools and setup

[**Download shared benchmark r11 (all profiles and setup)**](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r11.zip) · [ZIP SHA-256](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r11.zip.sha256) · [Runner/resource identities](tools/benchmark/RUNNER_IDENTITY.json).

Extract in Downloads on both Macs, then run:

```bash
/bin/bash "$HOME/Downloads/AirportItlwm-Shared-Benchmark-r11/SETUP_AirportItlwm_Benchmark.command"
```

This prepares `~/Desktop/AirportItlwm/TOOLS` and preserves differing files in timestamped BACKUPS; TESTS and unrelated files are untouched. **Run it after old beta setup**, which still installs r7. Verify `--version` reports `shared-2026-10-31-r11-public-be200`. Python 3.9+, existing iperf3 and Command Line Tools are required; no dependency is silently installed.

### Server and client commands

On the wired server Mac, manually turn Wi-Fi OFF, connect Ethernet and run:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_IPERF3_WIRED_SERVER.sh"
```

It asks for the actual wired interface and starts `iperf3 -s -B "$ip" -p 5201` using its checked current Ethernet IPv4. The client prompts for this address. After manually installing/rebooting with the matching extracted Alpha 2 bundle:

```bash
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" DEVICE_LABEL=BE200 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" BE200-ALPHA2 mapped
```

For AX validation use the SAME current Alpha bundle, after manual installation/reboot. Example requesting actual Mode 2 (substitute `AX201-ALPHA2` or `AX211-ALPHA2` for those systems, and `conventional` for an actual Mode 1 comparison):

```bash
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" AX210-ALPHA2 mapped
```

**Optional laobamac control is AX201/AX210/AX211-only; these builds do not support BE200.** After its separate native-environment installation/reboot:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 mapped
```

The last argument in that external call records DECLARED AppleVTD configuration; numerical KGP DMA/fault/epoch telemetry is unavailable. Existing Alpha 1/P7, private P8R1, AX/ZC and numbered laobamac calls retain their checks and paths. The [full guide](docs/benchmark/README.md) has Alpha 1, historical, conventional and override examples.

### AX201 validation commands

The former blanket AX201-conventional-only instruction is withdrawn. **Use actual runtime identity and mode, not the retail label.** Mapped requires reported Mode 2 plus active AppleVTD; conventional requires Mode 1 without independently requiring absence of system AppleVTD. EXPECTED_MODE checks, never selects the route. Configured DisableIoMapper and captured AppleVTD are separate evidence.

The runner discovers the actual PCI/controller/infrastructure-interface binding, excludes awdl0 and checks the server route and Wi-Fi traffic binding. `WIFI_IFACE` / `IOREG_WIFI_SERVICE` are optional checked overrides, never universal en2/ARPT/CNVW defaults. Fulvio's observed CNVW/en0 examples and all four ZC28/ZC8 mapped/conventional commands are in the [corrected command section](docs/benchmark/README.md#ax201-validation-commands). A Mode 1 mismatch alone does not prove Mode 2. This section and the guide supersede the blanket guidance in unchanged bundled documentation; failed checks must be returned, not bypassed.

### Submit beta results

Use the [ONE central GitHub results thread](https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2), one campaign per comment, or the equivalent shared [IM](https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/) / [TM](https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/) threads. Start with **card | candidate | AppleVTD configured/runtime | actual DMA mode (or unavailable)**. Link existing reports instead of uploading twice.

For EACH campaign attach ONE Return-ZIP containing the untouched runner-result ZIP, Ookla screenshot/link and short configuration notes. Include failures and slow runs. The [comment template and complete return instructions](docs/benchmark/README.md#submit-results) request actual patch/framework state, AP/radio and wired-server details. Archives remain attachments, not repository changes; separate issues are for concrete bugs.

## Selected historical best results — original whole-test measurements

Highest completed whole-test goodput per variable; these historical selections are not one combined campaign or a stability claim.

<details>
<summary>Show TCP, UDP, Internet results and original run references</summary>


**Definition:** “best” is the highest whole-test receiver goodput from a completed 8-second TCP or 5-second UDP measurement, never a one-second peak or a short final interval. Internet download/upload use the highest recorded respective networkQuality result, with directional RPM from that SAME run/phase. Different rows may come from different runs; **these tables are not one combined campaign or a stability claim**.

**Historical AX210 cohort:** KGP's spare AX210, AC channel 36/80MHz, wired M1 MacBook with UGREEN ASIX AX88179B (`.33`) and the existing comparable protocol. Earlier Realtek/`.161` and Luiz's AX211/other-AP results are not pooled into these best values. The external53 native-capable implementation is distinct from KGP's legacy target; the actual saved framework evidence and unverified patch state are recorded separately in the environment register. Completed TCP tests in an interrupted campaign may qualify; interrupted measurements and the known source-mutated 43-result attempt do not. ZC19 conventional has no qualifying original here and is shown as N/A.

**BE200 is a separate cohort:** Alpha 1/P7 and Alpha 2/P8R1 use the same BE200 system and wired ASIX peer, Tahoe 26.7.2 (25G332), actual Mode 2 with active AppleVTD; do not pool these with AX210 as a card ranking. R34/R35 identify the complete original campaigns. Per-test PHY/channel state was not continuously captured; earlier screenshots are not contemporaneous radio proof. The [BE200 per-row source ledger](docs/benchmark/BE200_RESULT_SELECTION.json) includes all 84 completed test summaries, exact JSON/server members and hashes. Protocol timing is unchanged across r8/r10/r11; observation/profile revisions are recorded separately.

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
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | P1 | 29.031 | 0 | R34 R03 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | P1 | 197.911 | 0 | R34 R02 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | P4 | 28.782 | 0 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | P4 | 226.185 | 204 | R34 R03 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | P1 | 484.847 | 45 | R35 R03 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | P1 | 506.846 | 1844 | R35 R02 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | P4 | 480.914 | 6315 | R35 R01 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | P4 | 475.986 | 2111 | R35 R01 |

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
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | 100 | 33.427 | 26.709 | 28/15394 | 0.161 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | 100 | 100.001 | 100.313 | 0/52293 | 0.000 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | 250 | 32.805 | 27.114 | 51/15103 | 0.298 | R34 R02 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | 250 | 250.000 | 250.877 | 0/130695 | 0.000 | R34 R03 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | 400 | 36.474 | 27.341 | 40/16986 | 0.211 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | 400 | 400.000 | 401.027 | 54/209109 | 0.026 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | 500 | 33.028 | 27.445 | 28/15192 | 0.163 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | 500 | 500.000 | 501.248 | 84/261265 | 0.032 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | TX | 600 | 32.715 | 27.228 | 39/15067 | 0.229 | R34 R01 |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | RX | 600 | 600.000 | 583.642 | 8559/312549 | 2.724 | R34 R03 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | 100 | 99.999 | 99.646 | 0/52086 | 0.000 | R35 R03 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | 100 | 100.001 | 100.324 | 0/52275 | 0.000 | R35 R02 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | 250 | 250.000 | 249.073 | 19/130310 | 0.015 | R35 R02 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | 250 | 249.999 | 250.753 | 0/130686 | 0.000 | R35 R01 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | 400 | 399.998 | 398.491 | 3/208331 | 0.001 | R35 R03 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | 400 | 399.999 | 401.083 | 0/208917 | 0.000 | R35 R01 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | 500 | 462.972 | 453.172 | 83/239083 | 0.034 | R35 R02 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | 500 | 499.999 | 500.656 | 406/261168 | 0.155 | R35 R01 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | TX | 600 | 457.111 | 446.404 | 106/236175 | 0.045 | R35 R02 |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | RX | 600 | 599.999 | 601.292 | 249/313629 | 0.079 | R35 R01 |

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
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | Download | 71.967 | 301.074 | R34 online |
| BE200 Alpha 1 / P7 | BE200 8086:272B | mapped (verified Mode 2) | Upload | 36.541 | 48.502 | R34 online |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | Download | 73.370 | 595.345 | R35 online |
| BE200 Alpha 2 / P8R1 | BE200 8086:272B | mapped (verified Mode 2) | Upload | 35.588 | 47.396 | R35 online |

**BE200 adverse findings remain:** P7 TCP TX approximately 29 Mbit/s, RX stalls/retransmissions and UDP losses are retained in R34. P8R1 retransmissions, UDP losses, sender-limited TX500/600 and RX variation remain in R35. Find My is unresolved. Original failed P8 and the pre-reboot identity mismatch are distinct attempts, not successful P8R1 measurements. Best rows cannot establish stable recovery or a common AX/BE200 RX cause.

**Adverse findings remain:** the severe earlier ZC26 conventional mixed TCP/UDP collapse, ZC28 mapped TX P1 247.018Mbps (one full interval 32.494Mbps), UDP loss bursts and sender-limited TX500/600 are not erased by maximum rows. ZC28's three conventional campaigns share one boot and do not prove permanent repair or long-run/sleep-wake stability. Online endpoints/times differ and upload-loaded tails remain long; best Internet capacity cannot qualify LAN performance or responsiveness. See the linked complete analysis/reference notes for the counterevidence. Selected raw-origin ZIPs and per-row source fields accompany the package; no individual logs are reproduced here.

#### Compact run references for the selected rows

Dates/times are from the original archive names (18–22 October 2026: CEST; 30 October: CET); `R01`/`R02`/`R03` after a reference means the round, and “online” means that archive's separate networkQuality stage. Only origins actually selected above are listed.

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


- **R34** — `BE200-ALPHA1_BE200_3X14_mapped_20261030_195034_oazuaw.zip`; SHA-256 `05eb55be559b93238f249b40a411382e80f0f5051eec6f7f40a5c1d3d0445684`. BE200 actual Mode 2 / AppleVTD, complete 42-test campaign.
- **R35** — `BE200-P8_BE200_3X14_mapped_20261030_232645_lLBMwH.zip`; SHA-256 `2fa6be173c86c26c2a4345cfc1d105537c70a5074525311668b5718185f50875`. BE200 actual Mode 2 / AppleVTD, complete 42-test campaign.


</details>

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

- **AX201 tester Fulvio ([@netiave on tonymacx86](https://www.tonymacx86.com/members/netiave.1548699/) / [@Funzio on InsanelyMac](https://www.insanelymac.com/forum/profile/2547924-funzio/) / [@netiave on GitHub](https://github.com/netiave))** — the same tester; earlier reports helped motivate the comparison beta. Their actual DMA route is not established here.
- **AX211 tester Luiz ([@LuizMeliga on tonymacx86](https://www.tonymacx86.com/members/luizmeliga.1658846/) / [@luizmeliga on GitHub](https://github.com/luizmeliga))** — earlier AX211 campaigns demonstrated strong RX on his own system/network.

These acknowledgements concern historical reports, not testing or qualification of ZC28. Klaus confirmed the listed TM/GitHub profiles for both testers and Fulvio’s IM profile. No InsanelyMac account has been confirmed for Luiz.

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

Separate issues are for concrete bugs needing independent investigation. Record card/PCI identity, macOS, exact candidate UUID/SHA, ACTUAL framework/patch environment, system AppleVTD setting and observed driver DMA route; unavailable is not zero. Existing debugging/forensic procedures are separate from the shared throughput campaign and must not be run concurrently or used as pre-test preparation.
