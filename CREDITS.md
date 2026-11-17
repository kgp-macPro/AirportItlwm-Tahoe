# Credits and acknowledgements

AirportItlwm-Tahoe builds on work by several projects and developers. The
credits below cover the regular Wi-Fi release and the experimental AirDrop
beta; beta contributions are identified explicitly.

### Driver foundations and contributed work

- **[zxystd and OpenIntelWireless](https://github.com/OpenIntelWireless/itlwm)**
  — itlwm/AirportItlwm, the Intel wireless driver foundation and the original
  Apple wireless integration. **[DexterSLamb](https://github.com/DexterSLamb)**
  — the pinned upstream fork used as this project's starting point.
- **Intel and the Linux iwlwifi/iwlmvm contributors** — firmware API
  definitions and implementations ported/adapted for MAC/PHY/LINK resources,
  AP/GO roles, stations, queues, beacons and TSF timing. The GL92 backend and
  AX GO integration depend on this work. Credit includes **Intel Corporation,
  Intel Mobile Communications GmbH and Intel Deutschland GmbH**, as named in
  the source; original SPDX and copyright notices are retained. Intel's
  firmware files remain separately licensed.
- **[gsfront](https://github.com/gsfront)** — contributed BE200 HE and optional
  HE SU-Beamformee support, firmware/link/station setup, HE information-element
  corrections and receive aggregation/reordering work through
  [PR #5](https://github.com/kgp-macPro/AirportItlwm-Tahoe/pull/5).
  Original commits, authorship and co-author notices are preserved.
- **[laobamac](https://github.com/laobamac/itlwm)** — AX HE implementation used
  as a reference and adapted to our Ventura Legacy path; native Wi-Fi and
  macOS integration sources for adaptation and comparison.
  **[crazyhulk](https://github.com/crazyhulk)** — authored the experimental
  AWDL/P2P implementation submitted as
  [laobamac PR #53](https://github.com/laobamac/itlwm/pull/53), including
  mechanisms adapted for our AX backend. Our Legacy frontend, DMA lifetimes
  and final integration remain distinct project work.
- **[mingww64](https://github.com/mingww64)** — authored
  [laobamac PR #36](https://github.com/laobamac/itlwm/pull/36), the WPA2/WPA3
  transition-mode association changes. The beta adapts the relevant scan-IE
  conversion and PSK-selection mechanisms to the Legacy driver. This credits
  the contributed WLAN change without claiming reliable Instant Hotspot.
  [PR #55](https://github.com/laobamac/itlwm/pull/55) was separately examined
  as an LQM/structure-layout reference; its WCL message path is not claimed
  as an R49 import.
- **[Bananz0](https://github.com/Bananz0), and Glen Muthoka as credited in
  the original source** —
  [AirportItlwm-Tahoe-AWDL](https://github.com/Bananz0/AirportItlwm-Tahoe-AWDL)
  host-protocol code retained and adapted for AWDL templates, frame/service
  handling, peer synchronization and channel structures; original copyright
  and GPL notices remain. Additional mechanisms from the companion
  [Tahoe BLE work](https://github.com/Bananz0/BrcmPatchRAM-Tahoe-BLE) were
  adapted into our separate modified BlueToolFixup.
- **[retropineaple](https://github.com/retropineaple)** — author of
  [ForbiddenItlwm](https://github.com/retropineaple/ForbiddenItlwm), an important
  AWDL mechanism and comparison reference. Announcement framing, service
  handling, peer timing and coexistence behavior informed adaptations and
  investigations here. The Data-Path-TLV infrastructure-omission mechanism
  was specifically adapted, with Legacy address fields retained.
  R49's Legacy frontend, fixed-channel GO role and
  BE200/AX firmware implementation are described separately in the beta
  section.
- **NorthKoreanNoodles / maddog860** — published the direct virtual-interface
  attach/publication approach, subsequently validated and adopted for this
  project's Legacy AWDL interface.
- **[Acidanthera](https://github.com/acidanthera)** — Lilu, MacKernelSDK and
  the original BlueToolFixup/BrcmPatchRAM foundation.
  **zxystd, OpenIntelWireless and
  [lshbluesky](https://github.com/lshbluesky/IntelBluetoothFirmware)** —
  IntelBluetoothFirmware and IntelBTPatcher, including the unchanged 2.5.3
  components in the tested bundle. The beta's modified BlueToolFixup and BLE
  integration combine these foundations, Bananz0's patch mechanisms and our
  native-image/controller-selection adaptations.

### Project development, testing and research

**[KGP / kgp-macPro](https://github.com/kgp-macPro) — Project lead, maintainer
and integration coordinator.** Project concept and technical goals;
continuous human direction, supervision and critical review of AI-assisted
development and reverse engineering; integration and refinement of upstream
contributions; regression investigation; software and hardware validation
across driver revisions, devices and macOS builds; and the design, execution
and analysis of reproducible benchmark campaigns.

KGP also coordinates community testing, reviews conflicting findings,
challenges assumptions and redirects investigations, maintains technical
documentation and evidence-backed handovers, and manages release preparation
and publication. Development combines AI-assisted implementation and binary
analysis with sustained human judgment, hands-on hardware testing and
repeated validation on real systems.

- **[netiave](https://github.com/netiave)** — historical AX201/CNVW testing,
  including the documented mapped/AppleVTD configuration.
  **[LuizMeliga](https://github.com/luizmeliga)** — historical AX211 testing.
  Their earlier infrastructure results are credited without implying that
  they tested the current AirDrop beta. Thanks also to the community members
  reporting reproducible successes, failures and regressions.
- **[Mieze / IntelLucy](https://github.com/Mieze/IntelLucy)** — prior art
  informing mapper-aware DMA and lifetime design; reference work, not a claim
  of copied implementation or co-development.
- **OpenBSD and the net80211/iwn/iwm/iwx contributors** — inherited wireless
  stack and Intel driver ancestry, with original per-file notices retained.
- **[seemoo-lab / Open Wireless Link](https://github.com/seemoo-lab/owl)** —
  AWDL protocol and timing research used as a reference; no OWL implementation
  code is imported into this driver.
- **OCLP contributors and
  [OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac)** — the restored
  wireless framework/root-patch environment used by this project's setup.
- **[REA](https://github.com/morluto/rea) and
  [Ghidra](https://github.com/NationalSecurityAgency/ghidra)** — binary-analysis
  tools used to investigate specific native interfaces and compiled paths.
  Their results do not substitute for firmware or hardware validation.

### AI-assisted development and human direction

- **OpenAI ChatGPT** — Collaborative analysis, technical discussion, critical review, documentation, handover preparation and release planning under KGP’s continuous direction.
- **OpenAI Codex CLI** — AI-assisted implementation, debugging, builds, automated checks, REA/Ghidra-assisted binary investigation and repository integration under KGP’s continuous supervision, review and hardware validation.

KGP directs, reviews and validates this work and controls project and release
decisions. These acknowledgements do not imply endorsement or support by
OpenAI. [Verified GitHub co-author attribution](docs/AI_ATTRIBUTION.md) is
limited to tools that actually contributed; it does not replace human
responsibility or hardware validation.

Inherited source copyrights, co-author notices and the [license](LICENSE)
remain applicable.
These acknowledgements distinguish contributed code, adapted work,
references, testing and tools; they do not imply joint maintenance or
endorsement by the named projects.


## Preserved historical attribution

The [unchanged pre-beta credits](docs/HISTORICAL_CREDITS_2.0.0.md) retain the
original detailed references, tester profile links, HE integration pins and
historical external-control attribution. Original source notices and licenses
remain unchanged.

## Inherited licenses and attribution

Existing LICENSE, source copyrights and firmware/SDK notices remain unchanged. The [preserved upstream README](docs/UPSTREAM_README.md) retains upstream credits in their original context, including Acidanthera/MacKernelSDK, Apple, AppleIntelWiFi, ErrorErrorError, Intel firmware/iwlwifi, Linux, mercurysquad/Voodoo80211, OpenBSD/net80211/iwn/iwm/iwx, pigworlds, rpeshkov/black80211, usr-sse2 and zxystd. This project attribution does not replace those notices.
