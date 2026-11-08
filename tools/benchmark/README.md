# AirportItlwm-Tahoe benchmark tools r14

**r14 corrects the packaged test instructions. The five r13 runners, their
traffic matrices, helper resources, identity profiles and guards are unchanged.**
Runner-generated provenance therefore continues to identify the r13 runner.

**AX201/CNVW supports mapped DMA. Fulvio's regular configuration is mapped;
conventional is a separate comparison, not an AX201 requirement.**

| DMA configuration | OpenCore Kernel → Quirks → DisableIoMapper | Runner expectation | Required actual mode |
|---|---|---|---|
| mapped | **false** | `EXPECTED_MODE=mapped` | **Mode 2** |
| conventional | **true** | `EXPECTED_MODE=conventional` | **Mode 1** |

Reboot after changing the EFI quirk or Wi-Fi boot arguments. The expectation
only checks the driver mode; it cannot change it. Record active AppleVTD
separately in the mapped configuration. Preserve all identity and mode guards.

| Wi-Fi campaign | Wi-Fi boot arguments | Wired peer media | Runner |
|---|---|---|---|
| VHT | No `-itlwmhe` or `-itlwmbf` | **1000Base-T / 1 Gbit/s** | Standard 3×20 |
| HE | `-itlwmhe`, no `-itlwmbf` | **2500Base-T / 2.5 Gbit/s** | 3×20 UDP1300 |
| HE + BF opt-in | `-itlwmhe -itlwmbf` | **2500Base-T / 2.5 Gbit/s** | 3×20 UDP1300 |

No `-novht`. Record actual negotiated peer media, Wi-Fi PHY/width and boot
arguments for each separate campaign. Keep the same extended matrix for HE and
HE+BF. Higher UDP1600/1700 matrices remain separate stress comparisons.

All five 3×20 runners, runner_profiles, optional AX diagnosis and offline checks
are included. [Complete guide](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/benchmark/README.md).
The five traffic matrices are unchanged from the tested P10R9 tools. New 2.0.0
profiles pin the versioned release; original P10/R9 profiles keep their pins.
The old 3×14 script remains for history. Verify this tools package with its
SHA256SUMS.txt. Offline bundle/matrix/identity/health verification:

```sh
python3 VERIFY_TOOLS.py --kext /absolute/path/AirportItlwm.kext
```

This checks the actual release bundle without starting traffic or querying
hardware. Optional --p10-kext verifies a separately retained original P10
bundle too. A failed identity must never be bypassed. All dependent resources
must stay alongside the runners. Use actual tester IP/interface, not a fixed
en2 or private address assumption. No script installs a kext.

Mandatory peer link: VHT 1000Base-T; HE/HE+BF 2500Base-T. Never 5/10G for these
comparisons. Standard runner first for VHT, extended RX stress for HE; UDP1700
is not a loss-free guarantee. The guide covers mode/bootargs, server/client
commands, IP prompt, output path and returning the complete ZIP.
