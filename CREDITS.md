# Credits and acknowledgements

## Upstream and foundation

- **zxystd / OpenIntelWireless** — AirportItlwm/itlwm and its Intel wireless foundation.
- **DexterSLamb** — pinned upstream fork used for this project.

## Project development and research

- **KGP / kgp-macPro** — Project lead; Tahoe concept and integration; AppleVTD and Intel/AWDL research; experiments; physical hardware/runtime validation; release.
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

## External comparison control

- **[laobamac](https://github.com/laobamac/itlwm)** — unchanged build53, packaged as a separately attributed external control. Its original source, pinned SDK and notices are enclosed; upstream artifact/source linkage is recorded. No independent rebuild or external code port into ZC28 is claimed.

## Inherited licenses and attribution

Existing LICENSE, source copyrights and firmware/SDK notices remain unchanged. The [preserved upstream README](docs/UPSTREAM_README.md) retains upstream credits in their original context, including Acidanthera/MacKernelSDK, Apple, AppleIntelWiFi, ErrorErrorError, Intel firmware/iwlwifi, Linux, mercurysquad/Voodoo80211, OpenBSD/net80211/iwn/iwm/iwx, pigworlds, rpeshkov/black80211, usr-sse2 and zxystd. This project attribution does not replace those notices.
