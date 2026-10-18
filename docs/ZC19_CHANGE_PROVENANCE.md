# ZC19: ZC8 service architecture with selected later TX and correctness changes

Status: experimental, not physically validated. Decision date: 2026-10-18.
Baseline: db22bb90b72b440b657246940fd8da17dbee23a4 (ZC8), tree
004db4fbdb5972e2617b9a9cc5aff8d83564e6f7. Shared existing ZC8 object database.
Branch: experiment/track-b1-zc19-perf-20261018. No historical ref rewritten.

## Equal-priority architecture decision

| Criterion | A: start at ZC18 | B: start at ZC8 (selected) |
|---|---|---|
| ASIX physical reference | TX 471–483; RX P1 451–469/P4 420–428 | TX 255–263; RX P1 568–591/P4 554–569 |
| Known TX mechanism | Cached coherent packet-control RAM already present | Same allocator/queue sites independently separable from observers and service handoff |
| RX architecture | Would need remove wrapped observers and unwind service-depth/deferred-feedback interactions | Original notification, host delivery, credits, immediate completion restart and frontend queue bodies remain explicit baseline |
| Safety | All later fixes already present | Audit and retain restart/deadline/BA fixes; ZC8 already owns core DMA/PC1/mbuf protections |
| Dependencies | Removing observer wrappers requires disentangling behavior embedded in wrappers | Selected source hunks apply without bringing wrappers/queue changes |
| Experimental risk | Less safety-port work; uncertain extent of reverse surgery | More safety reconciliation, but smaller explicit final mechanism set and source-derived regression models |

B is chosen for cleaner dependencies and the physically demonstrated RX foundation,
not because a specific post-ZC8 change is proven defective. A remains valid in principle.
No historical code is declared unsafe solely because an optimization did not help.

NEW original ZC13 ASIX campaign 035835_lrrSvE is important: 42/42, SHA256
97a068a58f491f9687ecbde1f042b0f3eb2d89a8697f24f3680ca56d9a3dd9d1.
TX P1/P4 473.101/474.528; RX 490.892/413.661, UDP RX600 601.630 Mbps.
Thus the ASIX RX discrepancy is already present by ZC13, before ZC14–18 RX changes.
Candidates include ZC10 observer cost, ZC12 wake/service sequencing, changed ACK
service timing from cached TX, or unrecorded runtime state. None is proved causal.
ZC8 ASIX RX P4 sustains roughly 520–591 Mbps full seconds; ZC13/18 often 390–460,
not merely different short tails. ZC8 RX P1 R1 has all 439 sender retransmissions
in startup and later ~1.7 MiB cwnd; other fast ZC8 runs also have thousands of
retransmissions. Counts alone do not locate or explain loss. Source JSON and
remote sender text, not rounded receiver summaries, are preserved in evidence.

Realtek versus ASIX is a sender-configuration observation, not a proved vendor fault.
The old Realtek write-cap A/B/A yielded 216.26/211.80/214.30 Mbps; no gain.
System Information and intrusive 1 Hz registry sampling remain prohibited in the
performance protocol. No new external fork or firmware code is imported here.

## Exhaustive functional change map

All selected source originates in this same KGP Git history. Original commit author
for the commits below, verified with Git: kgp-macPro
<218073525+kgp-macPro@users.noreply.github.com>. This is selective adaptation,
not an upstream merge, not laobamac/deeveedee code and not new authorship credit.
Existing project/file licenses and notices remain untouched. No new license boundary
is crossed. Git records the exact selected diff; copied test harnesses retain their
historical names to make provenance visible. The final report identifies local commits.

