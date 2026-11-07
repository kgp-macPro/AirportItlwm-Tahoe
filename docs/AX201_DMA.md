# AX201/CNVW DMA documentation correction — 7 November 2026

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

The present correction restores the already established policy in those three
current documents and the live release body. Historical records remain tied
to their original revisions. Driver source, release binary/tag/assets, r13 tools,
profile identities and safety checks are unchanged.

## Current commands

Use the [AX201 r13 mapped/conventional commands](benchmark/README.md#ax201cnvw-mapped-and-conventional-comparisons)
with `AX201-2.0.0` and the exact loaded 2.0.0 bundle. Fulvio’s main expectation is
`EXPECTED_MODE=mapped`; a separately configured comparison can use `conventional`.
The expectation checks the actual route; it cannot select or change it.

The unchanged original r13 asset contains the old sentence in START_COMMANDS.txt.
The corrected online guide and release body explicitly supersede that sentence;
the archive and its checksum are preserved.
