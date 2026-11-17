# Intel AX/BE native BLE patches

Explicit opt-in: `-kgpble`. No custom HCI queries, capability-report or
FirmwareLoaded prerequisite. Physical Intel Bluetooth USB selection accepts
named composite devices and other Bluetooth-class Intel USB controllers.
The stock Intel firmware loader/native stack perform normal initialization.

The eight BLE-R25-01 native edits, native code/page matching and normal
completion/error semantics are preserved. Native image preparation runs
independently of USB/FirmwareLoaded publication. No model or macOS build-number
allowlist is added. AX201/AX211 are not locally hardware-qualified.

Build only BlueToolFixup with `tools/bluetooth-intel/BUILD_BLE_PROFILES.py`.
The current delivery pairs it with the unmodified lshbluesky 2.5.3 RELEASE
IntelBluetoothFirmware and IntelBTPatcher, not our older experimental publisher.
No BTLEAdvFixup, codeless Injector or installer is included.

Original patch provenance: Acidanthera/BrcmPatchRAM tag 2.7.2
`aef376bb1e7e27cabc61da7edfb1a88fac7f7acc`; Bananz0
`cef812d600f13c7b64932e831c9857058bf9ae6d`, with the retained three displacement
adaptations for the verified native image. Original authorship/GPLv2 retained.
