# Private GL/BE200 source port — experimental P1

Baseline: exact frozen ZC28 `483940988c502379fb6d3dcde885dca83f00b7aa`.
The published root source, releases, beta archives and r7 runner are unchanged.
This is a private single-link infrastructure experiment, not qualified Wi-Fi 7.

## Source and firmware pins

Selective HAL reference: [HuajiFruit/AirportBZ](https://github.com/HuajiFruit/AirportBZ/tree/0ccab1627764406861fb9eb0a06ca36e31b4440f),
commit `0ccab1627764406861fb9eb0a06ca36e31b4440f`, API92/native WCL reference.
This is not established as the source of zxystd's 2024 experimental binary.
The reference README's runtime BE201 / compile-only BE200 claims are third-party
claims. This work makes no corresponding hardware claim.

- `ItlIwxBz.inc`: selectively adapted BZ ROM/context/firmware/PNVM, MSI-X cause
  setup, PHY/RLC/NVM, scan17, MAC/LINK/STA/SEC_KEY single-link implementations.
  Original source notices are retained. The native frontend and DMA arena are
  excluded. Dead legacy fallbacks in reference bodies are not hardware support.
- `if_iwxreg.h`, `if_iwxvar.h`: three-way definitions against the common upstream
  source; existing ZC28 ownership fields and fixed ring arrays retained.
- `include/HAL/ItlBzTxReference.hpp`: actual AirportBZ helper by laobamac,
  original notice retained. Used for header-offload encoding and validated
  completion advance; the reference queue-mask template is not used here.
- `iwx_boot_checks.h`: reference boot error-table address validator, notice retained.
- `ItlIwxBzContracts.hpp`, `ItlIwxBzTransport.inc`: KGP ownership integration,
  not recovered binary source. Helpers are compiled verbatim by host tests.
- Linux hardware/API cross-reference: v6.9
  `a38297e3fb012ddfa7ce0321a7e5a8daeb1872b6` (pcie/rx.c and trans.c);
  v6.10 `0c3836482481200ead7b416ca80c68a29cfdaabd` (wire ABI fixtures).
  File/line/byte references and hashes remain in the immutable completed audit.
- Apple IOPCIFamily `4822b27a36e2de70e231ecf2bf3021384fab6ec2`:
  configureInterrupts, reserved slot3. Vendored SDK base
  `05094e5e88cec7caedbfb35e8449ed0db94bf95b`, with ONE declared reserved-slot
  header patch. Exact header SHA256 is recorded by the build/ABI test. The compiler
  derives the same member-pointer slot as the baseline reserved3; no literal
  binary vtable displacement is used in production.

GL92 firmware `iwlwifi-gl-c0-fm-c0-92.ucode`, 1913128 bytes:
`c7092f1f78676f9e9c2c4d12716401db2116b009c5d7c464e8387c5b1c645d6f`.
PNVM `iwlwifi-gl-c0-fm-c0.pnvm`, 295036 bytes:
`f481e3b0403ca43a0a444b01c04fa4d1582868afcb026b1e697905f0039ccfa3`.
Both are unchanged reference payloads; Intel license is adjacent. Existing 35
compressed firmware arrays remain byte-identical; these two are appended.
API serializers/decoders are pinned together. Explicit advertised versions are
checked; documented implicit STA_CONFIG/STA_REMOVE v1 defaults are allowed.
The malformed-TLV envelope is rejected before the full parser mutates state.

## Effective implementation and contracts

Only PCI8086:272B discrete GL-C0/FM-C0 is admitted, with matching MAC/RF/steps and
non-integrated configuration. BE201 and family-number-based automatic admission
are excluded. Existing-device routing/personalities and firmware stay intact.
The Tahoe target retains `__IO80211_TARGET=__MAC_13_0` legacy Apple frontend.

Genuine provider MSI-X configuration precedes vector resolution. Setup, discovery
and enabled PCI MSI-X capability are checked. IOKit owns vector allocation;
the driver removes/disables its filtered source on partial failure/detach and
never manually frees OS-owned table storage. Cause banks/masks/IVAR and rearm
are used, including reset/rfkill restoration; no legacy ICT on BZ. A BZ-only service method handles fatal causes first and
publishes RFH-ready ALIVE credits before coalesced RX; the legacy MSI-X body
is restored verbatim behind its BZ dispatcher.

BZ GP_CNTRL master-disable request/status and SW_RESET replace the old-family
register sequence. Invalid MMIO, delay or timeout is not a DMA terminal. Failure
propagates into the existing PC1/quarantine closure, retaining backing. Boot
images/paging/IML/PNVM descriptors stay prepared until positive stop. Local
allocation failures may conservatively retain unpublished BZ storage until stop;
this can cost memory but cannot justify unsafe reuse.

512 free descriptors (16 bytes), 512 completion entries (4 bytes), 4KiB buffers.
511 initial tags with eight-credit rounding; the initial published producer is
504. Completion position, returned 1-based buffer identity, pool lease, refill
producer and credits are distinct. Validate/synchronize the returned entry before
reading backing. CPU service cookies protect reentry/reset. Invalid, duplicate,
unissued, stale-software-lifetime tags and fragmentation close admission.
Fragmented messages deliberately fail the generation; no partial notification is
delivered and essential boot responses are not silently discarded. Fragment
assembly is not implemented. The hardware tag contains no software generation;
a malicious delayed duplicate after the same tag has legitimately been reissued
is indistinguishable on that wire. Cross-lifetime safety relies on positive
hardware stop, ring clearing and gated callback exclusion, not magical ID epochs.

Both resolved BZ DMA modes use ZC28 prepared RX leases and TX/command backing;
actual mapper route is reported separately. Qualified allocation checks enforce
single full-length segment, backing identity, alignment, overflow and same4GiB
extents where published. Host references retain old RX backing after replacement.
No reference arena was transplanted. Required full fences remain.

MLD MAC/LINK/STA, PHY5/RLC2, scan17, NVM4, queue3, key1 and rate configuration
are connected to legacy net80211. Per-TID data queues are selected only after
port authorization; preauthorization/EAPOL uses the management queue. Queue IDs
outside the existing 31-entry driver domain are rejected, not aliased. A created
queue's backing is conservatively retained until positive device stop, including
a queue removed in firmware. Reassociation/reuse may require the normal full
reset rather than unsafe live backing replacement; this remains a hardware gate.

Command slots additionally require both firmware completion and caller release.
Code, generation, reset epoch and software serial are checked; uncertain outcomes
retain ownership. BZ waits use commandGate sleep/wakeup, not msleep while holding
that gate. Existing ZC21 negative error/TVQM/BA and ZC16/PC1 guarantees remain.
The reference activate() shortcut was excluded: BZ uses the ZC28 lifecycle body,
including quiesce-before-RUNNING and propagation of resume/readiness failure.
MLD data-queue creation failures abort association rather than reporting RUN
with KGP's retained/blocked uncertain queue. New generation wins over late queue
creation; a late caller cannot unlock a new generation's NIC lock.
Late return into a new lifetime cannot publish old RX credits or release its
service cookie. The RX parser's normal notification/data paths do not perform a
synchronous BZ command wait: key callbacks enqueue async commands, state changes
queue tasks, and upper-stack delivery occurs after packet parsing. Final host
references remain independent of the DMA-service scope.

## Observability, validation and remaining limits

No per-packet printing, registry allocation/polling or unsafe DMA inspection was
added. Fixed progress counters are captured by the existing watchdog under gate
and published off gate as `KGP_BE200_TRANSPORT` schema200/1 (48 uint64 values).
Boot/MLD stage properties are control-path diagnostics. Async key counters mean
submission, not acceptance. ALIVE polling hits are separately retained; polling
must not be mistaken for functional interrupt delivery.

Host tests execute actual helper/transport/cleanup/wait bodies under mocked KPI,
MMIO and DMA effects. They establish software boundaries, not hardware finality,
actual PAT attributes, IRQ operation, firmware acceptance, RF, association, IP,
throughput or sleep/wake. Existing regression assertions remain; mock-only API
surface additions keep BZ false for legacy tests. Earlier failed attempts and
corrected fixture/reporting errors remain in new evidence. Original audit stays
immutable; its two mixed CSV descriptions are corrected only in derived notes.

The first physical goal is boot/scan, one WPA2/CCMP join, IPv4/gateway/ARP and
short source-bound TCP transfers both ways. Full Wi-Fi7/EHT/HE, MLO,6GHz, roaming,
monitor mode, AWDL and power-cycle reliability are not qualified. Use the supplied
manual installation/rollback guide and bounded stage collector. KGP alone tests.
