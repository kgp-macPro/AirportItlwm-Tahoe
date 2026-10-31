CURRENT SHARED VALIDATION GUIDE — r11

The current 1.2.0 Alpha is primary for AX201/AX210/AX211/BE200. Beta/ZC comparisons are optional. Laobamac 44/47/53 is AX201/AX210/AX211-only and does NOT support BE200. Frozen beta ZIP and r7 remain unchanged; current instructions supersede old setup and blanket DMA assignments.

# Validation testing: AX201, AX210, AX211 and BE200

This is the current shared test guide. It supersedes older bundled tool-setup and blanket AX201 DMA instructions. Frozen beta r7 and all old result archives remain unchanged. Current tools: **shared-2026-10-31-r11-public-be200**; protocol **AIRPORTITLWM_3X14_NO_PROFILER_V1**.

## Quick start and prerequisites

1. Use the driver and matching framework/boot environment below. Save a complete working bundle/boot backup. Driver replacement, root patches and reboot are manual; tool setup does none of these.
2. Download/extract [AirportItlwm-Shared-Benchmark-r11.zip](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r11.zip) on both Macs. Extracting creates `AirportItlwm-Shared-Benchmark-r11/` in Downloads.
3. Run the one setup command below on each Mac. It prepares Desktop tools, preserves differing existing files and opens the instructions. It does not start tests.
4. Start the wired server, then run the appropriate client command. The client asks for the current wired-server IPv4.
5. Keep the original output ZIP, including failures; perform Ookla separately and return one outer Return-ZIP per campaign.

