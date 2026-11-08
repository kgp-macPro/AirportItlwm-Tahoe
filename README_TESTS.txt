# Current test instructions — AirportItlwm-Tahoe 2.0.0 / tools r14

Use the [maintained benchmark guide](docs/benchmark/README.md) and its complete
five-runner package. AirportItlwm-Tahoe 2.0.0 is the current driver. Alpha 2/P8R1,
older beta/ZC candidates and r11/r12/r13 instruction packages are historical.
r14 corrects the packaged instructions while retaining the exact r13 runners,
profiles, helper resources, traffic and guards.

AX201/CNVW supports mapped DMA. Fulvio's regular configuration is mapped Mode 2;
conventional is a separate comparison, not an AX201-specific requirement.

| DMA configuration | OpenCore Kernel → Quirks → DisableIoMapper | Runner expectation | Actual mode |
|---|---|---|---|
| mapped | **false** | EXPECTED_MODE=mapped | **2** |
| conventional | **true** | EXPECTED_MODE=conventional | **1** |

Reboot after changing the EFI quirk or Wi-Fi boot arguments. EXPECTED_MODE
only checks the driver mode; it does not switch it. Preserve all guards.

| Separate Wi-Fi campaign | Boot arguments | Required wired second-device link | Runner |
|---|---|---|---|
| VHT | No -itlwmhe or -itlwmbf | **1 Gbit/s / 1000Base-T** | Standard 3×20 |
| HE | -itlwmhe, no -itlwmbf | **2.5 Gbit/s / 2500Base-T** | 3×20 UDP1300 |
| HE+BF opt-in | -itlwmhe -itlwmbf | **2.5 Gbit/s / 2500Base-T** | 3×20 UDP1300 |

No -novht and no 5/10-Gbit/s peer for these comparisons. Record actual negotiated
Ethernet media, Wi-Fi PHY/width and boot arguments for each separate ZIP.
Keep the same extended matrix for HE and HE+BF. The guide supplies full server/
client commands, actual interface/IP selection, identity/mode checks and result
return instructions. Each runner performs 60 local tests and one online test.
Download: https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-Benchmark-Tools-r14.zip
ZIP checksum: https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-Benchmark-Tools-r14.zip.sha256

The mapped route is already proven; no extra campaign is required merely to
re-establish AX201 Mode 2. A full comparison means three Wi-Fi modes under mapped
and the same three under conventional, with the appropriate reboots.

The separately named laobamac controls remain optional historical AX-family
comparisons and do not support BE200. Keep their framework/build context separate.
Historical driver binaries, release tags and original result ZIPs are preserved.
