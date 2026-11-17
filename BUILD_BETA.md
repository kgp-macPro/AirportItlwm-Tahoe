# Build the corresponding 2.1.0 Beta 1 sources

These instructions build separate comparison outputs. The release ships the
four original, frozen binaries, not these new outputs. A successful build does
not qualify firmware acceptance, radio coexistence or file delivery. Different
toolchains/paths can change UUIDs and binary hashes.

## Complete source and dependencies

Use the `v2.1.0-beta.1` tag in kgp-macPro/AirportItlwm-Tahoe, or the supplied
SOURCE.zip. It includes the complete curated source tree, a full Git bundle,
MacKernelSDK, Lilu 1.7.2 sources and SDK, and original lshbluesky 2.5.3 sources
with firmware files. No private R45/R47 checkout or incremental bundle is needed.
Original source commits and file hashes are in SOURCE_PROVENANCE.json and
`tools/release_2_1_0_beta_1_inputs.json`. This public snapshot has its own commit;
it is not presented as the private historical R49 commit.

Full Xcode and Python 3 are required. The original WLAN build used Xcode 26.5
(build 17F42), macOS SDK 26.5. Select your existing Xcode toolchain normally.

From an extracted SOURCE.zip, create a fresh Git checkout from its full bundle:

```sh
git clone AirportItlwm-Tahoe-2.1.0-beta.1.bundle beta-build
cd beta-build
git checkout --detach v2.1.0-beta.1
python3 tools/verify_beta_source.py
```

The BlueToolFixup build intentionally retains its committed-source check.
Build from this Git checkout, not the plain GitHub-generated source ZIP.
The bundled SDK is already present; network acquisition is unnecessary.

## AirportItlwm R49

```sh
R49_BUILD_ROOT="$PWD/build/wlan-check" sh tools/awdl-r49/BUILD_RELEASE.sh
```

Output: `build/wlan-check/products/Release/Tahoe/AirportItlwm.kext`.
The frozen generated FwBinary.cpp is included as an original R49 input. The
fw_gen dependency can regenerate it from the included firmware when absent;
the original helper preserves an existing generated file.
The explicit MODULE_VERSION=2.0.0 matches the frozen binary's internal version.
Do not run the separate stable-2.0.0 source verifier as a beta verifier.

## Modified BlueToolFixup 2.7.5

```sh
python3 tools/bluetooth-intel/BUILD_BLE_PROFILES.py \
  --lilu-sdk "$PWD/third_party/Lilu-1.7.2-SDK/Lilu.kext" \
  --out "$PWD/build/bluetool-check"
```

Use a new output directory: the original build tool deliberately refuses to
overwrite one. It verifies its source against Git HEAD; do not disable this
check. MacKernelSDK and Lilu headers/plugin_start.cpp are supplied. The SDK
Lilu.kext is a build dependency, not an additional release runtime component.

## Original IntelBluetoothFirmware and IntelBTPatcher 2.5.3

The full unmodified original source, firmware, build scripts and license are
under `third_party/IntelBluetoothFirmware-2.5.3`. These are the sources of the
stock binaries; older experimental capability-publisher sources are not used.
To build them locally, provide the included SDKs at the original expected paths:

```sh
cd third_party/IntelBluetoothFirmware-2.5.3
ln -s ../../MacKernelSDK MacKernelSDK
ln -s ../Lilu-1.7.2-SDK/Lilu.kext Lilu.kext
xcodebuild -project IntelBluetoothFirmware.xcodeproj -target IntelBluetoothFirmware \
  -configuration Release ARCHS=x86_64 MODULE_VERSION=2.5.3 \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
xcodebuild -project IntelBluetoothFirmware.xcodeproj -target IntelBTPatcher \
  -configuration Release ARCHS=x86_64 MODULE_VERSION=2.5.3 \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Create those two links only if they do not already exist. Original upstream
notices and build logic remain unchanged. No local build output replaces the
stock release binaries. Nothing here loads or installs a kext.