**The current KGP Alpha for AX201/AX210/AX211/BE200 requires [Retro-OCLP-CustoMac root patching](https://github.com/kgp-macPro/OCLP-CustoMac)** with restored Ventura legacy wireless frameworks and the matching supported Skywalk/AMFIPass/`-amfipassbeta` boot environment. **KGP ZC28 and historical beta controls require OCLP-CustoMac 3.0.3, its Skywalk replacement, AMFIPass.kext with `-amfipassbeta`, and their matching supported framework/boot configuration.**

**External laobamac53 is a separate native Wi-Fi control:** its Wi-Fi implementation does not require OCLP, Skywalk replacement or `-amfipassbeta`. A kext swap does not remove root patches. Preserve a working configuration, follow the external driver's supported native environment and record ACTUAL loaded frameworks/patches rather than calling a run unpatched because the driver supports native operation. No supplied tool applies/removes root patches or changes EFI. The frozen [beta installation/rollback reference](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.1.0-beta.1/README_TESTS.txt) must be read with the current DMA erratum, not as a retail-card route rule.

Tools require macOS, Bash 3.2+, **Python 3.9+**, existing `iperf3`, `xcrun dwarfdump` (Command Line Tools), `ioreg`, `kmutil` and `networkQuality`. Setup installs no dependencies. No `system_profiler`, concurrent Wi-Fi scans or forensic capture during measurement.

## Desktop setup (tools only)

Paste this after extracting the tool ZIP into Downloads:

```bash
/bin/bash "$HOME/Downloads/AirportItlwm-Shared-Benchmark-r11/SETUP_AirportItlwm_Benchmark.command"
```

The same `.command` can be double-clicked if macOS permits it; the Terminal command above is the verified fallback. The installed common runner is `~/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh`, with all required `runner_profiles/` files. Existing differing tools are backed up under `~/Desktop/AirportItlwm/BACKUPS/benchmark-r11-<time>-<random>/`; existing TESTS and unrelated files are preserved. Symlink/conflicting paths and checksum errors are rejected. To undo a tool update, restore the complete corresponding TOOLS files from that backup. To undo a driver change, restore the complete known-working kext and matching boot configuration, then reboot manually.

**The old beta setup still installs frozen r7. Run this new setup AFTER using beta setup.** If beta setup is used again, rerun current setup and verify:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" --version
```

Expected revision: `shared-2026-10-31-r11-public-be200`. Compare its printed SHA-256 with [RUNNER_IDENTITY.json](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/tools/benchmark/RUNNER_IDENTITY.json) and the [tool ZIP checksum](https://github.com/kgp-macPro/AirportItlwm-Tahoe/raw/refs/heads/main/tools/benchmark/releases/AirportItlwm-Shared-Benchmark-r11.zip.sha256). Each campaign saves the executed script/resource copies, hashes, revision, protocol and effective settings. Frozen r7/r8/r9/r10 references are enclosed; none replaces the current runner.

## Wired server Mac — explicitly bind Ethernet IPv4

Connect Ethernet and turn the **server's Wi-Fi OFF manually**. On the server Mac after tool setup:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_IPERF3_WIRED_SERVER.sh"
```

It lists ports, asks for the real wired BSD interface, checks Wi-Fi is off and that wired IPv4/link are available, then runs **`iperf3 -s -B "$ip" -p 5201`** with that address (optional `IPERF_PORT`). Enter the printed current IPv4 in the client prompt. Nothing changes network settings. Ctrl-C stops the listener. Interface names and addresses are not shared defaults between testers.

## Candidate and interface selection

Install/reboot with each matching complete candidate manually; confirm association and IPv4 first. The on-disk candidate is a comparison reference, not an automatic installation path. `CANDIDATE_KEXT=/absolute/path/AirportItlwm.kext` overrides it. `AIRPORTITLWM_ROOT` and `AIRPORTITLWM_TEST_DIR` remain configurable. No label verifies hardware.

The runner discovers a unique Intel PCI -> selected controller -> infrastructure interface binding and records PCI/subsystem identity, provider/controller/interface paths and entry IDs. Optional `WIFI_IFACE` and `IOREG_WIFI_SERVICE` overrides must agree with that binding. It excludes awdl0 and rejects missing/ambiguous/wrong bindings. `en2`/`ARPT` are KGP's observations, not defaults.

For manual identification only, before the campaign:

```bash
ioreg -p IOService -w0 | /usr/bin/grep -i -B 6 -A 6 AirportItlwm
networksetup -listallhardwareports
```

`EXPECTED_MODE` checks the actual driver route; it does not select DMA mode or change OpenCore. **Mapped requires reported Mode 2 and active AppleVTD. Conventional requires reported Mode 1, without independently requiring system AppleVTD to be absent.** Record configured DisableIoMapper and captured AppleVTD separately. AX201 is not universally conventional-only; a retail name, CNVW/ARPT or AppleVTD alone proves neither identity nor mode. A guard failure is evidence to return, not permission to bypass checks.

All iperf traffic binds the verified Wi-Fi IPv4 and checks the route to the chosen server. Final sequential networkQuality binds that Wi-Fi interface. Ethernet may exist on the client; it must not carry the measured traffic. The common script does not disable it.

## Current 1.2.0 Alpha — primary KGP candidate for all four cards

The unchanged published Alpha 2/P8R1 is the primary KGP validation candidate for **AX201, AX210, AX211 and BE200**. BE200 has the documented physical campaign; AX201/AX210/AX211 are invited to validate it, not newly qualified by the BE200 result. No mandatory ZC comparison matrix. All four calls reference the same full extracted bundle.

### AX201 / AX210 / AX211 Alpha validation

Run ONE matching command after manual installation/reboot. These examples request Mode 2; use `conventional` instead only for the corresponding actual Mode 1 validation. Neither setting changes the route.

```bash
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" AX201-ALPHA2 mapped
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" AX210-ALPHA2 mapped
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" AX211-ALPHA2 mapped
```

The three AX profiles independently pin the same published UUID/executable/Info.plist hashes, the actual non-BZ `ZC30-PERF` marker and inherited health schema. Their allowed Intel PCI IDs come from the pinned implementation's non-BZ IWX match table, not a claimed retail-card identity mapping. Provider/subsystem identity is recorded; labels do not decide or qualify DMA admission. BE200 cannot pass an AX profile and an AX device cannot pass the BE200 profile. Default AX reference bundle: `~/Desktop/AirportItlwm/CANDIDATES/ALPHA2/AirportItlwm.kext`, unless overridden as above.

### BE200 Alpha validation

Use the unchanged [public Alpha 2 bundle](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.2). After extracting that release ZIP into Downloads and manually installing/rebooting, this exact command references its complete extracted bundle:

```bash
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2/AirportItlwm.kext" DEVICE_LABEL=BE200 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" BE200-ALPHA2 mapped
```

This candidate profile pins UUID `E4414AAB-1D58-3E40-93A5-6838D3CDCD20`, executable SHA-256 `8e4a38121b06941630afa98728b245cbaeec813c14139c46715dc44723461f63`, P8R1 registry/marker, Intel `8086:272B`, inherited health and BE200 transport diagnostics. It is not ZC28 identity. Without `CANDIDATE_KEXT`, the public default is `~/Desktop/AirportItlwm/CANDIDATES/BE200-ALPHA2/AirportItlwm.kext`; place a full unmerged reference bundle there yourself if using that layout.

**Optional own BE200 reference only:** The original private call `BE200-P8 mapped` still pins the SAME corrected P8R1 and retains `BE200-EXPERIMENTAL-P8` as its default directory. The original failed P8 is not accepted. Alpha 1/P7 remains independently pinned and supported:

```bash
CANDIDATE_KEXT="$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-1/AirportItlwm.kext" DEVICE_LABEL=BE200 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" BE200-ALPHA1 mapped
```

An absolute bundle path with `BENCHMARK_PROFILE=be200 CANDIDATE_LABEL=BE200-ALPHA2` is equivalent. BE200 conventional syntax remains available for explicit route validation, but the published measurements here qualify Mode 2 only; do not claim an untested route as qualified. Firmware/transport and inherited epoch/fault guards stay enabled. Optional histories are explicitly UNAVAILABLE when absent, not zero/success.

## Optional reference comparisons: AX201 / AX210 / AX211 beta builds

The Alpha above is primary. There is no required historical matrix. Optionally use the unchanged [1.1.0 Beta comparison package](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.1.0-beta.1) for its complete KGP candidates; run current tool setup afterwards to replace r7 safely. Set `DEVICE_LABEL` to the tester's reported card, with runtime hardware checked independently. Choose **one verified mode at a time**. These examples use `mapped`; substitute `conventional` only for the matching actual Mode 1 comparison:

```bash
DEVICE_LABEL=AX210 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28 mapped
DEVICE_LABEL=AX210 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC8 mapped
DEVICE_LABEL=AX210 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC19 mapped
DEVICE_LABEL=AX210 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC22 mapped
DEVICE_LABEL=AX210 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC26 mapped
```

Run separately with the matching installed/rebooted candidate, never paste the entire block as a batch. Existing ZC13/ZC18/ZC20/ZC21/ZC23/ZC24/ZC25/ZC27 and private ZC29/ZC30 calls remain supported with their existing schemas/layouts; they do not require a new physical campaign. Luiz's AX211 label is `DEVICE_LABEL=AX211`; his real interface/node must be discovered, not inferred from KGP or Fulvio.

### AX201 validation commands

These four explicit examples preserve Fulvio's **observed CNVW node / en0** override. Use the generic discovery commands instead on other systems. `DEVICE_LABEL=AX201` does not verify retail identity. A wrong-Mode-1 stop does not by itself prove Mode 2. Mapped is a validation request, not a general hardware qualification.

```bash
IOREG_WIFI_SERVICE=CNVW EXPECTED_MODE=mapped WIFI_IFACE=en0 DEVICE_LABEL=AX201 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28
```

Install/reboot ZC8 before:

```bash
IOREG_WIFI_SERVICE=CNVW EXPECTED_MODE=mapped WIFI_IFACE=en0 DEVICE_LABEL=AX201 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC8
```

For the intended AppleVTD-off comparison, configure `Kernel -> Quirks -> DisableIoMapper=true` yourself in the actual boot EFI and reboot with ZC28. Record the configured setting and runtime AppleVTD separately; the script requires actual Mode 1, not necessarily absence of system AppleVTD:

```bash
IOREG_WIFI_SERVICE=CNVW EXPECTED_MODE=conventional WIFI_IFACE=en0 DEVICE_LABEL=AX201 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28
```

Install/reboot ZC8 retaining that same conventional boot configuration before:

```bash
IOREG_WIFI_SERVICE=CNVW EXPECTED_MODE=conventional WIFI_IFACE=en0 DEVICE_LABEL=AX201 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC8
```

These current instructions supersede the blanket AX201 mode assignments in older bundled guides. No tool changes EFI or performs these manual steps.

## Optional external laobamac control — AX201 / AX210 / AX211 ONLY

**The supported laobamac comparison builds 44/47/53 do not support BE200. All laobamac commands here are exclusively for AX201, AX210 and AX211. BE200 uses the current Alpha and optional earlier own BE200 candidates, not these external builds.**

Run each separately after the correct external bundle/boot environment is installed. The numbered legacy path `~/Desktop/Laobamac/53/AirportItlwm.kext` remains supported; `LAOBAMAC_KEXT` overrides it. Public beta setup supplies that bundle unchanged:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 44 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 44 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 47 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 47 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 conventional
```

`mapped`/`conventional` here record the user's DECLARED AppleVTD configuration, not verified KGP numerical DMA telemetry. Loaded UUID, reference executable/Info.plist hashes, binding and route remain checked. Missing KGP fault/mode/epoch data is explicitly unavailable, not fabricated healthy/zero. Numbered 44/47 require their separately acquired matching full bundles at `~/Desktop/Laobamac/44/` and `/47/`; the beta package includes 53, not those other payloads. Source declarations: 44=`d695c971420cc8af0daa286dcd3297e8236c0a63`, 47=`bbe368ec7d55a5395d08c729e88eeaabebae8255`, 53=`738d4c61b5a64433b27c1ab09dba94916ab77d8d`. The earlier two-argument calls also remain unchanged:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC conventional
```

Those unnumbered calls retain their existing `~/Desktop/AirportItlwm/LAOBAMAC-738d4c6/AirportItlwm.kext` default, overridable with `LAOBAMAC_KEXT` and `LAOBAMAC_SOURCE_REF`. They are not BE200 commands. `EXTERNAL_SOURCE_REF` is a declaration, not proof of binary/source correspondence. Never use this profile to evade a KGP candidate guard.

## Unchanged measurement protocol and originals

Three rounds of fourteen cases: TCP TX P1, RX P1, TX P4, RX P4 (8 seconds each), then UDP TX/RX pairs at 100/250/400/500/600 Mbit/s (5 seconds, 1200-byte payload). The same order, 2-second gaps, eight original boundary checks and final sequential networkQuality are retained. Original final pre-online pause is also retained. No arbitrary warm-up or favorable-round selection. Nondefault settings remain parameterizable and recorded, but are not the reference protocol.

Each campaign preserves client JSON/intervals/stream values, sender/receiver accounting durations, TCP retransmissions (counts, not loss percentages), UDP actual sender rates, receiver rates and reported lost/total/percent from the SAME measurement, original server output, stderr/errors and settings. Failed/interrupted campaigns remain identifiable and are not complete measurements. networkQuality directional RPM and raw latency series remain alongside Internet capacity; online PASS means successful execution, not acceptable performance.

Results: `~/Desktop/AirportItlwm/TESTS/<candidate>_<label>_3X14_<mode>_<timestamp>_<random>.zip` plus the same-named original folder. Never edit/repack the inner runner ZIP. Adjacent equal health snapshots are accepted; rollback against the last accepted snapshot is rejected; FINAL must advance from INITIAL. Loaded UUID/marker/mode/fault/epoch and profile-specific guards remain strict. Boundary health cannot rule out all brief stalls.

## Submit results

Use the [existing central results thread](https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2), one campaign per comment, or the equivalent [InsanelyMac](https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/) / [tonymacx86](https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/) project threads. Link an existing report instead of uploading twice. No account on a second platform is required. Concrete bugs may have separate issues linked to the results.

Attach ONE outer Return-ZIP per campaign containing the untouched runner ZIP, Ookla screenshot/result link and brief notes. Include slow/failed attempts. Run Ookla after the campaign; if unavailable, say why. Keep forensic/debugging collections separate, never simultaneous with throughput measurement. Existing forensic attachments are unchanged.

Copyable comment:

```text
Card / PCI ID | candidate + UUID | AppleVTD configured/runtime | actual DMA mode (or unavailable)
System / macOS build:
Actual root patches / frameworks / Skywalk / AMFIPass / boot settings:
Runner revision + SHA256:
Wi-Fi interface/provider (from DEVICE_BINDING.json):
AP, band/channel/width, distance; wired server hardware/adapter/iperf3:
Boot / OFF-ON / sleep-wake / prior traffic / failures or slow cases:
Return-ZIP attachment or existing report link:
```

The tools do not qualify a card, select a DMA route, install a driver or start traffic until the tester runs the commands. Existing frozen Kext ZIPs, releases and original result archives are unchanged.

## Source and license

Maintained tools and frozen references are distributed as source under the repository GPL-2.0 license (LICENSE enclosed). The source is in `tools/benchmark/` on main; no kext or firmware is bundled in this tool ZIP.
