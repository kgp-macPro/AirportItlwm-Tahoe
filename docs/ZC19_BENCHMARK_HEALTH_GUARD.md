# ZC19 benchmark publication freshness correction — 2026-10-18

This is a benchmark-tool correction, not evidence of a ZC19 driver defect.
No Kext source, wire schema, compiled input or binary changes are included.
The existing Release remains built from
`f18801f700b433199cf45808eaa674c92ec4f422`, tree
`7269b802b1be0af1f2de546f93ba766c1abd1b82`.
The subsequent tooling commit does not identify a new compiled candidate.

## Source-proven cause

At the build commit, `tools/ZC19_3x14_PORTABLE_IP_PROMPT.sh` called `capture`
for INITIAL, each round BEFORE/AFTER, and FINAL: eight snapshots total.
INITIAL and ROUND_01/BEFORE have no minimum separating delay; likewise a
round AFTER and the next BEFORE. The old `check_snapshot` rejected every
publication counter less than or equal to INITIAL. Thus two valid reads of
the same published OSData caused a false failure. Comparing only to INITIAL
also missed a counter rollback that remained above the initial value.

`AirportItlwm/AirportItlwm.hpp:38` sets `kWatchDogTimerPeriod` to 1000 ms.
`AirportItlwm::watchdogAction` (`AirportItlwm.cpp:455–463`) calls
`ItlIwx::zc19Publish` and re-arms that timer after the callback. Initial
arming is at `AirportItlwm.cpp:678`. This is a nominal cadence, not a
promise that every boundary read observes new data.
`ItlIwx.cpp:17152–17189` implements the snapshot and publisher: a successful
gated snapshot increments word 3, while `attemptAction` skips a busy gate;
`setProperty` can also fail. Consequently repeated publication values alone
across adjacent snapshots do not prove stale or defective driver operation.
These source anchors refer to the unchanged build commit.

## Exact tool behavior

- Keep INITIAL as the mode/generation/reset/command-epoch reference.
- Track the last **accepted** health JSON separately in `PREVIOUS_HEALTH`.
- Accept an equal publication value at boundary reads.
- Reject any decrease relative to the last accepted snapshot, even when
  the value still exceeds INITIAL. Counter wrap is deliberately fail-closed.
- Require FINAL to be strictly greater than INITIAL. FINAL may equal the
  preceding snapshot. An entirely unchanged campaign fails before
  `finished_ok=1` / COMPLETE. Missing final references fail closed.
- Keep actual mapped/conventional mode, exact candidate marker, loaded UUID,
  schema/record size, named fault flags and epoch checks intact. Failed
  checks do not update the previous reference.

This establishes observed publication progress over the campaign. It does
**not** establish continuous watchdog liveness, freshness at every instant,
or absence of transient faults between samples. No timing threshold, extra
snapshot, wait, polling loop, profiler, network query or traffic change was
added. ZC8 and schema10/2 guard bodies remain unchanged.

## Direct validation

`python3 -B tools/host_tests/ZC19_BENCHMARK_GUARD_TEST.py` executes the actual
embedded Python, not a reimplementation. It reproduces the original failure
from the build commit and covers both modes, adjacent repeats, final equality
to the preceding read, counter rollback above/below INITIAL, unchanged entire
campaign, missing references, faults, mode/marker/epoch changes, malformed
records, and loaded UUID mismatch. It also executes the actual shell snapshot
functions and initial/three-round/final control flow with fixture-only ioreg
and no-op traffic substitutes. No physical benchmark or live registry read
is performed. Tests assert eight captures, 42 simulated cases, final failure
before COMPLETE, and no reference update after rejection.

`tools/host_tests/ZC19_SOURCE_CHECK.py` delegates its benchmark checks to that
suite; its existing driver-retention and emitted-fence checks are unchanged.
Sanitizer runs and a new Kext build are unnecessary for this shell/Python-only
change. Host fixtures cannot prove physical watchdog progression; the real
FINAL health check enforces it when KGP performs a campaign.

Validation on 2026-10-18: 13/13 focused tests PASS in both supported DMA
modes where applicable; Bash syntax PASS; existing `ZC19_SOURCE_CHECK.py`
retention and emitted-fence checks PASS (exit 0). The shell campaigns are
synthetic, not physical 42-test measurements.

## Binary and distribution provenance

Unchanged executable UUID: `1C2D5EBC-764D-3EF2-9714-11ADF9A2016A`.
Unchanged executable SHA-256:
`56704aaf87536e5812706ecfb97498d1fd2a09cf0b45941e74f2eb3f8e8ad624`.
The original `B1-ZC19-PERF` archive, its original runner, build records and
SHA256SUMS are preserved. The corrected runnable copy is distributed separately:
`~/Desktop/AirportItlwm/TOOLS/ZC19_3x14_PORTABLE_IP_PROMPT.sh`.
Use that copy for subsequent ZC19 campaigns, still targeting the original Kext:

```bash
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/ZC19_3x14_PORTABLE_IP_PROMPT.sh" ZC19
# Only after KGP independently selects conventional mode:
EXPECTED_MODE=conventional /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/ZC19_3x14_PORTABLE_IP_PROMPT.sh" ZC19
```

The same prompt selects the current wired peer IP. Benchmark protocol and
output/evidence destinations remain unchanged. No deployment or physical test
is performed by this tooling correction. No upstream code was imported.
