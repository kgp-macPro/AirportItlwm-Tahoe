# Private P8 / AX ZC29 corrective candidate

This is private development, not a new public release. Hardware testing is
pending. Installation, EFI/root patches, reboot and traffic remain KGP's work.

## Baseline and identity

Base: public Alpha source `028b4626d113efa283ed44f9c4feb92ba6735aab`, whose
1,489 frozen production/firmware/SDK input hashes were verified against tested
P7 `e1c50b1bbc2c80f78f4fd93d48e548c0eb58a338` before any edits. This supplies
portable offline tests without changing the starting driver. Final commit,
UUID, executable SHA256 and build settings are in `BUILD_IDENTITY.txt`.

The **same binary** identifies BE200 as `BE200-GL92-PRIVATE-P8` and other admitted
IWX devices as `AirportItlwm-Tahoe-B1-ZC29-PERF`. Neither identifies frozen P7 or
ZC28. No PCI/DMA admission is expanded. A retail label does not select a DMA
route. IWM/IWN are not changed.

## Why these changes

The first P7/r8 original has 42 complete tests. Whole-test receiver means are
TCP TX P1/P4 28.57/28.46 and RX P1/P4 195.37/213.10 Mbit/s. All six TCP TX tests
report zero retransmissions. RX P1 can be slow without retransmissions. RX P4
round 2 has a whole interval with three zero-byte streams and 11,927 sender
retransmissions. UDP TX is sender-limited at every requested rate. RX250 round
1 and RX500 round 3 lose datagrams throughout all five intervals, whereas
adjacent higher-rate tests can perform better. WAN upload is 36.54 Mbit/s;
there is no established absolute 28-Mbit/s transmit ceiling.

Original ZIP SHA256:
`05eb55be559b93238f249b40a411382e80f0f5051eec6f7f40a5c1d3d0445684`.
Local CRC, all 292 manifest entries, 42 JSONs and the exact r8 copy/hash passed.
No new physical test was executed by Codex. The separate analysis ZIP named
by KGP was not found and was not represented as read.

The matching-lifetime `KGP_RX_ZC4_PERIODIC` counters show zero starvation,
pressure episodes and recoveries across the saved boundaries. This is
counterevidence to a full packet pool; it does not exclude device-credit or
service delays. BZ's inherited batch-size fields were not maintained in P7 and
must not be interpreted as measured zero RX batch sizes. Epochs remain 1/1,
recorded faults remain zero. Sparse records do not exclude timing stalls.

### 1. Firmware BA ownership versus host encapsulation

`ieee80211_encap` used QoS headers only after host `BA_AGREED`. BZ
`iwx_ampdu_tx_start` returns `ENOSPC`, because GL92/TLC owns TX BA negotiation.
The host therefore kept generating non-QoS data. `iwx_tx` could not select its
allocated per-TID data queues from these headers and used the management queue.
The original records show all eight queues allocated but zero processed BA
TFD updates. This is a source-supported integration defect, not proof that it
alone explains every measured limitation.

A new, distinct `IEEE80211_C_TX_BA_OFFLOAD` capability enables QoS encapsulation
after authorization, RUN, negotiated QoS and the existing protected-HT policy.
Only validated BZ enables it. It does not fabricate host BA state or schedule
host ADDBA retries. EAPOL remains non-QoS/on the management queue. Existing
AX host-BA behavior remains unchanged. Its bit avoids the reference fork's HE
capability bit. Unsupported outgoing TIDs cannot index the eight-entry array.

Primary contract: Linux v6.9 `a38297e3fb012ddfa7ce0321a7e5a8daeb1872b6`,
`mvm/mac80211.c:393` (TLC -> `TX_AMPDU_SETUP_IN_HW`), `mvm/tx.c:564–645`
(new TX API, QoS/header/offload handling). Pinned AirportBZ
`0ccab1627764406861fb9eb0a06ca36e31b4440f` provides the MLD per-TID queue design,
but its native-WCL frontend must not be assumed equivalent to this legacy
encapsulation path. P8 uses original glue corrections; no wholesale allocator
or native frontend import.

### 2. Aggregate completion validation

The now-reachable BA path requires the AP station ID, an owned non-command
queue, its exact TID mapping (or the explicit management TID/queue), and
bounded positive queue progress. A wake-triggered generation/reset/shutdown
change prevents retirement of another queue from the old notification.
Ambiguous ownership continues to require positive stop/reset; no timeout frees
backing. The firmware selector additionally checks advertised TLC capability43
and BA notification v5. Linux's v4/v5 BA fixed fields and the two C flexible-tail
spellings are checked with actual compiler sizes and offsets.

TX response/BA counters count processed notification/TFD updates, **not** a
one-for-one packet retirement total. Differences from packet-publication counts
cannot be called outstanding queue occupancy or packet loss.

### 3. RX credit ordering and lifetime

Both legacy conventional IWX and mapper-backed AX paths wrote RX credits after
`if_input`. A callback can invalidate the lifetime or advance a newer RX batch,
after which the outer old write must not occur. P8 publishes valid replacement
credits before that callback with explicit ordering and current-lifetime checks.
Normal AX credit values, alignment and ring geometry are retained. AX keeps its
original full-batch service: it lacks BZ's service cookie, so introducing
mid-batch callbacks there without a new reentry design would be unsafe.

BZ additionally hands off at most 32 **completion entries** per chunk, returning
synchronized, prepared replacement credits first. Its existing cookie prevents
nested processing of the old CQ snapshot. After each handoff, generation,
reset, fault and cookie are checked before another CQ read. Redundant ordinary unchanged-credit
doorbells are skipped; RFH-ready ALIVE explicitly forces its required rewrite. Reorder release can deliver multiple mbufs per entry;
this is not a claim of 32 packets or a measured wall-clock latency bound.

