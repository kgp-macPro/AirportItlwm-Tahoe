# AX201/CNVW DMA documentation correction — 7 November 2026

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
