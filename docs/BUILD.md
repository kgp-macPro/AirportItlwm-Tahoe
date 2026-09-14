# Build and CI

The repository is already the complete AirportItlwm-Tahoe source. Do not apply internal RC/experimental patches. Only the dedicated Tahoe target produces this product; avoid the upstream “(all)” aggregate for a Tahoe build.

## Toolchain and dependency

The physically qualified release used Xcode **26.5 / 17F42**, Apple clang 21.0.0 and MacOSX26.5 SDK on an x86_64 Tahoe host. Install/select a suitable full Xcode for source builds; this document does not change your global Xcode selection.

MacKernelSDK is an ignored standalone dependency, as in the pinned upstream baseline, not a registered submodule. Its exact commit/tree and exported file hashes are in `tools/sdk_pin.json` and `tools/sdk_sha256.json`. From the repository root:

```sh
python3 tools/prepare_sdk.py
python3 tools/verify_source.py --sdk
python3 tools/run_host_tests.py
```

The setup helper fetches only the declared MacKernelSDK commit if the directory is absent, verifies the pin/content and refuses to overwrite existing unexpected content. An exact exported SDK directory is also accepted. No latest-branch fallback is used.

## Build directly

From the repository root, with the selected Xcode available through `xcrun`/`xcodebuild`:

```sh
build_root="$PWD/build/tahoe"
xcodebuild -project itlwm.xcodeproj \
  -target AirportItlwm-Tahoe -configuration Release ARCHS=x86_64 \
  SYMROOT="$build_root/products" OBJROOT="$build_root/intermediates" \
  SHARED_PRECOMPS_DIR="$build_root/precompiled" \
  CLANG_MODULE_CACHE_PATH="$build_root/module-cache" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Output: `build/tahoe/products/Release/Tahoe/AirportItlwm.kext`. The existing fw_gen dependency generates FwBinary.cpp as required; it is ignored by Git. Do not override MODULE_VERSION: the target defines 1.0.0 locally. The actual bundle and executable retain their upstream names.

A source rebuild is a new binary. Matching source/version does not guarantee the frozen release UUID/hash or transfer physical qualification to that build.

## CI validation

The workflow uses GitHub’s documented **macos-15-intel (x86_64)** runner and explicitly selected **Xcode 26.3**, which is listed on that image. The release’s Xcode 26.5 is not assumed to be present. If the selected Xcode is absent, CI fails clearly rather than silently using another toolchain. This hosted build is a compatibility check, not exact release-byte reproduction.

CI performs:

1. Checkout with credentials persistence disabled; no release/tag/write permissions.
2. Frozen source hashes, Tahoe/Ventura source-phase equivalence, target-local versions/macros/legacy dependency and upstream-target checks.
3. Exact MacKernelSDK acquisition/verification.
4. Preserved host ownership/reset/stale-completion/drain models under ASan/UBSan and TSan.
5. Explicit Tahoe Release x86_64 build, plus generated artifact bundle/version/architecture checks.
6. A labeled ZIP uploaded as **CI BUILD — NOT PHYSICALLY VALIDATED**, with a short retention period.

The workflow never creates tags or releases. Host tests use modeled IOKit/hardware behavior; they are not kernel, hardware or physical test substitutes. Existing qualification is tied to the [frozen release identity](../README.md#exact-release-identity), not CI outputs. The workflow’s own runtime success must be observed on GitHub; preparing it locally is not a claim that it has run.

Runner references: [available images](https://github.com/actions/runner-images#available-images), [macOS 15 Intel image/toolchains](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-Readme.md).
