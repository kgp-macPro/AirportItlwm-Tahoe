# Technical lineage and source provenance

## Pinned foundation

| Input | Commit | Tree |
|---|---|---|
| DexterSLamb/itlwm | `51543d6cfecf8aae3f66792b0699f5a1bb16a368` | `51f28a2669aaba21bc51cac645975dc7314555b8` |
| acidanthera/MacKernelSDK | `05094e5e88cec7caedbfb35e8449ed0db94bf95b` | `fbcf7dd1826765b762b40f88b77fc37e1fb93e33` |

The public tree is a clean export of the pinned Dexter source with the exact frozen 1.0.0 source patch and qualification-documentation overlay applied, followed only by publication documentation/CI/tooling changes. No canonical Git history was copied into the new repository and no runtime source was manually reconstructed.

The preserved upstream baseline has **no `.gitmodules` and no MacKernelSDK gitlink**. It uses an ignored standalone `MacKernelSDK` directory. This relationship is preserved: dependency metadata and the setup tool acquire the exact pinned SDK; the SDK remains separately licensed and is not vendored as new driver source. Normal builds use the already materialized tree; no internal patches are required.

## Functional lineage

- **2H:** prepared mapper-aware RX backing.
- **2I / 2J:** prepared ordinary-TX backing, retained through matching completion, and corrected idle-stop admission handling.
- **2K:** prepared large-command backing; inline commands preserve their established path.
- **2M:** coalesced same-workloop recovery from transient command-gate contention; management frames no longer remain stranded solely on a failed nonblocking gate acquisition.
- **2P:** NorthKoreanNoodles / maddog860 direct VIF sequence: existing creation/init, checked `attach(this)`, `fAWDLInterface` assignment, then one `registerService()` publication.
- **PC1 / 2Q:** checked power-cycle recovery with completion fencing and new-generation admission, retaining uncertain old mapped backing rather than reusing it.

PC1 functionally changes only ItlIwx.cpp and ItlIwx.hpp relative to frozen 2P. NKN VIF and accepted mapper/drain mechanisms remain intact. Rejected 2N/2O lifecycle frameworks, later historical AirDrop experiments and diagnostic-only DIAG1 policy are not implementation inputs.

NKN public reference commits: `bc71f590dee974ce9356a08c6572e00e08970d52` and `1cb855c6276ecd77f2ee1966b310006d89127b6f`, acquired tree `6bb8df3af09a6ed06f71d53d00e6eef299bc602f`. Historical narrow KGP VIF work was reviewed as independent prior art, not imported wholesale. Authorship is described in [CREDITS.md](../CREDITS.md) without priority claims.

## Product target and version

AirportItlwm-Tahoe clones the Ventura legacy target family: `USE_APPLE_SUPPLICANT`, `AIRPORT`, `__PRIVATE_SPI__`, `__IO80211_TARGET=__MAC_13_0`, `itlwm/PrivateSPI.pch`, and `IO80211FamilyLegacy`. No `__MAC_26_0` or Sonoma Skywalk hybrid is introduced. Target-local MODULE_VERSION is 1.0.0; bundle identifier, matching personality, executable and legacy dependency versions are preserved. Product output is `Release/Tahoe/AirportItlwm.kext`.

High Sierra, Mojave, Catalina, Big Sur, Monterey, Ventura, Sonoma 14.0 and Sonoma 14.4 remain in the project. Their source families are preserved, not newly physically qualified. The inherited inert source-build-file entry without a fileRef is retained; target membership and fw_gen dependency remain equivalent to Ventura. The inherited deployment floor does not claim older-OS support for this Tahoe distribution.

## Frozen build versus publication tooling

The authoritative full source patch SHA-256 is `3c4b13ea572896ec6797dba1b90c58cd194f1b5a1cc729d468d8b76488ce6098`; the documentation overlay SHA-256 is `74dab5f7ce579e3dc94431ed7bcedc47a16d8628c4a655ac5bf15281021cc1a0`. These are archival provenance identifiers, not downloads required to build this tree. Raw patches and captures remain in separate project evidence.

Publication replaces the inherited auto-release workflow with contents-read validation CI, adds pinned dependency setup and source/target checks, and carries the existing host models with portable fixture paths. It does not rebuild or alter the qualified kext. The manifest in `tools/frozen_source_sha256.json` protects all inherited/materialized non-documentation source inputs, including firmware, headers, project and licenses.

The retained single release build used Xcode 26.5 / 17F42, Apple clang 21.0.0 (clang-2100.1.1.101), MacOSX26.5 SDK, and generated FwBinary.cpp SHA-256 `ea888268197043f2b987db320c953ac8dbf2bdbd04f19bb34be986d0849d82cc`. Generated source is not stored in this tree. See [BUILD.md](BUILD.md) for direct build commands and the CI/release distinction.

The exact frozen binary passed both 26.6.2 release gates and additional scoped 26.7 tests without rebuilding. [QUALIFICATION.md](QUALIFICATION.md) preserves the evidence grades; source equivalence and host models alone do not establish physical qualification of a new build.