| ID | File/functions | Original complete commit | Before → after; invariant; test; expected effect/risk |
|---|---|---|---|
| TX1 | ItlIwx.cpp `iwx_tx_control_alloc`, `iwx_alloc_tx_ring`, `iwx_tvqm_enable_txq`; .hpp declaration | 31ac210425f497d3d8192982a643807361f1c6b2 | Packet TX TFD/command/byte-count RAM inhibited → checked coherent cached allocation via existing prepared mapper-aware backing. Queue0 firmware-command allocation unchanged. Positive selection, aligned complete IOVM segment, descriptor identity and failure cleanup required. `ZC13_CONTROL_HOST_TEST`, TX_ZC5. Expected TX CPU-cost gain physically established for ZC13 cohort, not guaranteed for ZC19. CPU-cache contract remains x86_64; host tests do not prove hardware coherence. |
| TX2 | ItlIwx.cpp `iwx_dma_publish_barrier`, `iwx_enable_txq`, `iwx_tvqm_enable_txq`, `iwx_tx`, `dexterOriginal_iwx_tx` | helper a635d22c3ba60cf25ca9b313f6c67390348eac82; sites 87b05e50e966f87b2fa74c7a596dbfc2b228f5c5 | Empty pinned OSSynchronizeIO → explicit compiler/hardware fence before queue enable and MMIO doorbell. No RX cache flushing or completion inference. Exact source assembly + final binary inspection. Required ordering, not claimed throughput gain. |
| RX1 | ItlIwx.cpp RxZcPool `takeReturned`, `replenish` | 008c159c56f1bc2ba1791ff787b02cdfd55596fa | At full prepared cache exchange final-returned cached preparation with completed incoming off-ring record rather than unmap incoming. Worker-exclusive Entry ownership; acquire final-return flag before new view; retain incoming even while host owns it. Cache bound/scan count unchanged. `ZC15_RX_CACHE_EXCHANGE`, `RX_ZC5`, `ZC16_RX_SUSPEND`, `ZC7_RX_HANDOFF`. Fewer prepare/complete operations, only mapped RX; no promised TCP gain. |
| LIFE1 | AirportItlwm.cpp `zc16AdapterPowerGated`, `enableAdapter`, `disableAdapter`; AirportSTAIOCTL.cpp `zc16SetPowerGated`, `setPOWER`; ItlIwx.cpp `enable`, `iwx_activate`, `zc16PublishRun`, `iwx_init`; matching .hpp declarations | 120ab2c61f0bbe0e6462f4eeea9fd7e5da29a7f2 | Serialize IWX enable/disable/power with existing main gate; propagate resume/readiness errors; stop interrupted init even before RUNNING; publish RUN/scan only if current generation, UP and PC1 state allow. PM wake policy unchanged, no automatic restart loop. Existing STOP_MASTER/quarantine/command ownership unchanged. `ZC16_LIFECYCLE`, `ZC16_COMMAND`, RX suspend. Prevent stale startup, not a physical wake-fix claim. |
| RX2 | ItlIwx.cpp `zc16ArmReorder`, `iwx_release_frames`, `iwx_reorder_timer_expired`; if_iwxvar.h reorder deadline | 120ab2c61f0bbe0e6462f4eeea9fd7e5da29a7f2 | Anchor missing-frame timer to occupied-slot timestamp; do not extend pending earlier deadline on nonprogress notification. Existing frame release/sequence/100ms policy retained. `ZC16_REORDER` compares frozen counterexample and current source, wraps, AMDSU/teardown, concurrency. This is correctness, not proven RX-speed fix. |
| RX3 | ItlIwx.cpp `iwx_oldsn_workaround`, .hpp bounded BA counters | f92775e65f743e3a84d9cd3634fd8c955125bc78 | Reset previous-drop flag at GP2 transition so threshold counts consecutive bad groups. No unconditional DELBA suppression or sequence acceptance change. `ZC18_OLDSN` with original-source negative control. No observed historical trigger in cited focused ZC18 captures. |
| RX4 | ItlIwx.cpp `iwx_rx_ba_session_expired`, `iwx_sta_rx_agg`; if_iwxvar.h timeout | 177d39aa865e6ac6d45127bc4f50c0f1a9ceb5c6 | uint16 truncation → uint32 microseconds, actual request-derived inactivity deadline, round up remainder. Zero remains disabled; ADDBA on-wire policy unchanged. `ZC18_SESSION` tests all65536 TU values and stale/disabled/concurrent cases. Separate from frame reorder deadline. |
| COMPAT1 | ItlIwx.cpp `drain2MSetup`, `iwx_start` | cda7ae51561dee54ac21b01a70d60755205f5eb6 | All supported IWX get existing fenced deferred retry when nonblocking start gate is busy; AX201 final batch cannot depend on future traffic. No service-depth handoff imported. `ZC12_DRAIN` executes current functions in queue/lifecycle mocks. AX201 mapped remains refused. |
| HEALTH1 | ItlIwx.cpp `zc19Snapshot`, `zc19Publish`, .hpp fixed health array; AirportItlwm.cpp `watchdogAction` | independent ZC19, callback-owner rule from c1b1d9a72aaae80ee3819996ecaa35a0d568c0ed | Existing watchdog publishes 256-byte schema19/1 under nonblocking snapshot gate, provider copy after gate; HAL explicitly arg0 because gate belongs to controller. No packet observer, ring scan, node read, new worker/timer. `ZC19_HEALTH` + actual runner guard fixtures. Health coverage retains named fault/epoch checks, no false ZC10 compatibility claim. |
| ID1 | ItlIwx.cpp `dma5Select`, `iwx_alloc_rx_ring` marker | independent ZC19 | Unique AirportItlwm-Tahoe-B1-ZC19-PERF marker only; qualification/allocator unchanged. Exact source check. |

## Retained and deliberately not imported

* Retain ZC8 positive mapper qualification, fail-closed unknown state, AX201 mapped
  exclusion, RX replacement-before-delivery/final host reference, prepared supply
  worker, bounded cache, TX positive completion/tokens, PC1/STOP_MASTER quarantine,
  generation fencing, command state machine and conventional packet bodies.
