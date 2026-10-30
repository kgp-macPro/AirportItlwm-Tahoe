# Building AirportItlwm-Tahoe 1.2.0 BE200 Alpha 2 — Performance Hotfix

The tested binary was built from `a10a32bf296d6b90da1b3390b3ef4459f9d207fc`. The Alpha2 tag adds only
publication documentation, the unchanged tested r10 benchmark enclosure and an
updated strict source manifest/CI label. It does **not** change the tested driver,
firmware, SDK or Kext bytes. `tools/verify_be200_release_source.py` verifies all
1490 production/firmware/SDK inputs against the actual build record plus
the pinned offline reference hashes. The old P7 manifest remains historical.

Use [be200-alpha](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/be200-alpha) or [v1.2.0-be200-alpha.2](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/v1.2.0-be200-alpha.2).
Main retains the frozen1.0.0 driver source and cannot produce this Alpha.
The release supplies the actual tested binary; rebuilding creates a new,
hardware-untested binary and need not reproduce its UUID/hash.

## Pins and actual build

- Local tested build: Xcode26.5/17F42, Apple clang21, MacOSX26.5SDK,
  `AirportItlwm-Tahoe / Release / x86_64`, restored Ventura legacy frontend.
- Standalone vendored MacKernelSDK05094e5e88cec7caedbfb35e8449ed0db94bf95b,
  with `configureInterrupts` declared at reserved virtual slot3.
  Patched header SHA2569255ed3c77cf30f153ef8a74fce5f9a981544bd066b09b10f946fd78e783704b;
  see [the exact declaration change](SDK_CONFIGURE_INTERRUPTS.diff).
- HuajiFruit/AirportBZ0ccab1627764406861fb9eb0a06ca36e31b4440f; Apple
  IOPCIFamily4822b27a36e2de70e231ecf2bf3021384fab6ec2; pinned Linux wire and
  ownership references/notices are retained in [provenance](../BE200_PROVENANCE.md).
- GL-C0/FM-C0 API92 firmware SHA256
  c7092f1f78676f9e9c2c4d12716401db2116b009c5d7c464e8387c5b1c645d6f.
- PNVM SHA256f481e3b0403ca43a0a444b01c04fa4d1582868afcb026b1e697905f0039ccfa3.
  Firmware/PNVM and Intel notices remain unchanged.

## Validate/build

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
python3 tools/verify_be200_release_source.py
python3 -B tools/validate_be200.py build/be200-validation
build_root="$PWD/build/tahoe"
xcodebuild -project itlwm.xcodeproj -target AirportItlwm-Tahoe \
  -configuration Release ARCHS=x86_64 MODULE_VERSION=1.0.0 \
  SYMROOT="$build_root/products" OBJROOT="$build_root/intermediates" \
  SHARED_PRECOMPS_DIR="$build_root/precompiled" \
  CLANG_MODULE_CACHE_PATH="$build_root/module-cache" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
python3 tools/verify_be200_release_source.py
```

Output: `build/tahoe/products/Release/Tahoe/AirportItlwm.kext`.
Actual private build/validation records accompany the release ZIP;35/35 local
groups passed and one Release build succeeded. Hosted CI runs those regressions
and an additional unqualified Xcode26.3/macOS15Intel build. CI output never
replaces the tested release artifact. Generic historical1.0.0 `verify_source.py`
and `prepare_sdk.py` are not this Alpha branch's entry points.

Frozen internal revisionP8R1, marker`AirportItlwm-Tahoe-BE200-GL92-PRIVATE-P8R1`,
UUID`E4414AAB-1D58-3E40-93A5-6838D3CDCD20`, executableSHA256
`8e4a38121b06941630afa98728b245cbaeec813c14139c46715dc44723461f63`.
Bundle/identifier/internal1.0.0version are unchanged; Alpha2 is the public
distribution identity. These build checks are not physical recovery or other
hardware qualification. The r10 source enclosure is unchanged toolcommit
`1ff414259b0b7e11fd93966c4c84892de7adac58`; its profile name`BE200-P8` pins P8R1,
not the superseded failedP8. Frozen r7/r8/r9 references remain intact.
