# 2.1.0 Beta 1 publication checks

This release repackages four frozen original kext bundles. Their executable
UUIDs, hashes and full file inventory are verified against the preserved R49
and Bluetooth packages. No release binary or Info.plist was rebuilt or edited.
See SOURCE_PROVENANCE.json and the runtime package BUILD_IDENTITY.json.

The curated public source snapshot preserves 1,515 original WLAN, modified
BlueToolFixup, MacKernelSDK, script and firmware inputs. All twelve shared
BlueToolFixup build/source inputs are identical at the original R49 and
BlueToolFixup runtime commits. Original lshbluesky 2.5.3 and Lilu 1.7.2 sources,
firmware and the Lilu SDK are supplied separately, not replaced by historical
experimental loader sources. The public commit has its own identity and
descends from existing public main; private experimental history is excluded.

A fresh Git checkout compiled AirportItlwm, the modified BlueToolFixup,
IntelBluetoothFirmware and IntelBTPatcher successfully using Xcode 26.5
(build 17F42). The original BlueToolFixup committed-source check remained
enabled. These outputs have their own hashes/UUIDs and are not the shipped
binaries. Source input hashes were checked again after compilation.
Existing upstream deployment-target/build warnings do not qualify newer
platforms or change the supplied release originals.

The existing R49 recorder's self-test exercises decoder/PCAP fixtures only.
Package manifests, ZIP integrity, README images/links and complete offline
Git bundle checkout are publication/integrity checks, not firmware tests.
No new firmware-mock success or comprehensive REA seal is claimed. Prior
targeted ABI/compiled-path findings are retained without repeating unchanged
analysis. The frozen R49 code has no new runtime behavior requiring new REA.

Hardware evidence remains scoped to the recorded BE200/AX210 successes and
failures and the final visual BE200 R49 comparison. AX201/AX211 AirDrop,
reliable Mac-to-Mac setup, broader coexistence, Instant Hotspot and Continuity
Camera remain as documented. No new hardware capture or installation is
performed for publication.
