# AirportItlwm-Tahoe 1.2.0 BE200 Alpha 1 — installation, first checks and rollback

**Historical Alpha 1 installation reference.** For current release identity use the [release notes](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.2.0-be200-alpha.2); for throughput work use the [maintained shared guide](../benchmark/README.md). The commands/UUID below belong to Alpha 1, not a universal current candidate.

**Required: Retro-OCLP-CustoMac root patching.** This BE200 alpha uses the restored Ventura legacy wireless frameworks and requires the corresponding root patches provided by [Retro-OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac). Keep the matching framework/Skywalk and AMFIPass/`-amfipassbeta` boot configuration; this is not a native, unpatched Wi-Fi driver.

**Test environment: macOS Tahoe 26.7.2 (build 25G332), x86_64, with AppleVTD enabled.**

**Further performance optimization is planned.** The current alpha focuses on initial functionality and recovery testing; its performance results are preliminary.

**Bluetooth setup used during these BE200 tests:** [BlueToolFixup.kext v2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases) by acidanthera, together with [IntelBluetoothFirmware v2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1) from lshbluesky. The Intel USB Bluetooth controller was identified as **8087:0036**, and both Magic Keyboard and Magic Mouse were connected during testing.

## Install manually

1. Download the [complete Alpha ZIP](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v1.2.0-be200-alpha.1/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-1.zip) and extract it. No compilation is needed.
2. Confirm the BE200 PCI identity and required matching restored Ventura legacy wireless/root-patch environment. Keep a known working boot method and a complete backup of the current AirportItlwm.kext and boot configuration. This ZIP does not install the root patches, AMFIPass or Bluetooth kexts.
3. In your existing user-managed OpenCore installation, replace the complete AirportItlwm.kext at its established location with the supplied bundle. Do not merge files, keep duplicate competing copies or load it into the running system. Preserve your matching kernel entry/order and framework/boot configuration. No script here edits EFI.
4. Reboot manually. Check the **loaded** UUID `3610B51E-B7CD-3822-94BD-61EA7C138D99` and Wi-Fi binding; an internal 1.0.0 version or a Desktop filename alone does not identify this Alpha.

## Minimal first checks

Record initial login autojoin, a fresh scan/join if required, DHCP address/router and actual Wi-Fi route. Determine the actual Wi-Fi interface from its AirportItlwm IORegistry binding; do not assume en2 or ARPT on another machine. Keep Ethernet routing separate. The supplied optional collector resolves the associated infrastructure interface, rejects an ambiguous mapping, and does not toggle, join or send test traffic:

```sh
cd "$HOME/Downloads/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-1"
sudo /bin/bash ./COLLECT_BE200_JOIN.sh
```

Adjust the extracted directory path if your browser chose another location. A compatible Python 3 interpreter is needed only for the optional collector/decoder. Its output directory/ZIP is printed at completion. Capture one manually controlled OFF/ON cycle per attempt. If a failure occurs, preserve that state and original capture before retrying. One manual Sleep/Wake is a separate check. Avoid System Profiler queries and parallel scans during throughput work; no broad iperf matrix is required for this initial Alpha check.

The Internet test shown is 79.5/28.5 Mbps on a nominal 80/30 connection. It is not a LAN benchmark; local throughput remains a separate investigation. All performance results are preliminary.

## Roll back

Restore the complete previous AirportItlwm.kext and its matching backed-up boot configuration, then reboot manually. Merely swapping a Wi-Fi kext does not remove or restore existing root patches. If a previous framework/patch state is needed, use the corresponding project's documented manual recovery procedure; these tools never apply/remove patches automatically.

## Status and limits

The current candidate passed the four OFF/ON cycles and immediate login/Sleep/Wake joins reported by KGP. These scoped observations do not qualify long-term reliability, other cards/modes, Wi-Fi 7, MLO, 6 GHz, BE201 or full Continuity features. BlueToolFixup/IntelBluetoothFirmware are separate dependencies, not bundled Wi-Fi functionality. See the [Alpha campaign](https://github.com/kgp-macPro/AirportItlwm-Tahoe#current-be200-alpha-testing-campaign) and [build/source correspondence](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/v1.2.0-be200-alpha.1/docs/be200-alpha/BUILD.md).
