# Installing AirportItlwm-Tahoe 2.0.0

Install the complete **AirportItlwm.kext** from the [2.0.0 release](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v2.0.0).
Verify [SHA256SUMS.txt](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/SHA256SUMS.txt) before use. Do not substitute a CI artifact.

## Required framework and AppleVTD environment

[OCLP-CustoMac 3.0.0 or newer](https://github.com/kgp-macPro/OCLP-CustoMac/releases)
and the matching Ventura Legacy/Modern Wireless root patches are required.
The product target uses `__IO80211_TARGET=__MAC_13_0`, `USE_APPLE_SUPPLICANT`,
`itlwm/PrivateSPI.pch` and `IO80211FamilyLegacy`, with its companion patched
Apple dependencies. OCLP-CustoMac supplies that framework/patch environment;
this separate Intel driver supplies the Wi-Fi and device-specific DMA paths.
Follow the matching OCLP-CustoMac security/KDK/patch instructions for your OS.
AirportItlwm alone does not restore these frameworks or change EFI/security.

The documented AppleVTD configuration keeps OpenCore `DisableIoMapper=false`
with usable platform/DMAR settings and confirmed active AppleVTD. This flag
alone does not prove runtime IOMMU state or mapped Wi-Fi DMA. The runner checks
actual driver Mode 2 (mapped) or Mode 1 (conventional); it does not select the path.
BE200/AX210 campaigns prove AppleVTD+mapped. Fulvio’s successful historical
AX201/CNVW Alpha-2 tests from 4 November also record actual **Mode 2 (mapped)**
throughout all eight boundaries, 42/42 tests +online; a separate conventional
Mode 1 campaign also completed 42/42 +online. [Original evidence](docs/AX201_DMA.md); the previous blanket
conventional-only restriction was withdrawn. Separately configured conventional
Mode 1 comparisons remain valid. Do not derive the DMA route from the card label
or a Registry name such as CNVW. Use the [AX201 r13 commands](docs/benchmark/README.md#ax201cnvw-mapped-and-conventional-comparisons)
with profile `AX201-2.0.0`: Fulvio’s mapped command uses `EXPECTED_MODE=mapped`;
the separate conventional command uses `EXPECTED_MODE=conventional`. This variable
checks the actual mode and **does not switch it**. The historical result is not
a new 2.0.0 HE/BF hardware validation. AX211’s historical Alpha 2 mapped evidence
is likewise not new HE qualification. Conventional configurations remain supported within their
established device/firmware/runtime boundaries; do not change the whole
machine's IOMMU configuration merely to force a profile label.

## Manual installation and rollback

1. Back up your working EFI, Wi-Fi bundle and boot arguments. Keep a known
   bootable fallback. The original P10/R9 artifacts and historical releases
   remain separate from the newly numbered binary.
2. Use your established OpenCore EFI procedure to replace the **whole**
   `AirportItlwm.kext`, maintaining a correct `Kernel/Add` entry. Avoid duplicate
   copies/drivers claiming the same Intel controller. No install script is supplied.
3. Choose boot arguments below, reboot manually and verify the loaded version/
   UUID before benchmarking. Reapply only the appropriate OCLP root patches
   when your matching OS/patch procedure requires it.
4. If needed, restore the previously working complete bundle and saved boot
   arguments through the same manual EFI procedure. Do not mix an executable
   from one build with another Info.plist.

| Mode | Boot arguments |
|---|---|
| VHT |No HE/BF opt-in|
| HE |`-itlwmhe`|
| HE+SU-Beamformee opt-in |`-itlwmhe -itlwmbf`|

No `-novht` for these campaigns. BF alone does not enable HE. Firmware/NVM/AP
negotiation still determines actual mode and width. Check System Information,
bootargs and captured runtime diagnostics; a retail model name is not proof.

```sh
kmutil showloaded -b com.zxystd.AirportItlwm --show-mach-headers
sysctl -n kern.bootargs
networksetup -listallhardwareports
```

Release UUID `E01CC22A-6A2D-3E0D-A8FD-229ED62C1751`; executable SHA256 `ab8688febf2ccc78984ea6b2b9be933cb14317bc7951983330ffadab32d3a2f8`.
The benchmark candidate path is the on-disk identity reference for this exact
already loaded binary. It never installs the candidate or changes DMA mode.

## Benchmark peer — mandatory comparison conditions

| Wi-Fi test mode | Required wired peer link for comparable campaigns |
|---|---|
| VHT | **1000Base-T / 1 Gbit/s** |
| HE | **2500Base-T / 2.5 Gbit/s** |
| HE + BF opt-in | **2500Base-T / 2.5 Gbit/s** |

**Do not use 5000Base-T or 10GBase-T for these comparison campaigns.** The OWC
adapter supports 10 GbE but must be limited to the appropriate mode. Verify and
record the *negotiated* media on the wired peer. These are this project's
comparison-method conditions, not general IEEE requirements for HE.

See the [complete test guide](docs/benchmark/README.md) for server/client start,
IP prompt, five runners, profiles and returning the full result ZIP. LAN iperf
is separate from Internet networkQuality. Our 80/30 Internet connection does
not cap the wired-peer LAN measurement.

## Tested Bluetooth configuration

| Loaded kext in the BE200-P10 and AX210-R9 preflights | Version | Exact source |
|---|---|---|
| IntelBluetoothFirmware.kext |2.5.1|[lshbluesky v2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1)|
| IntelBTPatcher.kext |2.5.1|[lshbluesky v2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1)|
| BlueToolFixup.kext |2.7.2|[Acidanthera/BrcmPatchRAM 2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases/tag/2.7.2)|

These are separate Bluetooth components, not bundled AirportItlwm features.
Follow their own dependencies and instructions. Hackintool's
“Apple / THIRD_PARTY_DONGLE / 0x004C” does not identify the physical Bluetooth
chip. The separate Bananz0 BLE/AWDL analysis is not integrated into 2.0.0.

[Device validation matrix](README.md#device-mode-and-validation-scope) ·
[Detailed results](docs/results/2.0.0.md) · [Build/source provenance](docs/BUILD.md)