Host-final-reference rules, preparation/mapping identity, required full fences,
STOP_MASTER/quarantine, epochs, command safety and P6/P7 link/recovery fixes are
retained. No SFENCE substitution, live cache-policy change or remapping occurs.

This is a corrective scheduling/lifetime hypothesis for RX, not a measured
throughput fix. The old post-handoff order also exists in ZC8 and external53;
its presence alone cannot explain their version-dependent contrast.

### 4. RX parsing and bounded observations

BZ checks the actual completed notification payload before reading its RX
descriptor/MPDU and before padding movement. Spare DMA-buffer capacity is not
a valid message-length envelope. Invalid packets are dropped without treating
partial notifications as complete. Existing fragmented-message fail-closed
policy remains unchanged.

Previously reserved transport counter slots20–31 are defined only for P8:
QoS/non-QoS data, management/TID data publication, hardware-decrypt drop,
duplicate drop, reorder consumed (buffered **or** dropped), direct dispatch
before stack acceptance, reorder drop, reorder buffered, CRC/overrun drop and
envelope drop. They are gate-serialized additions, with no printing, allocation
or IORegistry access per packet. Existing watchdog capture and off-gate export
are retained. Old candidate records remain reserved/unknown, not successful zero
measurements. These observations do not constitute end-to-end delivery proof.

## Active AX201/AX210/AX211 investigation

Fulvio's two complete original campaigns confirm TCP RX P4 means 586.71418
Mbit/s (ZC28, 23 October) versus 826.12482 (external53, 24 October): difference
239.41064. Both bind PCI8086:7a70/subsystem8086:0074 at CNVW, and use the same
recorded wired IP192.168.1.16. ZC28 records actual Mode2; external53's numeric
DMA mode is unavailable, its mapped configuration declared. “AX201” is the
reported label, not independent RF-module identification.

The originals establish different frontends/frameworks: ZC28's
`AirportItlwmInterface` / `IO80211FamilyLegacy 1200.12.2b1` versus external53's
`IOPCIEDeviceWrapper` / `AirportItlwmSkywalkInterface` /
`AirportItlwmEthernetInterface` / `IO80211Family 1200.13.1`. This is a concrete
RX handoff comparison lead. It does not establish causation or complete patch
state. AMFIPass is present in both loaded lists; swapping the driver is not
evidence that old root patches were undone.

The Ookla screenshots confirm510.91/815.33 Mbps download, but endpoints differ:
WARIAN/Bari versus TIMSpA/Potenza. All original UDP rates/intervals are retained.
The peer often sends only~200–370 Mbps when400–600 is requested, in both
campaigns, with high reported remote CPU. Separate real losses include
external RX400R1~28%, RX250R2~24%, RX600R2~18%, and ZC28 RX500R2~3%.
Unequal sender/receiver duration and inconsistent JSON loss denominators are
not replaced with invented packet-loss calculations. There is no universal
external-winner inference from the TCP results alone.

The external source contains optional HE support gated by `AIRPORT_WCL` and
`itlwm_he=1`; this does not prove the measured run negotiated HE. VHT setup
is source-equivalent in the reviewed functions. Missing captured PHY/AP/server
hardware details remain unknown. Different Linux PHY modes may matter but
cannot dismiss the existing same-system/version RX contrast.

AX210/ASIX ZC8→ZC13, ZC22's partial recovery and later negative experiments,
historical AX201/AX211 results, the severe distinct ZC26 conventional incident
and all adverse observations remain evidence. A shared RX initiating cause
across cards is still a hypothesis. No new AX admission or physical qualification
is claimed by this candidate.

## Validation and remaining limits

The actual production functions are compiled into sanitizer harnesses, including
P7/current encapsulation and actual queue selection, BA completion ownership,
mapped/conventional AX RX versus frozen ZC28, BZ replenishment/reentry, firmware
TLVs/API/wire layouts and the declared SDK interrupt slot. Existing BE200 boot,
PNVM, link, recovery and ZC28 regressions remain required. Full results and
all initial fixture/build failures are recorded in the evidence directory.

Mocks do not prove cache attributes, interrupt timing, firmware aggregation,
over-air delivery, performance, power or long-term stability. Hardware must
establish the improvement. No requirement to repeat unchanged P7 campaigns:
the saved complete r8 baseline is the reference for the next candidate test.

## Additional active function: Find My

KGP reports our AX210/BE200 MacPro shown only locally, whereas laobamac makes
the same MacPro findable from other Apple devices. Exact card, driver version,
boot and patch/framework state of that successful Find My control still need
evidence; Fulvio's separate native-control performance archive is not a substitute.

Registration, fresh Wi-Fi-based local positioning, iCloud position reporting
and the Bluetooth-based offline Find My network are distinct stages. The
legacy frontend's getSCAN_RESULT definitely advertised rates after copying
none (zero count after bzero), and could advertise more inline IE bytes than
copied. P8 fixes bounded truthful rate/SSID/IE output, preserving complete IE
boundaries, addresses, channel, age and iterator behavior. Actual getter/target
wire structures are sanitizer-tested. No AWDL/Bluetooth/cloud capability is
invented, and no account/privacy setting is changed. This correctness repair
does NOT establish Find My working; the native converter has similar old code,
so this defect alone is not a unique explanation of the functional contrast.

Primary Apple references: [Location Services](https://support.apple.com/en-euro/guide/mac-help/mh35873/mac),
[Find My setup](https://support.apple.com/en-ie/102648). Primary search descriptions
were available on30October2026; direct guide downloads failed/HTTP429. No full
guide download is claimed. Focused passive locationd/findmy/airportd evidence
and a user-controlled functional comparison remain separate from throughput.
