# Installing AirportItlwm-Tahoe 1.0.0

The distribution is **AirportItlwm-Tahoe 1.0.0**; install **AirportItlwm.kext**, with executable `AirportItlwm` and identifier `com.zxystd.AirportItlwm`. Use the frozen release asset, not an unqualified CI build.

## Requirements

- Intel AX210 / PCI 8086:2725 is the physically qualified reference adapter.
- x86_64 macOS Tahoe with the restored Ventura legacy Modern Wireless environment, including `IO80211FamilyLegacy` and its companion Apple framework dependencies. Follow [OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac) for the required root-patch environment and its security/KDK requirements; this driver neither supplies nor replaces those patches.
- A separately maintained working OpenCore setup and recovery path. Source support for another Intel device does not establish its qualification.

## Configuration distinction

The AppleVTD-capable qualification used OpenCore `Kernel → Quirks → DisableIoMapper=false`, together with captured active AppleVTD runtime state. The conventional comparison used the established mapper-disabling configuration (`DisableIoMapper=true`), referred to internally as “Normal Mode.” That is project shorthand, not an official Apple/macOS/OpenCore mode. Configuration settings are KGP-controlled test inputs; `false` alone does not prove AppleVTD is active, and `true` does not prove that all devices in the machine operate without IOMMU translation.

For the optional AppleVTD path, the qualified combination is the restored framework environment, this release driver, and `DisableIoMapper=false`, with runtime AppleVTD presence verified. Do not infer that a single quirk change is sufficient for an arbitrary platform; preserve the other requirements of your established framework/bootloader setup. This release does not prescribe new DMAR/ACPI patches or machine-wide mapper policy.

KGP’s current public EFI distribution remains unchanged and continues to contain/recommend the established conventional Intel AirportItlwm configuration, including `DisableIoMapper=true` guidance where applicable. AirportItlwm-Tahoe is a separate, optional installation; it is not bundled into that EFI or OCLP-CustoMac. Existing users are not required to migrate.

## Manual installation

1. Keep a known-working recovery setup. Record the existing kext and OpenCore entries before replacing the driver.
2. Verify the release ZIP checksum and the extracted executable identity below. Do not rename or resign the bundle.
3. In your chosen OpenCore configuration, replace the existing AirportItlwm bundle with `AirportItlwm.kext`. The `Kernel → Add` entry uses `BundlePath=AirportItlwm.kext`, `ExecutablePath=Contents/MacOS/AirportItlwm`, `PlistPath=Contents/Info.plist`, `Enabled=true`. Preserve dependency ordering required by the restored legacy wireless environment. Do not load stock/diagnostic/Tahoe AirportItlwm variants simultaneously, or a competing Intel Wi-Fi driver for the same device.
4. Validate your OpenCore configuration using the validator matching your OpenCore version. Preserve required OCLP root patches; do not install this bundle into system library directories as part of this procedure.
5. After a user-controlled boot, verify the loaded image and binding, then check association and traffic. Interface names vary; `en2` is the reference system’s name, not a universal requirement.

## Read-only identity and state checks

Run in the directory containing the extracted bundle:

```sh
shasum -a 256 AirportItlwm.kext/Contents/MacOS/AirportItlwm
shasum -a 256 AirportItlwm.kext/Contents/Info.plist
dwarfdump --uuid AirportItlwm.kext/Contents/MacOS/AirportItlwm
kmutil showloaded --list-only | grep -i AirportItlwm
ioreg -r -c AirportItlwm -l -w0
ioreg -p IOService -n ARPT -r -l -w0
ioreg -r -c AppleVTD -l -w0
ioreg -r -c AppleVTDDeviceMapper -l -w0
ifconfig -l
ifconfig awdl0
scutil --nwi
```

The disk hash establishes the file you inspected; loaded UUID and provider binding establish additional runtime evidence. A file name alone does not prove which image is loaded. A missing `AppleVTDDeviceMapper` class-query result alone does not prove that AppleVTD is absent. Configuration intent and runtime evidence must be read together.

| Field | Frozen 1.0.0 release identity |
|---|---|
| Bundle / executable | `AirportItlwm.kext` / `AirportItlwm` |
| Identifier | `com.zxystd.AirportItlwm` |
| Short / bundle version | `1.0.0` / `1.0.0` |
| Target / configuration / architecture | `AirportItlwm-Tahoe` / `Release` / `x86_64` |
| LC_UUID | `7039DFF1-232B-3697-8E6E-8EE505044F28` |
| Executable SHA-256 | `1202ec9bf855e944f59cc3742a90c0cf1a0a292642dfe7ed8d091a1a9c435811` |
| Info.plist SHA-256 | `05f027cc5978593c3542942488f27537fe5fb04c48b3817d7ec5025d3bc1c288` |

The `networksetup -getairportnetwork` API can report “not associated” despite active Wi-Fi, IPv4 and successful bound traffic in this restored environment. Use combined interface, driver and traffic evidence; do not treat that one API result as a functional failure.

## Scope

**Important scope note:** This kext does **not** claim to restore AirDrop, Continuity Camera, or Personal Hotspot. These features, and Intel BE200 support, remain future development gates. Broader post-1.0.0 development is planned to resume starting in October 2026; this is a development plan, not a promised completion date or feature order.

The full public qualification summary is in the repository’s [qualification document](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/docs/QUALIFICATION.md). No release asset installs itself or changes EFI, OpenCore or system configuration.
