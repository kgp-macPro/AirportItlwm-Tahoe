# BlueToolFixup 2.7.6 - Tahoe compatibility hotfix

This project-modified BlueToolFixup corrects the selection of compatible native
Bluetooth functions after some Tahoe updates. It is not an official Acidanthera
2.7.6 release and does not fix incremental OTA verification failures.

## What to replace

This archive supplies **BlueToolFixup.kext only**, plus its checker and records.
Use it with the original AirportItlwm-Tahoe 2.1.0 Beta 1 (R49) base package.

| Component | Action |
|---|---|
| Project-modified BlueToolFixup 2.7.5 | Replace with the included 2.7.6. |
| AirportItlwm R49 | Keep the original binary. |
| IntelBluetoothFirmware 2.5.3 | Keep the original lshbluesky binary. |
| IntelBTPatcher 2.5.3 | Keep the original lshbluesky binary. |
| Lilu | Keep the existing compatible dependency; minimum 1.7.2. |

1. Back up the working EFI and keep an accessible rollback setup.
2. Replace BlueToolFixup.kext in the EFI you actually boot. Retain its enabled
   OpenCore entry; do not load an additional copy or a second version.
3. Keep the documented AirDrop arguments unchanged:

   ```text
   -itlwmhe -itlwm-awdl -itlwmbeawdldpnoinfra -kgpble
   ```

4. Restart manually. A live file replacement does not change the loaded kext.
   For normal AirDrop use, `-btlfxoff` must not be active because it disables
   BlueToolFixup. Do not judge Bluetooth/AirDrop while that plugin is disabled.
5. Optionally run the included read-only checker from the extracted folder:

   ```sh
   sudo python3 CHECK_LOADED.py
   ```

   Expected BlueToolFixup version: **2.7.6**. Expected UUID:
   **BE8B2187-41AA-3EF6-963D-2C4CEBA49982**. A matching loaded UUID alone does
   not prove BLE transmission, remote discovery or file delivery.
6. Check Wi-Fi/Internet, visibility from both peers before offering a file,
   and actual file arrivals in each direction. Ethernet can be disabled
   manually for a WLAN comparison; AirDrop may still choose local Wi-Fi.

Nothing in this package installs automatically, changes permissions or edits
EFI, NVRAM, boot arguments or Apple system files on your behalf.

## Scope and remaining limitations

The eight additional patch effects are unchanged. The selector now tolerates
identified native address changes while retaining instruction/operand and
signed-page checks. Unknown native layouts skip the extra patches. There is
no Tahoe-build allowlist, but arbitrary future macOS code is not guaranteed.

The common Intel Bluetooth selection includes BE200, AX210, AX211 and AX201;
their separate WLAN firmware/transport choices are untouched, including AX201
CNVW/Mapped-2. After updating to both Tahoe 26.7.3 and 26.7.4 RC, successful
bidirectional Hackintosh-iPhone file transfers confirmed basic R49 AirDrop
functionality with this hotfix. This does not constitute physical
qualification of all four cards. Discovery can still be
delayed after a restart, even with all eight patch effects present. Mac-to-Mac
setup delays, slow/failed transfers and the separate unreliable Instant
Personal Hotspot path remain open.

## macOS updates

This hotfix keeps the existing page-cache patch mechanism. Incremental OTA
updates can reject the modified read view of bluetoothd. A later delta
attempt with `-btlfxoff` failed on a different file, System Information.

The approximately 1 GB delta updates to Tahoe 26.7.3 and 26.7.4 RC failed in
Software Update at the preparation/verification stage of installation.
After these failed attempts, Software Update automatically offered the full
approximately 18.3 GB OTA payloads. These completed successfully on **two
separate system disks**,
reaching **Tahoe 26.7.3 and 26.7.4 RC**. Successful full OTA updates were
confirmed **with and without `-btlfxoff`**; this update path does not require
the argument.

An effective `-btlfxoff` disables the complete BlueToolFixup plugin. Removing
only `-kgpble` leaves the base patches active. Neither is a verified complete
incremental-update remedy, and changing arguments needs a restart. For normal
Bluetooth/AirDrop use after an update, remove a temporary `-btlfxoff` and
restart to re-enable the plugin.

## Sources and credits

Use the corresponding `BlueToolFixup-2.7.6-Tahoe-Hotfix-SOURCE.zip` and the source tag `bluetoolfixup-intel-v2.7.6`.
It contains the curated public-source snapshot, Git bundle and build records.
Apple binaries, private captures and private development ancestry are excluded.
Acidanthera/BrcmPatchRAM, OpenIntelWireless, Bananz0/Glen Muthoka and the
existing project authorship/licenses are preserved. See the main README for
the unchanged R49 WLAN credits and full beta setup.
