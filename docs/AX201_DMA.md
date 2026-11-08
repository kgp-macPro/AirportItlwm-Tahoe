# AX201/CNVW DMA documentation correction — 8 November 2026

## DMA configuration: mapped and conventional

**AX201/CNVW supports mapped DMA. Fulvio's regular configuration is mapped
Mode 2; conventional is a separate comparison, not an AX201 requirement.**

| DMA configuration | OpenCore Kernel → Quirks → DisableIoMapper | Runner expectation | Actual driver mode |
|---|---|---|---|
| mapped | **false** | `EXPECTED_MODE=mapped` | **Mode 2** |
| conventional | **true** | `EXPECTED_MODE=conventional` | **Mode 1** |

**Reboot after changing the EFI quirk or Wi-Fi boot arguments.** These are
manual test configurations. Record active AppleVTD separately for mapped;
the flag alone is not runtime proof. `EXPECTED_MODE` checks the loaded driver's
actual mode and cannot select it. Keep every identity, binding and health guard.

## Additional 2.0.0 mapped evidence from 7 November 2026

The newly supplied original `AX201-2_0_0_AX201_3X20_mapped_20261107_185335_rxIkoY.zip`
completed **60/60 local tests plus online**, with all **374 manifest entries**
verified. All eight health records and raw boundaries report **mapped Mode 2**,
with unchanged generation/reset epochs and no health faults at those boundaries.
The loaded release UUID/hash match 2.0.0. Runtime HE counters and valid firmware
rate reports establish an HE capture, with 160-MHz/two-stream TX observed.

This is **one** standard-matrix HE run. The BF setting and wired peer's negotiated
Ethernet media were not supplied; it is not a three-mode VHT/HE/HE+BF comparison
or a controlled HE/BF qualification. Do not call it VHT solely from the runner
filename or infer a 1/2.5-Gbit/s peer from throughput. The aggregate-only
[original-data audit](AX201_2_0_0_20261107.json) preserves actual receiver/sender
rates and losses without publishing raw registries or private identifiers.

## Alpha-2 evidence from 4 November 2026

Fulvio’s original **AX201_test_results.zip** contains two complete **1.2.0 Alpha 2**
3×14 campaigns. This is historical Alpha-2 validation, **not a 2.0.0 hardware test**.

| Configuration | Actual DMA telemetry | Completion | Manifest checks |
|---|---|---|---:|
| mapped |Mode 2 in all eight Health records and all eight raw IORegistry boundaries|42/42 + online PASS; exit0|283/283|
| conventional |Mode 1 at the same eight boundaries|42/42 + online PASS; exit0|283/283|

Both use the same CNVW / PCI8086:7a70 / subsystem8086:0074 binding and recorded en0.
All eight identity boundaries per campaign retain Alpha-2 UUID
`E4414AAB-1D58-3E40-93A5-6838D3CDCD20`, executable SHA256
`8e4a38121b06941630afa98728b245cbaeec813c14139c46715dc44723461f63`.
This verifies archived frozen identity; it is not an independent source-to-binary rebuild.
The archive CRC and both sets of checksums pass. Original ZIP SHA256:
`1770c5cb8270f4d80f86b03cf4bb05b185b8ff6481699d0d7bec81036e0c3cff`.

Campaign directories:

- `AX201-ALPHA2_AX201_3X14_mapped_20261104_190359_4ylfzR`
- `AX201-ALPHA2_AX201_3X14_conventional_20261104_191309_anIld7`

Three-round means recalculated from original TCP **receiver** values (Mbit/s):

| Alpha-2 route | TX P1 | TX P4 | RX P1 | RX P4 |
|---|---:|---:|---:|---:|
|mapped|397.2|399.9|839.8|837.8|
|conventional|238.3|262.5|791.1|717.0|

Mapped is higher in all four means in this pair. This is not a universal performance
claim or a new HE/BF qualification. The 3×14 protocol is not relabelled as the current
r13 3×20 protocol. See the [audited records and per-round TCP data](AX201_ALPHA2_20261104.json).
No new campaign is needed to establish the already proven mapped route.

