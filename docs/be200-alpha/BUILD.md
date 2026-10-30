# Building AirportItlwm-Tahoe 1.2.0 BE200 Alpha 1

The tested executable was built from `e1c50b1bbc2c80f78f4fd93d48e548c0eb58a338`, after driver correction `80ad358b00f9473c33cf6ee08ffb8e94034c725d`. This public branch adds documentation, pinned offline test references, portable test-tool discovery and Alpha-specific CI. It does not alter the tested production source. The exact tested executable is supplied in the release; rebuilding creates a new, untested binary.

Use the **be200-alpha** branch or **v1.2.0-be200-alpha.1** tag. The main branch retains the frozen 1.0.0 driver source and cannot produce this Alpha.

## Pins and ABI

- Tested build: Xcode 26.5 / 17F42, Apple clang 21, MacOSX26.5 SDK, x86_64; target AirportItlwm-Tahoe / Release / restored Ventura legacy `__MAC_13_0` frontend. Test runtime is separately macOS Tahoe 26.7.2 (25G332), with AppleVTD active.
- Complete standalone MacKernelSDK is included, not an automatically updated submodule: `05094e5e88cec7caedbfb35e8449ed0db94bf95b`, plus the explicit configureInterrupts declaration at reserved virtual slot 3. See SDK_CONFIGURE_INTERRUPTS.diff; patched IOPCIDevice.h SHA256 `9255ed3c77cf30f153ef8a74fce5f9a981544bd066b09b10f946fd78e783704b`. Do not replace it with an arbitrary newer SDK. The compiler-slot test checks the actual declared ABI against the baseline reserved slot.
- Apple IOPCIFamily reference `4822b27a36e2de70e231ecf2bf3021384fab6ec2`.
- Selective HuajiFruit/AirportBZ source `0ccab1627764406861fb9eb0a06ca36e31b4440f`; Linux/API and notices in ../BE200_PROVENANCE.md and the offline reference provenance.
- Firmware GL-C0/FM-C0 API 92: SHA256 `c7092f1f78676f9e9c2c4d12716401db2116b009c5d7c464e8387c5b1c645d6f`.
- PNVM SHA256 `f481e3b0403ca43a0a444b01c04fa4d1582868afcb026b1e697905f0039ccfa3`. Intel license accompanies the unchanged payloads.

## Validate and build

From this branch, select an installed full Xcode for this shell (no global xcode-select change):

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
python3 tools/verify_be200_release_source.py
python3 -B tools/validate_be200.py build/be200-validation
build_root="$PWD/build/tahoe"
xcodebuild -project itlwm.xcodeproj -target AirportItlwm-Tahoe \
  -configuration Release ARCHS=x86_64 \
  SYMROOT="$build_root/products" OBJROOT="$build_root/intermediates" \
  SHARED_PRECOMPS_DIR="$build_root/precompiled" \
  CLANG_MODULE_CACHE_PATH="$build_root/module-cache" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
python3 tools/verify_be200_release_source.py
```

Output: `build/tahoe/products/Release/Tahoe/AirportItlwm.kext`. The upstream bundle/identifier/internal version remain AirportItlwm / com.zxystd.AirportItlwm / 1.0.0; the public Alpha release identity does not rewrite the tested binary. Identify it by marker, UUID and SHA256, not the internal version. The frozen marker is `AirportItlwm-Tahoe-BE200-GL92-PRIVATE-P7`.

The generic inherited 1.0.0 `verify_source.py`/`prepare_sdk.py` checks are not this branch's Alpha entry points: their historical frozen manifests describe 1.0.0, not BZ. The dedicated verifier checks every retained P7 production input and the offline reference hashes; CI additionally executes all 29 BE200/existing-device regression groups and builds the legacy target. Hosted CI uses Xcode 26.3 on macos-15-intel; it is a compatibility build and never replaces the physically tested release bytes. No release/upload permissions are granted to this workflow.

Models and ABI/build checks do not independently establish hardware DMA finality, repeated recovery, performance or other-device qualification. See the release for the exact user-observed hardware scope.
