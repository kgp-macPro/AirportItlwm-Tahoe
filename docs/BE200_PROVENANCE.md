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

Genuine provider MSI-X configuration precedes vector resolution.
Causes are masked/cleared before enabling the provider source. The filter selects
BZ before lease admission, and pre-admission actions do not touch uninitialized
rings/MMIO; IVAR configuration clears any resulting single-vector auto-mask. Setup, discovery
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


## P2 boot failure correction (29 October 2026)

P1 commit 070d4b641dfd967d366af35bfcc953a0c759b931 was physically loaded
(UUID 26148E5B-9D27-3DE2-BF02-3F9E2609BE22) but failed attach before a usable
Wi-Fi interface. Original IORegistry enumerated 8086:272b, subsystem 8086:00f4,
ARPT at PCI 163:0:0. The injected AX210 model string is not hardware identity.
Legacy IO80211/Skywalk/AMFIPass were loaded. Provider retained firmware-context
error EIO, pre-ROM IRQ capability snapshot and all eight boot-DMA allocations;
no wait-ALIVE/parsed-ALIVE properties or interface child remained.

The actual P1 pre-ROM loop passes kIODirectionInOut to synchronize(). Apple XNU
f6217f891ac0bb64f3d375211650a4c1ff8ca1ea iokit/Kernel/IODMACommand.cpp rejects
Out|In with kIOReturnBadArgument before checking preparation. P1 reduces this
to EIO. This exact P1 context+Apple method failure is reproduced by the new host
test. The public source is a pinned reference, not claimed exact source of the
running xnu-12377.161.15.701.48 kernel. P1 did not retain the raw IOReturn; the
runtime error plus source and reproduction locate the unsupported call, without
claiming hardware-independent success of later stages.

P2 publishes CPU-initialized boot bytes with synchronize(kIODirectionOut) for
context, scratch, peripheral-info, IML, RX free/completion/status and command
ring. These are bidirectional ALLOCATIONS; before the ROM doorbell the CPU is
producer, so Out is the required copy. Completion-side In and all mapping,
backing, preparation, final-reference/stop proofs and the full fence remain.
The checked helper refuses missing/mismatched backing or failed sync; it records
the exact boot region/index/reason and raw IOReturn. No address/ROM publication
on failure. Diagnostics are boot-only, not per-packet registry work. Actual PCI
configuration/MMIO identity is recorded separately from injected properties.
The stale reference MLD-source-implemented=false metadata is corrected, alongside
an explicit false hardware-qualified property; no runtime qualification implied.

New host test executes entire P1 and P2 context startup bodies and the pinned
Apple synchronize method with mocked allocation/walkAll/MMIO/provider. It checks
initialization copy direction, fence/ownership-before-doorbell, all eight regions'
missing/mismatched/unprepared/sync failures, partial allocation and arm/admission
failures. The full original Apple source and APSL notice are kept as a test-only
reference (tools/host_tests/reference/Apple-IODMACommand-f6217f891ac0.cpp), not linked
into the Kext. The previous P1 suite did not execute this pre-ROM loop against
the Apple option contract; its passing models did not establish this API usage.

Physical P2 boot/ALIVE/IRQ/PNVM/NVM/scan/association/data and both DMA routes remain
unverified. No firmware/API, allocator, ring geometry, packet path or admission
policy is changed for this correction. All previously documented limits apply.


## P3 PNVM resource correction (29 October 2026)

P2 was physically loaded with UUID69BF7B25-76D5-33B7-B072-F539583CAD8C.
Its boot synchronized all8 regions (complete/rawIOReturn0),started ROM and parsed
ALIVE6 with valid status. The P1 API correction therefore passed on this hardware.
It then failed at PNVM resource lookup:ItlwmPnvmName=iwlwifi-gl-c0-fm-c0-.pnvm,
DescFound0,FailStep2,ReadError22,stagehal:pnvm-load. PNVM doorbell/completion,NVM
and a usable network interface were not reached. Original boot data preserved.

The cause is a concrete compatibility interaction:itl80211/linux/kernel.h maps
strrchr to reverse_strchr,which returns AFTER the delimiter,unlike standard C.
The imported PNVM loader assumed standard semantics and retained one extra '-'.
The existing AX210 PNVM function compensates with -1; it is unchanged. P3 uses
an explicit FirmwareName/PnvmName pair in ItlIwxBzContracts.hpp for our sole
selected source baseline,GL-C0/FM-C0 API92. Unknown names are refused,not guessed.
Firmware/PNVM bytes,API versions,allocation/mapping,lifetime/fences,IRQ policy,
packet and frontend paths are unchanged. The PNVM envelope/SKU bounds are checked
before typed access; missing SKU returns an error,not apparent parse success.
All errors normalize to positive driver errno while DMA retention stays intact.

The added test runs the actual P2/P3 resource loader,actual compatibility helper,
actual getFWDescByName and zlib routine,actual section parser and PNVM publication
routine against the complete embedded payload and all16 source SKUs. DMA,wait,
MMIO and OSData KPIs are mocks:descriptor/chunk bytes and failure closure can be
checked offline,but firmware PNVM acceptance cannot. Original notices remain.

P2's legacy ItlwmBootIrq* zero counters do not count our private bzInterrupt/
bzNotif path and do not prove no interrupts. P3 exports actual lifetime filter/
action/RX/ALIVE counters and ALIVE SKU on the boot thread after the wait. No hot-
path allocation/print/registry export is added. Decoder includes PNVM and firmware
properties even if attach fails before watchdog/interface publication. Source
implementation metadata is kept distinct from hardware qualification.

P3 hardware PNVM acceptance,NVM,scan,association,keys,DHCP/data,both DMA routes and
lifecycle remain untested. P2's partial boot success is not inherited runtime
qualification for P3 or evidence of functional BE200 infrastructure Wi-Fi.
