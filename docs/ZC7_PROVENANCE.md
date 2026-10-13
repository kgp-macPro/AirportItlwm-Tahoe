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

## Pull admission — KGP-INDEPENDENT / APPLE/API-CONTRACT / audit-informed; external reuse: NO
AirportItlwm::outputStart/outputPacket/outputReservedPacket/setLinkStatus; _ifq.cpp/h:
new KgpOutputQueue and single-use reservation wrappers. One common queue spinlock
serializes membership and reservations, including existing internal EAPOL enqueue.
Reserve before macOS dequeue; commit or cancel once. Consumer never tests software
wait state; ring oactive remains independent. Capacity return atomically clears
waiter under the same lock, signals the retained interface outside lock. Published
Apple IONetworkingFamily bb910e2b458939f6b1f53c0fa6907f258b431ea4 drainOutputQueue /
signalOutputThread records a signal before NoResources return and retries, avoiding
the check/sleep race. API behavior still requires physical legacy-stack qualification.
Stop/flush increments epoch, closes pull admission; late reservations cannot enqueue.
Internal EAPOL remains allowed before link-up. Reservations retain queue; wake holds
interface reference, never calls HAL. No new periodic telemetry or per-packet logs.
Invalid packet guards now return immediately, correcting inherited null/post-free fallthrough.
Conceptual comparison with laobamac d695c97 only; no batches, helper bodies or WCL
gate policy copied. Existing GPL licensing applies; no new external attribution.

## RX list handoff — DEXTER/UPSTREAM helper + APPLE/API-CONTRACT; audit-informed; external reuse: NO
_mbuf.cpp::_if_input: replace traversal of handed-off mbuf linkage with existing
_mbuf.h::ml_dequeue before handoff. The helper already belongs to our upstream base;
no new external implementation body. laobamac cad1841 independently highlighted
this same ownership boundary. Pinned Apple inputPacket asserts nextpkt == 0, may
free on missing ifnet, or mutates queue linkage. The old loop reads nextpkt after
handoff and supplies still-linked packets. Test the actual replacement function
with immediate free and linkage rewrite. Both DMA routes use this common delivery.
No RX staging copy, storage/map/cache/final callback/reorder/interrupt change.
No promise that this source defect explains any particular measured RX limit.
The pre-existing no-interface panic behavior is unchanged in this narrow fix.
