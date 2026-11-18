# BlueToolFixup 2.7.6 — Tahoe discovery compatibility

## Finding and correction

The running WLAN R49, BlueToolFixup 2.7.5 and stock IntelBluetoothFirmware /
IntelBTPatcher 2.5.3 match the published binary UUIDs. The boot arguments include
`-kgpble`, without `-btlfxoff`. All eight additional patch sites remain ORIGINAL
in the current 26.7.3 readable bluetoothd. The unchanged 2.7.5 production
classifier rejects CoreInit. The original boot diagnostic itself was not
retained in the available unified logs. We therefore establish a reproducible
compatibility rejection, not a captured boot-time rejection message.

The former statement “no build allowlist” was literally correct but incomplete:
hashing every byte of fourteen whole functions made relative addresses an
effective build dependency. A synthetic UUID change did not test that problem.
This was our selector defect, not evidence of an R49 WLAN or HCI firmware defect.
Outgoing successful AirDrop transfers do not by themselves qualify incoming
discovery or all of Bluetooth.

Independent LLVM instruction disassembly identifies PC-relative operands in the
seven functions required for the edits and their call/global graph. Their full
instruction layouts, after excluding only these address operands, are identical
in all three available native images:

| Source volume | Product/build | Old selector | New selector |
| --- | --- | --- | --- |
| `/Volumes/Sequoia-M2 1` | 26.5 / 25F71 | rejects CoreInit | matches layout |
| `/` | 26.7.3 / 25G342 | rejects CoreInit | matches layout |
| `/Volumes/Tahoe-beta` | 26.7.3 / 25G416, per its plist | matches | matches layout |

The mounted beta is **not** claimed to be 26.7.4 RC. No installation of another
OS is necessary to demonstrate or correct this defect. The seven raw 25G416
functions also match the old 25G332 function contracts exactly.

2.7.6 hashes the complete normalized instruction layout. It keeps opcodes,
vtable slots, ordinary immediates, field offsets and internal control flow.
Annotated relative addresses may change. Seven unrelated unedited HCI/Ready /
advertisement callback hashes are no longer an eligibility gate. These callbacks
are still neither modified nor replaced. The two same-layout bool setters are
resolved through the actual CoreInit calls and checked against the shared
address flag, rather than chosen as a first global signature match. The
event-mask consumer must still reference the actual unique mask constant.

Each plan is rebuilt using that native image's ORIGINAL instruction bytes. The
patch spans have been narrowed to the actual replaced instructions so no
reference build's call/RIP displacement is copied. Signed page hashes, CDHash,
slice, vnode, original native validation and per-page atomicity are retained.
A bounded temporary 8-KiB workloop buffer avoids a large kernel stack frame.
The page hook does no extra allocation, decoding or file I/O. Unknown layouts
still disable the optional extra component safely; no build-number list was
added. This is layout compatibility, not a proof of every function's behavior
or all future Apple implementations.

## Scope and cards

Only BlueToolFixup changes. R49 WLAN UUID/hash, its radio architecture, stock
IntelBluetoothFirmware 2.5.3 and IntelBTPatcher 2.5.3 remain unchanged. Existing
USB device/interface matching is unchanged: named Intel products plus actual
Intel Bluetooth-class devices, without a Wi-Fi-model allowlist, FirmwareLoaded
gate or HCI preflight. Multiple physical controllers retain the existing
ambiguity guard. USB identity alone is not proof of controller capabilities.

| Wi-Fi card | Hotfix path | New physical qualification |
| --- | --- | --- |
| BE200 | shared Intel Bluetooth selector and normalized native layout | pending |
| AX210 | same | pending |
| AX211 | same; no family exclusion | not locally available |
| AX201 | same; WLAN CNVW/Mapped-2 path untouched | not locally available |

Existing R49 BE200/AX210 results remain historical evidence on their tested OS
and Bluetooth build. This update does not invent new WLAN backends or extend
firmware support merely by adding PCI/USB IDs.

## Applied validation and its limits

* **REA/disassembly:** identified the relative operands in the actual native
  functions and their call/global relationships. Normalized layouts match the
  three available Tahoe files. The final Kext's selected compiled paths and
  build identity are recorded separately. No full firmware “REA PASS”.
* **Production host tests:** all three real files are processed by the actual
  classifier/page editor under ASan/UBSan. Tests cover all eight edit effects,
  no writes outside them, wrong same-layout setter targets, flag alias/state,
  invalid signatures/pages, malformed input, missing workspace, and page
  atomicity. The old published selector independently produces exactly the
  same CPU-patched image as 2.7.6 for 25G416 (SHA-256
  `83eddd307a496495631ccc59a63964cb08817d8264608f5d9ee655062f8ea79e`).
* **Kernel-service stand-ins:** existing production selector/preparation/page
  control tests run against each real file. They check original USB matching,
  lifecycle and validation guards with mocked USB/VFS/KPI services. They do
  not run the kernel hook or prove notification timing on hardware.
* **Hardware:** no new installation, reboot, HCI transaction or capture was
  started. Native boot-time activation and restored discovery on MBP/iPhone
  require the manual 26.7.3 test. New AX201/AX211 hardware success is not claimed.

The old fourteen-function/hash tests remain historical sources. Their claim of
build independence is superseded by the concrete address-relocation tests and
the restricted claims above. Repeated assertion totals are not independent
firmware evidence. No model was extended to declare AirDrop successful.

## Update caveat

This retains BlueToolFixup's existing shared page-cache patch mechanism. It
does **not** fix the separate incremental-OTA verification problem. Before a
future incremental OTA, boot once with `-btlfxoff` to prevent both base and
additional BlueToolFixup page edits; removing only `-kgpble` is insufficient.
After the update remove that temporary argument and reboot. Installation and
boot-argument changes remain manual. A full installer is an alternative.
The local installer logs and matched CRC establish the previous OTA failure;
the general mechanism is also documented in
[Acidanthera issue 2080](https://github.com/acidanthera/bugtracker/issues/2080).
