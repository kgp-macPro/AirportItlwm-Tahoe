# Credits and acknowledgements

## Upstream and foundation

- **zxystd / OpenIntelWireless** — AirportItlwm/itlwm and its Intel wireless foundation.
- **DexterSLamb** — pinned upstream fork used for this project.

## Project development and research

**KGP / kgp-macPro — Project lead, maintainer and integration coordinator.** Project concept and technical goals; continuous human direction, supervision and critical review of AI-assisted development and reverse engineering; integration and refinement of upstream contributions; regression investigation; software and hardware validation across driver revisions, devices and macOS builds; and the design, execution and analysis of reproducible benchmark campaigns.

KGP also coordinates community testing, reviews conflicting findings, challenges assumptions and redirects investigations, maintains technical documentation and evidence-backed handovers, and manages release preparation and publication. Development combines AI-assisted implementation and binary analysis with sustained human judgment, hands-on hardware testing and repeated validation on real systems.

- **ChatGPT by OpenAI** — Reasoning, evidence analysis, architecture, experiment planning, runtime interpretation, safety review and documentation.
- **OpenAI Codex CLI** — Repository/source analysis, implementation, tests, reproducible builds, auditing, evidence and release preparation.

## Technical references and acknowledgements

- **NorthKoreanNoodles / maddog860** — Independently identified and published the Ventura direct VIF attach/publication approach. The narrow `attach(this)` → `fAWDLInterface` assignment → `registerService()` method was independently validated in this project under Tahoe + AppleVTD and adopted for the final AWDL baseline.
- **[Mieze](https://github.com/Mieze) / [IntelLucy](https://github.com/Mieze/IntelLucy)** — Prior art informing mapper-aware DMA/lifetime design. The implementation remains source-native; this does not imply copied code or co-development.

## Earlier physical tester reports

- **AX201 tester Fulvio ([@netiave on tonymacx86](https://www.tonymacx86.com/members/netiave.1548699/) / [@Funzio on InsanelyMac](https://www.insanelymac.com/forum/profile/2547924-funzio/) / [@netiave on GitHub](https://github.com/netiave))** — the same tester; earlier conventional-route results helped motivate the comparison beta.
- **AX211 tester Luiz ([@LuizMeliga on tonymacx86](https://www.tonymacx86.com/members/luizmeliga.1658846/) / [@luizmeliga on GitHub](https://github.com/luizmeliga))** — earlier AX211 campaigns demonstrated strong RX on his own system/network.

These acknowledgements concern historical reports, not testing or qualification of ZC28. Klaus confirmed the listed TM/GitHub profiles for both testers and Fulvio’s IM profile. No InsanelyMac account has been confirmed for Luiz.

## AI assistance

AI-assisted work was performed under continuous human direction, review, physical testing and editorial control. KGP controls project and release decisions. This assistance and these acknowledgements do not imply endorsement by OpenAI or any referenced project or developer.

## HE integration and analysis credits

- **[gsfront](https://github.com/gsfront)** — BE200 HE and optional HE SU-Beamformee, link/station firmware setup, HE IE correction, 256-entry HE RX BA/reordering and associated frame-release/rate-histogram work. [Original PR #5](https://github.com/kgp-macPro/AirportItlwm-Tahoe/pull/5), merged as `999d71fafb4788702b527572932dcc9768fb122e`; both original commits/authorship preserved.
- **[laobamac](https://github.com/laobamac/itlwm)** — AX201/AX210/AX211 HE reference and basis for adaptation to our Ventura Legacy path; pinned v2.4.0-alpha [`7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`](https://github.com/laobamac/itlwm/tree/7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e). Capability/firmware/HE-context/TLC and reporting prerequisites were reviewed and adapted, not a wholesale frontend/DMA replacement. GPL notices remain.
- **[REA — Reverse Engineer Anything](https://github.com/morluto/rea)** — local native binary analysis with Ghidra, used to inspect built driver/Legacy handoff and error paths alongside source/runtime evidence. Used REA 6.8.0 / Ghidra 12.1.4; no local binaries or private data sent to external analysis services.
- **OCLP developers and [OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac)** — required framework/root-patch environment, distinct from the Intel driver implementation.

## Historical external comparison control

- **[laobamac](https://github.com/laobamac/itlwm)** — unchanged build 53, packaged as a separately attributed external control. Its original source, pinned SDK and notices are enclosed; upstream artifact/source linkage is recorded. No independent rebuild or external code port into ZC28 is claimed.

## Inherited licenses and attribution

Existing LICENSE, source copyrights and firmware/SDK notices remain unchanged. The [preserved upstream README](docs/UPSTREAM_README.md) retains upstream credits in their original context, including Acidanthera/MacKernelSDK, Apple, AppleIntelWiFi, ErrorErrorError, Intel firmware/iwlwifi, Linux, mercurysquad/Voodoo80211, OpenBSD/net80211/iwn/iwm/iwx, pigworlds, rpeshkov/black80211, usr-sse2 and zxystd. This project attribution does not replace those notices.
