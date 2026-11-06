# P6 — legacy link publication handoff

## Evidence and first known blocker

P5 original archive CRC/full-manifest verified (SHA256 in tracked fixture). This is
the first successful BE200 authentication/association/RSN reference: actual RX
auth status0, association status0, RUN_READY16, RSNDone true, four TX publications
and four positive TX returns, mode2/gen1/reset1/fault0. No IPv4. Key error0 means
submission success, not independent proof of all encrypted data reception.

Read-only configd capture shows DHCP en2 started at22:41:13.426 then media inactive.
Dynamic Store en2/Link stays ActiveFALSE after Wi-Fi UI Connected. en0 is primary
Ethernet IPv4 .35. Legacy bind/SSID -3903 is NotSupported, not timeout; it remains
a separate potential compatibility issue if fixing link publication is insufficient.
The four22:50 screenshots postdate the archive, are copied unchanged/hash-recorded,
and support connected UI/blank DHCP. Displayed AWDL/Continuity is not qualification.
An attempted passive capture was denied /dev/bpf access, including escalated
execution. There is no packet evidence proving either no Discover or no replies.

## Binary-derived ordering contract

The read-only IO80211FamilyLegacy copy in Desktop/EFI-OCLP-Intel matches the loaded
UUID4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB. Full identity, symbol table, relocations and
bounded disassembly are in new evidence/reports and provenance. This binary is
an analysis reference, not modified or shipped. setLinkState@0x9859c arms a one-shot
event flag@0x987ea, then calls controller setLinkStatus via slot0x980@0x98811.
The relocation@0x14c4f0 resolves that slot to IONetworkController::setLinkStatus.
inputEvent@0x9a21e rejects unarmed LinkUp/Down. Down similarly arms before callback.
Same-state setLinkState returns true without callback. setLinkState also posts
Apple80211 notifications: no duplicate synthetic postMessage is needed.
The production supplied SDK slots match (interface two-arg0xab0, getter0xc88).
No binary patch, private offset access or hardcoded vtable displacement is added.

P5 frontend called super::setLinkStatus before setLinkState, caching currentStatus
before the callback. This can drop the early event and suppress the armed callback.
It is an exact source/binary-supported defect and fits configd evidence. Runtime
event interception itself is not traced; DHCP success after correction is untested.

## Effective production change

AirportItlwm.cpp/.hpp only frontend handoff plus explicit member initialization.
The public entry synchronously enters the existing recursive workloop gate when
needed; stack request/borrowed metadata do not outlive runAction. Interface state
transition now runs first, with a gate-owned reentry guard. Its callback delegates
directly to super, once, without repeating queue start/stop/flush or recursing.
Only successful superclass updates advance currentStatus. Successful same-state
framework-origin calls without a nested callback forward the supplied metadata.
Interface bool and gate IOReturn are checked as their declared types; callback
failure is not blindly retried. A late interface failure may follow a successful
base publication: return false without claiming transactional rollback. No private
framework flag reset or speculative recovery is attempted. AWDL mirroring occurs
only after primary success; this is not AWDL qualification. NoNetworkChange/inactive
and absent-interface behavior remain direct base publication.

Applies to the legacy frontend shared by existing families: required framework
contract does not depend on retail model. No transport/DMA/lease/firmware/PNVM,
key ABI, queue geometry, completion/fence/quarantine/restart/admission change.
AirportItlwmV2 remains untouched; normal SPI and private output paths are tested.

## Validation and remaining limits

BE200_LINK_HOST_TEST extracts actual P5 and P6 functions, compiles them with
binary-derived framework/BSD mocks, reproduces P5 lost LinkUp, then tests Up/Down,
framework-origin reentry, failures, duplicate transitions, queue work once,
no-network-change, absent dependencies, metadata,1000cycles and concurrency.
Both normal/private SPI modes run under ASan/UBSan and TSan. Mock behavior is a
reconstruction, not recovered Apple source and not physical evidence. Saved P5
hardware properties/configd facts are a separately labelled fixture.
Existing P5/P4 transport/PNVM/boot/connection/ownership and ZC28 regression checks
remain. Frontend preservation check is narrowed exactly to this handoff/init;
all remaining frontend bytes and shared queue/frozen runner stay identical.

Collector adds configd/IPConfiguration logs and Dynamic Store snapshots. Optional
passive en2 ARP/DHCP capture is max128packets,768byte snaplen,120second input timeout,
no promiscuous/monitor mode or active traffic. BPF permission errors/export exit
are preserved. No per-packet driver logging or registry allocation is introduced.
Hardware test must first check en2 LinkActive, then actual DHCP packets/lease,
then interface-scoped gateway/bidirectional traffic. Do not credit en0 Internet.
If active link is restored but DHCP stalls, packet TX, RX/decrypt/handoff remain
next questions; do not infer DHCP server fault. No broad throughput test yet.

## New P5 physical counterexample and refinement

KGP manually OFF/ON/rejoined. Same P5 UUID E52DFFDF-3CB3-3D6A-8C57-004F22FCB75B,
same boot UUID0E7F32FA-2B96-444C-9A36-7909E586789A. Captured23:27:33; configd
23:21:23.451 link INACTIVE,23:21:40.754 link ACTIVE,23:21:43.163 DHCP BOUND,
23:21:44.794 IPv4/DNS/DHCP publish success. Actual DHCP ACK for192.168.178.192,
mask255.255.255.0/router192.168.178.1,matching en2 MAC. Dynamic Store linkTRUE.
This is stronger than a UI screenshot; DHCP control exchange works on P5 after
the user's transition. General bidirectional data/Internet is not established;
en0 .35 remains primaryIPv4. New23:24 screenshots copied unchanged and viewed.
Generation6/reset4 versus original1/1,5ALIVE/4positiveStops,fault/error0 at capture:
same boot but NOT same driver lifetime. Metadata stage/error can be stale;
KGP_BE200_ERROR6 is not the watchdog's current error0. Do not conflate them.

The P5 actual-function mock also reproduces why a later transition may work:
first early LinkUp is rejected, setLinkState then arms an event whose callback is
suppressed; that unused arm can let the next early transition through. OFF/ON
source sequencing fits the new observation. The internal flag was NOT read from
the live kernel, so the exact live callback chain remains an inference. P6 consumes
the arm at the correct current transition rather than depending on prior state.
The existing bind/SSID API errors alone cannot explain an absolute inability to
obtain DHCP, because this same P5 now has a valid ACK/lease.

Production P6 has no new en2/ARPT/name matching: actual fNetIf/controller objects.
Targeted collector keeps explicitly chosen WIFI_IFACE/IOREG_WIFI_SERVICE, validates
their unique real binding, records it and rejects mismatch/ambiguity/AWDL-only.
Auto-discovery draft is preserved outside committed source; it is not a delivery
prerequisite. All interface-specific collection uses the checked value. Other
machines need their own verified names; no SSDT or ACPI change is required.
