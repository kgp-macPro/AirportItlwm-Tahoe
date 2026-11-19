# BlueToolFixup 2.7.6 - publication status, 19 November 2026

This note supersedes the earlier candidate's pending-hardware and general
OTA-workaround wording. Historical reports remain records of their original
test stage; they are not current installation advice.

## Correction

The previous eligibility hashes covered complete functions including
PC-relative native address operands. Although no build-number allowlist was
present, compatible layouts could still be rejected after an OS update.
The old production classifier reproducibly rejected CoreInit on 25G342,
and all eight extra patch effects were absent in the preserved read view.
The original boot rejection message itself was not retained.

The new selector checks seven complete normalized instruction layouts,
excluding only independently disassembled PC-relative address operands.
Opcodes, ordinary immediates, field/vtable offsets, internal flow and actual
call/flag relationships remain checked. Matching bool setters are resolved
through their real call/field relationships. Each plan uses that native
image's original instructions; old-build call displacements are not copied.
Signed pages, CodeDirectory/CDHash, slice/vnode binding and page atomicity
remain checked. The eight edit effects and native initialization remain the
same. No new HCI preflight, FirmwareLoaded gate, card-model or build list.

## Evidence and limits

Real native files from 26.5/25F71, 26.7.3/25G342, 26.7.3/25G416 and
26.7.4/25G422 have been checked. The old 25G332 reference functions are
covered through matching retained functions. These are file/layout results,
not a physical hardware campaign on each OS/card combination.

Production classifier/page-editor tests and negative cases check the local
selection and write rules; USB/VFS/KPI stand-ins remain mocks. Selected native
and final-binary disassembly checks do not qualify RF, firmware or whole-system
reliability. No broad REA/firmware PASS is implied.

The owner confirms successful bidirectional Hackintosh-iPhone file transfers
after the updates to both Tahoe 26.7.3 and 26.7.4 RC, establishing basic R49
AirDrop functionality with BlueToolFixup 2.7.6 on both releases. Recent
Hack-iPhone attempts were all successful within the reported sample (no
numeric count supplied). In one retained boot, all eight
patch effects were present before delayed visibility recovered without a
restart, mode change or toggle under Contacts Only. This leaves discovery
timing/identity/RF/service lifecycle open. It does not prove a contact-mode
block or exclude a driver contribution. Mac-to-Mac delays/failures and Instant
Personal Hotspot remain separate limitations. No new captures are invented.

## OTA correction

The three original 25G342 delta failures rejected bluetoothd, BOM expected
082cab6f / actual 49584324. The actual value was independently reproduced
from the patched read view. This retained page-cache mechanism is not fixed
by 2.7.6.

The later 19 November 00:35:26 CET delta failure rejected System Information,
BOM expected 9e62f44c / actual df53245e. The owner reports -btlfxoff, but the
effective historical argument/plugin state was not independently retained.
Do not report another captured bluetoothd failure with the plugin disabled.
BlueToolFixup directly targets BlueTool/bluetoothd, not System Information;
the responsible other patcher for that mismatch is unproven.

The full product MSU_UPDATE_25G422_full_26.7.4_minor appears in the installation
journal at 01:22:17 CET and the target volume now reports 26.7.4/25G422.
The owner additionally confirms successful full OTA updates to 26.7.3 and
26.7.4 RC on two separate system disks. The approximately 1 GB delta updates
failed during installation preparation/verification within Software Update;
Software Update then automatically offered the full approximately 18.3 GB
OTA payloads, which completed successfully. Successful full OTA
updates occurred with and without -btlfxoff; the argument is not required
for this update path. These are confirmed full-OTA successes, not merely a
proposed fallback. They are not described as separately launched installer
applications, and no success rate for untested systems is inferred.

Removing -kgpble only disables the optional extras. Effective -btlfxoff
disables the complete plugin, but is not a proven overall delta-OTA remedy.
Do not request another installation solely to reproduce the already preserved
failures, or automatically edit EFI/NVRAM. Preserve logs and distinguish the
actual rejected file. Mechanism reference:
https://github.com/acidanthera/bugtracker/issues/2080

## Frozen identity and source

- BlueToolFixup 2.7.6 runtime source: 78733f46d8fceb04e032b7dc58ec059a8873e0a5.
- UUID: BE8B2187-41AA-3EF6-963D-2C4CEBA49982.
- Executable SHA-256: 43686e31d54aa840110aff37c37c85179eae1a001b0f0422b38bd48985f51199.
- Curated unpublished source: 412c4ae7355d3251211f7c73e3066385593ec4c1.
- Its public base: 4f9bcf863fee87c34de1c4c0f00cbde317e9cbdc.
- R49 source: c9a72c638e72945e8dd6b34728c35b6fca948a44.
- R49 UUID: 557EF8F5-45E3-3B78-B2AE-D1B3B0C4EED0.
- R49 executable SHA-256: 6359207e57bba95178712e30ef7c572156baf07131e9139d07afc2aa180af925.

The exact published hotfix tag must point to the documentation-only successor
of that curated public snapshot, not to private runtime history. Keep both
stock lshbluesky Intel components at 2.5.3. Existing credits/licenses remain.