## Earlier corroborating evidence

Fulvio’s successful historical ZC28 campaign records actual **mapped Mode 2**.
The first-party original-data audit is retained in
[P8 corrections: active AX investigation](private-be200/P8_CORRECTIONS.md#active-ax201ax210ax211-investigation).
It identifies PCI8086:7a70/subsystem8086:0074 at CNVW, with the original
23 October ZC28 campaign and its own framework/build context. The separate
24 October external53 comparison lacks numerical DMA telemetry; its mapped
configuration is a declaration, not an independently measured Mode 2.

This established evidence withdrew the blanket “AX201/CNVW conventional only”
restriction. It does not claim every retail AX201 platform uses mapped DMA or
that a historical VHT result validates 2.0.0 HE/BF. Actual PCI/subsystem, firmware,
binding and runtime mode remain decisive. Conventional comparisons remain valid.

## Regression origin

Commit `c4be2dfb59bb4f7aa5c7eee7804753efd628ecc3` (6 November 2026),
“Prepare Tahoe 2.0.0 documentation, verified campaign results and benchmark tools r13”,
reintroduced the blanket conventional statement in README.md, INSTALL.md and
this release’s benchmark guide. The parent guide already explicitly said that
AX201 is not automatically conventional and offered a mapped command. The
2.0.0 release body repeated the regressed wording during publication. This is
a documentation reconciliation error, not new adverse hardware evidence.

The 7 November correction (commit `efa5f863b94975aa09da3905706600afb37ef355`)
restored the established policy in those three current documents and the live
release body, but left the downloadable start commands and local release notes
unchanged. The 8 November completion below closes those remaining entry points. Historical records remain tied
to their original revisions. Driver source, release binary/tag/assets, r13 tools,
profile identities and safety checks are unchanged.

## Current commands

Use the [AX201 r14 mapped/conventional commands](benchmark/README.md#ax201cnvw-mapped-and-conventional-comparisons)
with `AX201-2.0.0` and the exact loaded 2.0.0 bundle. Fulvio’s main expectation is
`EXPECTED_MODE=mapped`; a separately configured comparison can use `conventional`.
The expectation checks the actual route; it cannot select or change it.

## Completion of the instruction correction — 8 November 2026

The first online correction did not repair every current entry point:
`RELEASE_NOTES_2.0.0.md` still contained the blanket conventional claim,
`README_TESTS.txt` still pointed to Alpha 2/r12, and the downloadable r13 package
retained its wrong `START_COMMANDS.txt` sentence. Those active instructions are
corrected together in the r14 instruction package and current repository docs.

The r14 package keeps all five executable r13 runners, helper resources, profile
identities and guards byte-identical. It changes the packaged instructions and
package metadata; actual runner provenance still identifies r13. The old r13
archive/checksum remain historical, and driver binaries/tags remain unchanged.

The prescribed configurations are explicit: `DisableIoMapper=false` → mapped
Mode 2; `DisableIoMapper=true` → conventional Mode 1, with a manual reboot after
changes. VHT requires a 1-Gbit/s peer; HE and HE+BF require a 2.5-Gbit/s peer for
these comparisons. Record actual negotiated media, boot arguments and PHY/width
per campaign. These rules also belong at the top of the canonical handover.

The stale `README_TESTS.txt` was last rewritten in `dbec9b8dddbac06c5f9a656df2c1500caecd3380`
for the then-current Alpha-2/r12 entry flow. It was not updated during 2.0.0
publishing. The wrong r13 `START_COMMANDS.txt` and local release-note claim
originated in `c4be2dfb59bb4f7aa5c7eee7804753efd628ecc3`. No adverse DMA evidence
caused these statements; the failure was incomplete documentation reconciliation.

The original r13 ZIP and `SHA256SUMS.txt` remain unchanged historical assets.
Use [r14](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-Benchmark-Tools-r14.zip), its
[ZIP checksum](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-Benchmark-Tools-r14.zip.sha256) and the
[combined release/r14 checksums](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/SHA256SUMS-r14.txt).
