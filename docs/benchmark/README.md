# Current benchmark guide: Alpha 2/P8R1 and laobamac 53

Use one installed driver at a time. The active comparison has two driver variants:

| Variant | Cards | Identity and environment |
|---|---|---|
| [AirportItlwm-Tahoe Alpha 2/P8R1](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.2) — primary | BE200, AX201, AX210, AX211 | One unchanged bundle; Retro-OCLP-CustoMac restored Ventura legacy Wi-Fi frameworks and matching Skywalk/AMFIPass/`-amfipassbeta` environment. UUID `E4414AAB-1D58-3E40-93A5-6838D3CDCD20`; executable SHA-256 `8e4a38121b06941630afa98728b245cbaeec813c14139c46715dc44723461f63`. |
| laobamac **build 53** (`738d4c61b5a64433b27c1ab09dba94916ab77d8d`) — optional external comparison | AX201, AX210, AX211 only, with independently verified matching PCI device and complete build-53 bundle | Native Wi-Fi implementation; check its own supported framework/boot environment and record what is actually loaded. The local reference has UUID `AEF5621B-B1D8-3487-BC9D-92E035BC087E` and executable SHA-256 `259bac9432bffa8bf9c73411d8ac8f4a854552375e9f60ba0870ea3151ed973c`. The runner validates loaded UUID against the chosen on-disk bundle; the source commit remains a declaration, not a binary provenance proof. **No BE200 support is claimed.** |

