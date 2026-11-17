# AirportItlwm-Tahoe 2.1.0 Beta 1 — Experimental AirDrop (R49)

Experimental native AirDrop for Intel **BE200, AX210, AX211 and AX201** on
macOS Tahoe, using this project's Ventura Legacy wireless environment and
AppleVTD-capable driver.

Bidirectional file transfers have been demonstrated on **BE200 and AX210**
alongside usable infrastructure Wi-Fi. **AX201 and AX211 testers are welcome**;
their applicable paths are included, while current AirDrop hardware results
are still needed. Existing AX201 CNVW/Mapped-2 support is retained.

**This is a pre-release. Version 2.0.0 remains the regular Wi-Fi release.**
Connection setup and transfer speed are still being improved, and some
Mac-to-Mac attempts can be delayed or fail.

## Downloads

| File | Purpose |
|---|---|
| [AirportItlwm-Tahoe-2.1.0-beta.1-RELEASE.zip](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.1.0-beta.1/AirportItlwm-Tahoe-2.1.0-beta.1-RELEASE.zip) | Matching Wi-Fi and Bluetooth kexts, installation instructions, optional diagnostics and component identities. |
| [AirportItlwm-Tahoe-2.1.0-beta.1-SOURCE.zip](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.1.0-beta.1/AirportItlwm-Tahoe-2.1.0-beta.1-SOURCE.zip) | Corresponding Wi-Fi and modified BlueToolFixup sources, build instructions and dependency/source records. |
| [SHA256SUMS.txt](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.1.0-beta.1/SHA256SUMS.txt) | Checksums for the release files. |

The runtime package contains the unchanged **AirportItlwm R49** together with
project-modified **BlueToolFixup 2.7.5**, original **IntelBluetoothFirmware
2.5.3** and original **IntelBTPatcher 2.5.3**. BlueToolFixup 2.7.5 identifies our
modified build, not an official Acidanthera release. Installation is manual;
follow INSTALL.md and retain a working rollback configuration.

## Required setup

- OCLP-CustoMac 3.0.0 or newer with the matching Ventura Legacy wireless root
  patches. This beta uses `IO80211FamilyLegacy`.
- Infrastructure Wi-Fi on **5 GHz primary channel 44**. The private AWDL
  context is fixed to **44/20 MHz**; the tested infrastructure configuration
  retains 160 MHz.
- The supplied Bluetooth combination and its dependencies, including
  **Lilu 1.7.2 or newer**.
- HE without beamforming for the documented beta setup; leave `-itlwmbf`
  disabled initially.

Documented additional boot arguments:

```text
-itlwmhe -itlwm-awdl -itlwmbeawdldpnoinfra -kgpble
```

`-kgpble` enables the additional BLE patches in our BlueToolFixup build.
`-itlwmbeawdldpnoinfra` omits optional infrastructure information from the
AWDL Data-Path TLV; it does not disable the normal Wi-Fi connection.
BTLEAdvFixup is not required by this setup.

## Known limitations

- Mac-to-Mac discovery/session setup can be slow, and some transfers fail.
  Successful sessions do not establish universal reliability. Reported
  iPhone results have generally been more consistent.
- This is a fixed-channel beta; other AP channels and automatic private
  channel hopping are outside the documented setup.
- Instant Personal Hotspot remains unreliable. The adapted WPA2/WPA3
  compatibility change is included, but BLE setup and WLAN network matching
  can still fail.
- Continuity Camera is not qualified by these AirDrop results.
- The beta is not locked to macOS 26.7.2. Later builds still require compatible
  Legacy interfaces; future macOS compatibility is not guaranteed.

## Source and credits

The corresponding Wi-Fi and modified BlueToolFixup code is published in the
same [AirportItlwm-Tahoe repository at this release's source tag](https://github.com/kgp-macPro/AirportItlwm-Tahoe/tree/v2.1.0-beta.1). BlueToolFixup
is under `bluetooth/BlueToolFixup-Intel/`, with shared headers under
`bluetooth/Common/`. The unchanged Intel Bluetooth components are separately
identified with their original sources.

This work combines OpenIntelWireless/OpenBSD foundations, ported and adapted
Intel/Linux firmware mechanisms, Bananz0's AWDL/BLE work, laobamac's HE/macOS
work, crazyhulk's AWDL PR #53, mingww64's WPA transition-mode fix and specific
ForbiddenItlwm adaptations with our own Legacy, AppleVTD and firmware
integration. See the README's full Credits section and the preserved licenses.

## Testing and feedback

Please report the card and PCI/subsystem IDs, macOS build, WLAN/Bluetooth
versions, AP channel, boot arguments, peer device and transfer direction.
Record actual file arrivals, start delays, failures and whether Internet
access remains usable.

For an infrastructure comparison, disable Ethernet during the test. AirDrop
may still choose local Wi-Fi through the AP; that is valid behavior. Optional
paired captures distinguish it from an actual AWDL transfer.

<details>
<summary>Frozen build identity</summary>

This release packages the tested binaries without rebuilding them. The
public package version is 2.1.0-beta.1; AirportItlwm's internal bundle version
remains 2.0.0. Identify the beta by **R49**, its UUID and executable hash:

- WLAN runtime source: `c9a72c638e72945e8dd6b34728c35b6fca948a44`.
- WLAN UUID: `557EF8F5-45E3-3B78-B2AE-D1B3B0C4EED0`.
- WLAN executable SHA-256:
  `6359207e57bba95178712e30ef7c572156baf07131e9139d07afc2aa180af925`.
- Modified BlueToolFixup runtime source:
  `1f27753f9b757dae59d995dafb083fd7dd9f61ba`.

</details>