* Do not import ZC10 per-packet/ring/rate observers or ZC18 TCP observers. These are
  not needed by TX1 and entail additional gate checks, clocks/atomics and ring scans.
  Their physical causal role is UNKNOWN. The unsafe historical ic_bss event reads
  never enter this branch. ZC11's gate-context correction is applied to HEALTH1.
* Do not import ZC12 service-depth producer wake suppression or outputStart oactive
  bypass. ZC8 reservation/ownership/backpressure and bounded32 drain remain. This
  removes one pre-ZC13 scheduling variable, not a claim that ZC12 was incorrect.
* Do not import ZC14 cached RX free-ring allocator (physical scan regression cause
  unresolved) or early RX credits. ZC8 inhibited allocator and credits AFTER host
  handoff stay byte-identical. Early credits may be safe/beneficial, but not necessary
  for cached TX or cache exchange. Its exclusion cannot explain ZC13 ASIX by itself.
* Do not import ZC17 feedback-batch deferred completion restart. ZC8 immediate
  completion capacity/restart behavior remains; positive retirement unchanged.
  No claim that ZC17 is unsafe, nor that exclusion alone restores RX throughput.
* No firmware/rate/ring-size/TCP-window/ACK classification change; no changes to
  net80211 input, checksum/decrypt/duplicate handling, DMA address selection,
  conventional packet RX or IWM/IWN HALs.

## Device/mode scope

AX210 and AX211: TX1/2 and shared lifecycle/BA/reorder corrections in both positively
selected modes; RX1 only mapped. AX201/family22000: conventional only, same shared
TX/control and lifecycle/reorder/BA corrections plus COMPAT1; mapper exclusion
unchanged. Other supported IWX follow original attach/device qualification, no new
mapped support. IWM/IWN source and queue remain unchanged; controller dispatch only
adds IWX dynamic-cast branches. The same legacy IO80211 integration is preserved.
Physical AX211/AX201 regressions cannot be excluded by models. ZC19 has no physical
pass yet. One old ZC18 mapped wake pass is not this candidate's qualification.

## Health schema and benchmark compatibility

KGP_ZC19 OSData, 32 little-endian uint64, schema19/1,256bytes:
0schema,1version,2bytes,3publication count,4monotonicns,5mode(0unknown/1conventional/2mapped),
6generation,7family,8PC1fault,9TXblocked,10commandFrozen,11PC1resetEpoch,12commandEpoch,
13PC1suspended,14RXfault,15if_flags,16..27 historical12 BA counters,
28snapshot gate misses,29property failures,30..31reservedzero.
BA slots16..27: GP2changes,oldgroups,endedstreaks,thresholdDELBA,samegroupsuppressed,
peakstreak,nonzerotimeoutsetup,zerotimeoutsetup,callback,rearm,expiry,lastnonzeroTimeoutUs.
Cumulative instance state; epochs expose reset, no per-test reset. Command epoch is read atomically, matching existing reset writers. Setup slots6/7/11
are atomic; all other BA reads/writes under gate. Snapshot is not a globally atomic
view of firmware/host traffic. A busy gate skips publication and retains prior copy.
No KGP_ZC10/ZC17/ZC18_TCP property is fabricated. Existing legacy properties remain.
`tools/ZC19_3x14_PORTABLE_IP_PROMPT.sh` copies the corrected portable runner and ONLY
adds schema19 guard: actual mode, marker, fault state, epoch consistency, fresh
publication. Existing frozen runner unchanged; no extra captures or protocol changes.
All helpers are optional user-run physical tools; validation tests use synthetic plists.

## Falsification and regression gates

With same ASIX sender and no system_profiler: short TCP RX P1/P4 then TX P1/P4,
UDP TX500/RX600. Support: RX approaches ZC8≥550 and TX≥400 in same candidate;
falsify integration hypothesis if TX remains~260 or RX remains~420, or safety/
association fails. This multi-change reconstruction cannot identify one causal
optimization from a success or failure. Do not claim more than combined feasibility.
Next full42 only if smoke passes. Both modes and AX211/AX201 qualification remain
required before release. Never install/boot/test autonomously. FrozenZC8/ZC18 are
unchanged rollback references. No speed, stability, or sleep/wake result is claimed
from a clean build or mocked sanitizer tests.

Additional exact interval finding: ASIX RX P4 after second2 has only185/104/116
reported sender retransmissions in ZC8 mapped rounds, versus1531/1350/1017 in the
first ZC18 mapped campaign and1122/1198/1063 in ZC13. Rounded remote sender
cwnd median per stream after second2 is316/304/387KiB in ZC8, versus140/146/137
in ZC18 and118/148/164 in ZC13. These are transport feedback differences, not
proof of the initiating drop layer. Remote RTT/advertised window are unavailable
in these text records. Full per-flow/per-second decode is in the evidence directory.
