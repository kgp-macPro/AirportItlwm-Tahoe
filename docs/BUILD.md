# Building AirportItlwm-Tahoe 2.0.0

The repository contains the complete Tahoe target and pinned, vendored MacKernelSDK. No experimental patch application, submodule checkout or latest-SDK substitution is needed. Use the dedicated `AirportItlwm-Tahoe` target, not the aggregate for other macOS versions.

## Release source and binary

The release build was produced from **`4b948ae16a7cbe82699dc7078cf4c917456eff89`** with Xcode **26.5 / 17F42**, Apple clang 21 and MacOSX26.5 SDK, x86_64, unsigned. The final tag also includes public documentation, regression fixtures and benchmark tooling; the protected production-input manifest confirms that those later commits do not alter the build inputs.

| Item | 2.0.0 release |
|---|---|
| Bundle | `AirportItlwm.kext` / `com.zxystd.AirportItlwm` |
| Bundle versions | `2.0.0` |
| UUID | `E01CC22A-6A2D-3E0D-A8FD-229ED62C1751` |
| Executable SHA-256 | `ab8688febf2ccc78984ea6b2b9be933cb14317bc7951983330ffadab32d3a2f8` |
| Info.plist SHA-256 | `ba5bc5c337f639b82e04f22ac29e6bb338f26e046d46d79a26a38ecdd5dfd0ef` |

This is a **new versioned rebuild**, not a byte-identical copy of hardware-tested P10 or R9. All protected functional sources, firmware, headers, SDK and build scripts match the final R9 build input; the two Tahoe target-local MODULE_VERSION values alone change from 1.0.0 to 2.0.0. Campaigns continue to identify their original binary. See [lineage and scope](TECHNICAL_LINEAGE.md), [hardware qualification](QUALIFICATION.md) and [results](results/2.0.0.md).

## Verify and build

From a full checkout, with a suitable full Xcode selected:

```sh
python3 -B tools/verify_release_source.py
python3 -B tools/validate_release.py build/release-validation
build_root="$PWD/build/tahoe"
xcodebuild -project itlwm.xcodeproj \
  -target AirportItlwm-Tahoe -configuration Release ARCHS=x86_64 \
  MODULE_VERSION=2.0.0 \
  SYMROOT="$build_root/products" OBJROOT="$build_root/intermediates" \
  SHARED_PRECOMPS_DIR="$build_root/precompiled" \
  CLANG_MODULE_CACHE_PATH="$build_root/module-cache" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Output: `build/tahoe/products/Release/Tahoe/AirportItlwm.kext`. The fw_gen dependency generates the embedded firmware source; it remains untracked. Do not replace pinned firmware or mix targets. Other inherited target versions are historical/upstream values, not the Tahoe product version.

Matching source does not guarantee identical UUID or executable bytes across toolchains. Verify downloaded packages with the release SHA256SUMS and BUILD_IDENTITY. A local rebuild needs its own explicitly reviewed identity profile; do not disable the runner's UUID/hash checks to accept it.

## Continuous integration

Contents-read-only CI uses macOS 15 Intel and Xcode 26.3 as a compatibility build, checks the 2.0.0 production/SDK/firmware and reference manifests, runs the actual-function host regression suite, then builds the dedicated Tahoe target. It does not publish, install or hardware-qualify a driver. Checkout disables persistent credentials. Output is labeled **CI BUILD — NOT PHYSICALLY VALIDATED**.

The earlier PR #5 check used the old frozen P8R1 source guard and failed when the intended HE boot-argument source differed. The final workflow protects the reviewed 2.0.0 input rather than applying that obsolete P8R1 identity to new code. Historical guards remain available for historical releases.

Host tests mock hardware/IOKit and cannot prove firmware acceptance, physical throughput, sleep/wake or long-term stability. Successful P10/R9 hardware campaigns are separately pinned in the result index. No additional hardware campaign was requested solely for release metadata.
