# Pinned offline BE200 test references

These are unchanged files from the completed source/binary audit, copied here so the Alpha tests do not depend on a private Desktop path. No reference source is linked into the kext.

- Linux iwlwifi v6.9: a38297e3fb012ddfa7ce0321a7e5a8daeb1872b6; `drivers/net/wireless/intel/iwlwifi/fw/api/tx.h` and `fw/pnvm.c`. Original Intel notices and GPL-2.0 OR BSD-3-Clause SPDX identifiers are retained.
- The host wire fixture contains 29 layout assertions against the pinned Linux v6.10 0c3836482481200ead7b416ca80c68a29cfdaabd, AirportBZ 0ccab1627764406861fb9eb0a06ca36e31b4440f and original IWX definitions. Exact origins/hashes are in analysis/HOST_ABI_PROVENANCE.json. It is a test fixture, not recovered binary source.
- Existing Apple synchronize test reference and its APSL notice remain in the adjacent reference directory.

License texts: repository LICENSE (GPLv2), and the Linux dual-license references retain the BSD option; `LICENSES/BSD-3-Clause.txt` below. Intel redistributable firmware has its own adjacent firmware license. No license is replaced by this summary.

- Exact frozen ZC15 baseline source `f2a02dcd7cd0f44c8ec2804428103beccca4d2b0:itlwm/hal_iwx/ItlIwx.cpp` is exported unchanged under legacy/. That commit is not an ancestor of this branch, so a clean public checkout must not depend on a private Git object. Its original notices are retained; only test extraction reads it.
