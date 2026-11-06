# AirportItlwm-Tahoe benchmark tools r13

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