Changing a kext does not remove root patches or change OpenCore. Preserve a known-working complete bundle and boot configuration; install/reboot and rollback are manual. The tool package only copies benchmark tools. Earlier Alpha 1, ZC/beta and laobamac 44/47 builds, their original downloads and [historical measurements](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/README.md#selected-historical-best-results--original-whole-test-measurements) remain available for research; they are outside the active two-variant entry flow. [Framework comparison history](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/beta/FRAMEWORK_ENVIRONMENT_REGISTER.txt) records the differing environments.

## 1. Install the tool-only package on both Macs

Download [Benchmark r12](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r12.zip) and its [SHA-256 file](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r12.zip.sha256). Extract the complete folder into `~/Downloads`, then run on **both** Macs:

```bash
/bin/bash "$HOME/Downloads/AirportItlwm-Shared-Benchmark-r12/SETUP_AirportItlwm_Benchmark.command"
```

The setup verifies its complete manifest, backs up differing Desktop tool files under `~/Desktop/AirportItlwm/BACKUPS/benchmark-r12-*`, and leaves `TESTS` and unrelated files intact. It installs no kext or dependency and starts no traffic. Requirements: macOS, Bash 3.2+, Python 3.9+, existing `iperf3`, Command Line Tools (`xcrun dwarfdump`), `ioreg`, `kmutil` and `networkQuality` on the client. Run `--version` on the installed 3×20 script and compare its SHA-256 with [RUNNER_IDENTITY.json](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/tools/benchmark/RUNNER_IDENTITY.json). The r12 package carries both protocols; the unchanged 3×14 runner and [r11 package](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r11.zip) remain available for historical comparisons.

## 2. Start the wired server

Connect the server Mac by Ethernet and turn its Wi-Fi off manually. Run:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_IPERF3_WIRED_SERVER.sh"
```

The helper asks for the **actual wired BSD interface**, checks its IPv4/link and Wi-Fi-off state, then runs `iperf3 -s -B <wired IPv4> -p 5201`. Keep it open during the client campaign. Enter its printed IPv4 when the client prompts. Ctrl-C ends the server. Record the actual peer, adapter, link rate and any server limit; the runner cannot establish those from the client alone.

## 3. Run the Wi-Fi client

Install/reboot with the chosen complete kext manually, then select its matching command. The path in `CANDIDATE_KEXT` is the **on-disk identity reference** for UUID, executable and Info.plist hash checks; running the command does not install or load that path. Point it to the same exact bundle already loaded. The runner discovers the Intel PCI provider, controller and infrastructure interface, excludes `awdl0`, verifies the loaded identity and binds iperf to the discovered Wi-Fi IPv4 and route to the server. `WIFI_IFACE` and `IOREG_WIFI_SERVICE` are optional checked overrides for a specific machine, never universal `en0`/`en2` or CNVW/ARPT rules. `DEVICE_LABEL` is descriptive text, not PCI proof.

Use the [Alpha 2 release ZIP](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v1.2.0-be200-alpha.2/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2.zip), extracted into Downloads. Run **one** matching command after installation/reboot:

```bash
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" BE200-ALPHA2 mapped
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" AX201-ALPHA2 mapped
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" AX210-ALPHA2 mapped
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" AX211-ALPHA2 mapped
```

These examples request actual **Mode 2 mapped plus active AppleVTD**. For a separately configured actual Mode 1 run, replace the last word with `conventional`; that check does not require system AppleVTD to be absent. `EXPECTED_MODE` checks the driver route; it cannot select it. AX201 is not automatically conventional, and a retail card name does not establish PCI identity, DMA route, or a successful test. AX profiles pin the same Alpha 2 UUID/hash, their non-BZ `ZC30-PERF` marker, inherited health schema and source-derived PCI allowlist; BE200 requires `8086:272B` and its BE200 transport record. Return guard failures intact rather than bypassing them. Actual AX201/AX210/AX211 support remains card and platform specific; the BE200 results do not qualify those systems.

### 802.11ac/VHT160 status for Alpha 2/P8R1

**BE200 VHT160 verified; AX201/AX210/AX211 VHT160 validation pending.** The BE200 60-test archive below records channel 44/160 MHz and completed traffic. AX201/AX210/AX211 hardware capability alone is not a measurement with this build. KGP identifies Luiz's AX211 campaign as 80 MHz; its retained `AX211_Alpha2_P8R1_Return_20261103.zip` notes do not record a contemporaneous channel width, so that archive independently proves neither 80 nor 160 MHz. A future AX VHT160 result needs the matching loaded Alpha 2 UUID/hash, PCI/binding and Mode proof, contemporaneous negotiated channel/width and complete traffic/stability records.

**Static source review, separate from hardware proof:** The tested P8R1 commit `a10a32bf296d6b90da1b3390b3ef4459f9d207fc` has byte-identical relevant source blobs to the [public Alpha 2 source commit](https://github.com/kgp-macPro/AirportItlwm-Tahoe/commit/e1eb70a8ad2552e66bcd8b9b4c4d56fcdd0c805d). In the non-BZ IWX path, [NVM channel flags and VHT capability setup](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/e1eb70a8ad2552e66bcd8b9b4c4d56fcdd0c805d/itlwm/hal_iwx/ItlIwx.cpp#L6143-L6481) provide 160-MHz channel/capability bits when the NVM and 11ac conditions allow them. The global `vht160_supported` bit is set before each channel's `VALID` check, so this static flag alone does not prove a usable 160-MHz channel. [VHT peer-capability and operation handling](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/e1eb70a8ad2552e66bcd8b9b4c4d56fcdd0c805d/itl80211/openbsd/net80211/ieee80211_proto.c#L711-L820) contains 160-MHz width/center-channel selection. [PHY context](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/e1eb70a8ad2552e66bcd8b9b4c4d56fcdd0c805d/itlwm/hal_iwx/ItlIwx.cpp#L8697-L8899), [station flags](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/e1eb70a8ad2552e66bcd8b9b4c4d56fcdd0c805d/itlwm/hal_iwx/ItlIwx.cpp#L10029-L10048) and [VHT TLC firmware rates/width](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/e1eb70a8ad2552e66bcd8b9b4c4d56fcdd0c805d/itlwm/hal_iwx/ItlIwx.cpp#L11563-L11806) include 160-MHz cases after association. These paths show intended VHT support; they do not establish successful 160-MHz negotiation, firmware acceptance or sustained AX traffic. This review concerns existing 802.11ac code and does not depend on the planned HE integration from PR #5.

For the **optional AX-only laobamac build 53** comparison, use its complete verified build-53 bundle at the numbered path, after the appropriate separate manual installation/reboot:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 mapped
```

The expected reference path is `~/Desktop/Laobamac/53/AirportItlwm.kext`; `LAOBAMAC_KEXT` may override it with another full build-53 path. `mapped` or `conventional` in this external command records the **declared** AppleVTD configuration. The external kext lacks KGP numerical mode/fault/epoch telemetry; actual DMA mode is unavailable, not zero or independently verified. A BE200 PCI binding is rejected for these retained laobamac comparison builds. Do not use a different build under the build-53 label.

## Protocol and interpretation

**3×20** (`AIRPORTITLWM_3X20_NO_PROFILER_V1`) runs three rounds of 20 cases: TCP TX/RX with one and four streams for 8 seconds, then UDP TX/RX at requested 100, 250, 400, 500, 600, 700, 800 and 900 Mbit/s for 5 seconds with 1200-byte payload. The gap is 2 seconds. A sequential, interface-bound Internet `networkQuality` follows. The [3×14 protocol](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/tools/benchmark/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh) ends at UDP 600 and has 42 tests; do not combine its rows with 3×20 as one campaign. The r12 bundle retains that exact r11 runner for reproducibility.

**TX** means the tested Wi-Fi client sends toward the wired server; **RX** means the server sends toward the Wi-Fi client. In UDP, `-b` is the requested target; `SUMMARY.csv` and raw JSON separately retain **actual sender**, **receiver** and **reported packet loss**. A 900 Mbit/s target with a 600 Mbit/s sender is not a 900 Mbit/s TX measurement, and the 300 Mbit/s target gap is not packet loss. TCP retransmissions are sender counts, not loss percentages. `cpu_utilization_percent.host_total` in the client iperf JSON is the local iperf process CPU measure; a high value suggests a possible sender limit but does not identify its sole cause. PHY rate, LAN goodput and separate Internet `networkQuality` results have different meanings.

The runner saves every original iperf JSON (including intervals and server output), commands, stderr, `SUMMARY.csv`, settings, eight identity/health/binding checkpoints, provenance, manifest, final online result and `RUN_STATUS.txt`. A complete 3×20 result requires **60/60 plus ONLINE PASS**; incomplete runs are archived with their failure and must not be relabeled complete. Boundary snapshots cannot exclude every transient.

## Return results

Send the **untouched runner result ZIP** from `~/Desktop/AirportItlwm/TESTS/` for each campaign, including failed attempts. Include brief notes with card and PCI ID, OS/build, installed variant and exact build, configured and observed AppleVTD/Mode, framework/root-patch state, AP/channel/width/security/PHY reading, server adapter/link rate/IP, distance, reconnect or sleep observations, and any separate speed-test screenshot/link. The original ZIP already contains the machine-readable evidence. Use [GitHub results issue #2](https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2) or the existing [InsanelyMac](https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/) / [tonymacx86](https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/) threads; link an existing report instead of uploading twice.

The independently checked [BE200 Alpha 2 VHT160 3×20 result](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/benchmark/BE200_ALPHA2_VHT160_3X20_20261104.md) is a 60-test campaign with an unchanged driver and a different AP/server setup; KGP reports a 1-Gbit/s wired-server link for this archive. Its full original ZIP is the only newly published result archive. A later manual 1→10→1-Gbit/s server-link comparison is documented separately in the result report, outside the 60-test tables; the 10-Gbit/s TCP RX drop has no localized cause. The immediately preceding 3×14 archive remains internal. Earlier [BE200 observations](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/benchmark/BE200_OBSERVATIONS_ARCHIVE.md), [per-row historical selection ledger](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/benchmark/BE200_RESULT_SELECTION.json), beta downloads and old tools remain unchanged.
