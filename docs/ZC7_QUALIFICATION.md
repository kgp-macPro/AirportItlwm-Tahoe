# ZC7 qualification and review boundary

ACTIVE candidate: bounded reservation before pull dequeue, lean runtime, narrow RX
handoff and A-MSDU failure-path corrections. Physical performance UNKNOWN.

## TX invariants
- Same queue lock protects queued membership and outstanding reservations.
- queued + reserved <= unchanged capacity (2048) for all local producer routes.
- Mac output owns every packet until successful dequeue; reservation already exists.
- Commit transfers ownership exactly once; cancel changes capacity only.
- A stop/flush changes epoch; stale tickets cannot enqueue after rearm.
- Internal EAPOL admission respects reserved space and is permitted before link up.
- Software wait state never sets oactive and never blocks dequeue/consumer.
- First available capacity clears wait state under lock; interface signal outside lock.
- Apple's drain loop latches signals before NoResources return, and wakes after it.
- Existing ring oactive/completion restart, gate and deferred drain remain functional.
- Interface and queue references cover callbacks/tickets across detach; no new HAL callback.
- Pull reservation is not DMA ownership. Hardware ownership begins only in existing HAL.

## RX independent assessment
The IWX DMA/cache/worker/reorder functions equal the functional BOOTFIX1 baseline
(except build marker). ZC6 observers removed. Sync remains full 4 KiB per completed
buffer before parsing, not assumed removable or necessarily costly. 512 descriptors,
2048 active preparation entries, bounded retained cache, final callbacks and outside-
gate replenishment unchanged. No fresh map per RX when a returned preparation exists.
Header attach/copy-mbuf for multiple frames, data decapsulation/alignment, reorder and
stack queueing remain potential costs with no isolated timing evidence. No payload
staging architecture is introduced. Two independently modeled correctness fixes:
detach nextpkt before stack handoff; validate A-MSDU before split and honor KPI errors.
Contiguous A-MSDU header skips unnecessary pullup invocation; no measured speedup claim.

## Cold-boot auto-join: unresolved RELEASE BLOCKER
User-reported since ZC4. ZC3/4 attach/init/scan/newstate/enable/disable functions are
byte-identical; only RX storage integration changes in IWX plus models. This does not
exclude changed attach latency, readiness timing or framework interactions. No new
source change claims to restore auto-join. Run the single read-only procedure
tools/diagnostics/ZC7_COLD_BOOT_CAPTURE.py after genuine cold boot BEFORE manual join.
The procedure records boot identity, association, loaded image, mapper/driver health,
DHCP state and existing logs, but does not scan/connect/recover or change settings.
It may require sudo for wdutil/log visibility; denied/redacted records remain UNKNOWN.
Distinguish no visible auto-join request, scan failure, missing association, auth failure,
and link/DHCP publication. Absence of a logged request does not prove OS never asked.
Cold-boot automatic connection needs its own physical validation before final release.

## Deferred mechanisms
No WCL/Skywalk migration, foreign DMA arena, blocking runAction policy or copied
external batching implementation. Existing global ring-full coupling unchanged.
The 32-bit qfull mask versus possible 512 TVQM queue IDs warrants separate review;
observed queue 2 does not establish it as this throughput cause. Do not claim resolved.
External offload-header/IV, completion-tail/length and scan timing changes are not
blanket imports. KGP mapped completion validates tokens/generation already.
Do not remove DMA sync, zeroing or lifecycle checks based on throughput conjecture.
Current models do not establish real firmware, DMA coherency or PHY behavior.

## First physical evaluation (KGP only, after independent review)
Preserve BOOTFIX1/ZC6 rollback bundles. No automated deployment. Prefer 5 GHz/80 MHz
with a wired MacBook iperf3 peer; current .70 peer is Wi-Fi and is not an equivalent
LAN control. Discover current addresses using networksetup -listallhardwareports,
ifconfig en2, ipconfig getifaddr en2, route -n get PEER; confirm peer's wired binding.
Capture kmutil showloaded, dwarfdump UUID, ioreg controller/PCI health and wdutil info.
Bind iperf3 explicitly -4 -B WIFI_IP%en2; verify en2/en0 counter deltas, not route alone.
Only after association/PC1/mapper health: TCP TX/RX P1 8s; UDP TX/RX -l 1200 -b 100M
5s; if healthy one 250M TX or RX and P4 if discriminating. Preserve exact sender and
receiver statistics. Same AP/radio/peer settings for any comparison. No automatic
600M stress campaign. Stop on reset, PC1 fault, mapping fault, loss of connectivity
or persistent serious degradation. KGP alone chooses recovery/rollback.
Unchanged software queue bound means upstream queue drop/congestion remains possible;
new admission does not promise lossless overload or 500 Mbps. Independent review PENDING;
physical runtime NOT YET TESTED; public release NOT AUTHORIZED.
