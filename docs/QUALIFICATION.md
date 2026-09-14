# AirportItlwm-Tahoe 1.0.0 qualification

**RELEASE-QUALIFIED.** The two Tahoe 26.6.2 release gates are complete. Qualification applies to the frozen binary, AX210 reference hardware and restored Ventura legacy wireless environment. It is not a universal compatibility or unlimited-duration stability claim.

## Configurations and grades

The AppleVTD-capable qualification used OpenCore `Kernel → Quirks → DisableIoMapper=false`, together with captured active AppleVTD runtime state. The conventional comparison used the established mapper-disabling configuration (`DisableIoMapper=true`), referred to internally as “Normal Mode.” That is project shorthand, not an official Apple/macOS/OpenCore mode. Configuration settings are KGP-controlled test inputs; `false` alone does not prove AppleVTD is active, and `true` does not prove that all devices in the machine operate without IOMMU translation.

The conventional comparison’s setting identifies the established KGP qualification configuration, corroborated by the documented conventional EFI baseline; it is USER-CONTROLLED, not a fresh read of EFI or an independently supplied conventional-mode configuration capture. Physical PASS and same-byte continuity are KGP’s authoritative observations. “Normal Mode” is not a macOS boot-mode assertion.

| Tested environment | Result | Evidence basis |
|---|---|---|
| Tahoe 26.6.2 / 25G83, AppleVTD, `DisableIoMapper=false` | FULL PHYSICAL + RUNTIME PASS; release gate | Retained runtime captures plus KGP physical qualification |
| Tahoe 26.6.2 / 25G83, conventional comparison, `DisableIoMapper=true` | FULL PHYSICAL PASS; release gate | KGP-controlled / USER-OBSERVED qualification |
| Tahoe 26.7 / 25G229, AppleVTD | Additional scoped compatibility PASS: Wi-Fi, power cycles, reconnect, AWDL/P2P integration, AirPlay and Screen Mirroring | Retained captures plus KGP observations; **not** a second full release qualification |

| Field | Frozen 1.0.0 release identity |
|---|---|
| Bundle / executable | `AirportItlwm.kext` / `AirportItlwm` |
| Identifier | `com.zxystd.AirportItlwm` |
| Short / bundle version | `1.0.0` / `1.0.0` |
| Target / configuration / architecture | `AirportItlwm-Tahoe` / `Release` / `x86_64` |
| LC_UUID | `7039DFF1-232B-3697-8E6E-8EE505044F28` |
| Executable SHA-256 | `1202ec9bf855e944f59cc3742a90c0cf1a0a292642dfe7ed8d091a1a9c435811` |
| Info.plist SHA-256 | `05f027cc5978593c3542942488f27537fe5fb04c48b3817d7ec5025d3bc1c288` |

**No rebuild occurred between 26.6.2 AppleVTD, 26.6.2 conventional comparison and 26.7 AppleVTD testing.** Captured disk hashes and loaded UUIDs independently corroborate the AppleVTD runs; conventional-mode byte continuity is USER-OBSERVED/KGP-controlled.

## Captured power-cycle recovery

| AFTER observation | 26.6.2 / 25G83 | 26.7 / 25G229 |
|---|---:|---:|
| Reset / terminal epoch | 2 / 2 | 2 / 2 |
| Expected / software generation | 3 / 3 | 3 / 3 |
| MASTER_DISABLED matched / fault | 1 / 0 | 1 / 0 |
| Recovery count | 2 | 2 |
| Suspended / blocked / admission | 0 / 0 / 0 | 0 / 0 / 0 |
| Retained TX submitted / normal completed | 256 / 256 | 259 / 256 |
| Retained TX quarantine | 0 | 3 |
| Command published / CmdDone / reuse | 25 / 25 / 25 | 25 / 25 / 25 |

Both AFTER captures show active en2 with IPv4, active awdl0 and P2P/AirLink/wifip2pd integration, source-bound ping 5/5 and interface-bound HTTPS success. Error fields are clear and bounded RX snapshots remain healthy. These are retained snapshots, not counts of all traffic. Fresh bound traffic independently establishes recovery.

The additional 26.7 result demonstrates recovery with three old backings retained in quarantine and new-generation admission open. This supports the checked terminal/reset, completion fence and new-generation source model (**INFERENCE**); it does not prove every hypothetical late completion impossible. No stale completion was deliberately injected.

## Physical and AWDL observations

KGP additionally qualified repeated OFF/ON (including menu-bar control), Forget Network/reconnect, Sleep/Wake on 26.6.2, bidirectional AirPlay and Screen Mirroring. Active awdl0, registered/matched/active IO80211P2PInterface, AirLink and wifip2pd have retained registry support. Separately reported packet samples showed real IPv6/mDNS `_airplay._tcp.local`, `_raop._tcp.local` and `_airplay-p2p._tcp.local`; the conventional-mode sample contained 50 packets, zero kernel drops. Packet-sample details are USER-OBSERVED reports, not independently decoded raw packet evidence in this public summary.

The `networksetup` “not associated” result may disagree with active en2, valid IPv4 and successful bound traffic. The combined evidence establishes functionality; that API discrepancy does not negate it.

## Evidence limits

| Grade | What it establishes |
|---|---|
| SOURCE-PROVEN | Preserved PC1/2Q functional lineage, prepared backing/lifetime policy, NKN path, legacy target and exact SDK pin |
| BUILD-PROVEN | Successful retained single 1.0.0 Xcode build and focused host/sanitizer checks; a CI build is a separate result |
| RUNTIME-PROVEN | Retained same-byte AppleVTD identity/binding/interface/recovery/traffic captures |
| USER-OBSERVED | Conventional comparison, additional physical cycles/features and separately reported packet samples |
| INFERENCE | Observed recovery supports the source model within tested conditions |
| UNKNOWN | Universal hardware/platform support, exhaustive stale-completion behavior, full unload qualification and long-duration stress |

Uncertain TX backing is retained in a finite pool until reboot; exhaustion and failed terminal checks remain fail-closed. Raw captures, internal reports and local identifiers are retained outside this public source repository. Additional 26.7 tests do not expand the release gate or the qualified 26.6.2 Sleep/Wake scope.

**Important scope note:** This kext does **not** claim to restore AirDrop, Continuity Camera, or Personal Hotspot. These features, and Intel BE200 support, remain future development gates. Broader post-1.0.0 development is planned to resume starting in October 2026; this is a development plan, not a promised completion date or feature order.
