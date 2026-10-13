# B1-ZC7-PERF provenance and change ledger

Baseline: frozen ZC6-PERF1 4409d1cd6aaf124960aad122cf4e3b3a914e25c6.
Architecture: existing KGP mapped RX/TX, PC1, conventional route and legacy IO80211.
No external function bodies, classes, layouts or constants are copied or adapted.

## Lean build — KGP-INDEPENDENT; copied/adapted external code: NO
AirportItlwm::outputStart/outputPacket/setLinkStatus, ItlHalService::getPerf1,
ItlIwx PERF1 call sites/field/getter/publisher: remove the observer-only ZC6 delta
against frozen BOOTFIX1, preserving all functional baseline code. This eliminates
77-counter/31-queue atomics, counter argument evaluation, snapshots and publication,
not merely the property. Retained historical Perf1.hpp/tools are unreferenced
development evidence, not compiled runtime instrumentation. Internal marker becomes
AirportItlwm-Tahoe-B1-ZC7-PERF; bundle version remains 1.0.0 (experimental continuity).
No measured overhead or speedup is claimed. Existing DMA/RX/TX/PC1 health stays.
Licensing: existing project notices unchanged; no additional external attribution.
