# P7 OFF/ON recovery correction — private experimental candidate

## Baseline and observed hardware behavior

Complete frozen P6 832a02cc779258d0e007c782d592a4ece907f076. P6 loaded UUID
9CA6A692-EAD7-3629-9C45-E7DDA7D190B2 confirmed; bootUUID
70D276B6-06D5-49EA-909D-D85D469B3EAF,boottime29October23:56:07.
KGP USER-OBSERVED boot autojoin withoutOFFON,one manual sleep/wake autojoin and
Internet68.2/28.0. SporadicOFFON scan/join failure remains a P6 knownissue.
Requested00.19.23 screenshot absent at capture;do not claim it inspected.

The read-only capture was already recovered RUN gen11/reset8/mode2/fault0.
Its retained connection history contains earlier gen4/reset3 and gen8/reset6:
RUN_STOP17 then about1.0s later firstfault30/error35 and RUN_STOP error35.
FLOW2J retained first failure is code0x11e(TXPATH_FLUSH),payload8,status35.
These are same-host clock records;no unsynchronized one-way latency inference.
Flags0x800 indicate only stale MLD management-queue presence at these points.
Zeros at the later snapshot do not erase those failures. Trace retained64 of112
events/48overwritten;FLOWprefix128 overflow1407 at earlier snapshot (later
snapshot may progress). No complete live call stack or every OFFON failure captured.

## Source-supported mechanism and correction

Pinned HuajiFruit/AirportBZ0ccab1627764406861fb9eb0a06ca36e31b4440f has gated
init/stop and generation fences in newstate;our inherited existing-method bodies
missed them. Its MLD leave helper waits for TX queue drain through commandSleep.
That releases/reacquires the gate: radioOff/reset can finish while the old task
still holds cached RUN/target state. P6 resumes into RUN_STOP/flush without
checking generation and can submit a command into stopped firmware,then requeue
or publish obsolete state. This fits the captured fault;exact live interleaving
remains a source-grounded inference. No claim all sporadic failures are explained.

P7 changes are BZ-only except extension of its already BZ-specific cleanup:
- iwx_newstate_task stamps generation/reset epoch and checks after leave,run-stop,
  deauth and before final publish/requeue. Current-lifetime errors still schedule
  recovery;stale continuations return. No unconditional blanket error suppression.
- iwx_send_cmd checks bzCommandReady before ring/response/ledger access:hardware
  transport-owned,notSHUTDOWN,notfaulted,undergate. Refused commands returnENXIO,
  allocate nothing,publish no descriptor/doorbell and fabricate no completion.
  PC1 packet-suspend is deliberately not command-admission:boot commands run
  before final packet readiness. Existing command ownership ledger unchanged.
- Entire BZ iwx_init/iwx_stop runs under the existing recursive workloop gate.
  Stop invalidates generation BEFORE reset/cancel;net80211 state cleanup also
  finishes before SHUTDOWN is cleared. Failed gate dispatch closes admission.
  Existing PC1 expected-generation math and positive reset proof retained.
- bzStoppedCleanup clears stale MLD management/key/pending-mask claims ONLY
  after the existing positive STOP_MASTER+terminalepoch+gate checks. Uncertain
  hardware stop retains backing and those claims,blocks further admission.
- Scan submission is latched with generation+reset epoch. Init tests that
  predicate before sleeping and after waking,with bounded2000msbudget(existing
  1000mscommandbudget+1000mstaskmargin). Already enteredSCAN or progressedAUTH/
  RUN is success for that lifetime;lost/spurious wake and state transience do
  not authorize success or a blind timeout/reset. Failed/stale scan cannot latch.

No DMA allocator/remap,buffers,geometry,fences,firmware/PNVM/API/SDK/key layouts,
MSI-X policy or P6 object-based frontend link fix changed. No en2/ARPT/CNVW
dependency added. Existing AX201/AX210/AX211/IWM/IWN routing is unchanged.
Rare bounded connection events11stop/12stale/13scan/14inactivecommand extend
existing preallocated history;no per-packet logging or registry publication.

## Validation and remaining qualification

Actual old/new functions are compiled into host fixtures:state task,full stop,
init,scan,predicates and wait. P6 post-leave reset/flush fault and pre-sleep scan
wake failure are reproduced at mocked boundaries;P7 rejects stale continuation
and accepts same-lifetime scan progress. Gate/firmware behavior is a simulation,
not recovered live callstack or hardware proof. Saved P6 wire/properties/flow
are checked separately. Actual positive/uncertain cleanup tests retain safety.
Inherited boot,PNVM,MSI-X/ABI,ownership,connection,P6 frontend and ZC28 tests retained.

P7 physical OFFON behavior is UNTESTED. P6 isolated boot/sleep successes do not
qualify repeated cycles,all modes,families,performance or duration. WAN variability
remains open. Alpha publication is explicitly ON HOLD until KGP's next hardware
test;no push/tag/release. Existing1.1.0Beta andv1.0.0stable remain unchanged.
