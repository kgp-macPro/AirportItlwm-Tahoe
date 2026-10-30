# AirportItlwm-Tahoe 1.2.0 BE200 Alpha 2 — Performance Hotfix — installation and rollback

**Required: Retro-OCLP-CustoMac root patching.** This BE200 alpha uses the restored Ventura legacy wireless frameworks and requires the corresponding root patches provided by [Retro-OCLP-CustoMac](https://github.com/kgp-macPro/OCLP-CustoMac). Retain the matching Skywalk/framework and AMFIPass/`-amfipassbeta` boot configuration. This is not a native/unpatched Wi-Fi implementation.

**Test environment: macOS Tahoe 26.7.2 (build25G332), x86_64, with AppleVTD enabled.**

1. Download/extract [AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2.zip](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v1.2.0-be200-alpha.2/AirportItlwm-Tahoe-1.2.0-BE200-Alpha-2.zip).
2. Back up the complete existing AirportItlwm.kext and boot configuration.
3. Manually replace the whole bundle in its existing enabled OpenCore location
   with the complete supplied AirportItlwm.kext. Do not merge or enable duplicate
   variants. Preserve the required working root patches/boot environment.
4. Reboot manually; loaded UUID must be`E4414AAB-1D58-3E40-93A5-6838D3CDCD20`.
   The internal1.0.0version alone is not identification.

Record autojoin, IPv4/router and Wi-Fi-bound connectivity; simultaneous Ethernet
can otherwise hide which interface carries Internet traffic. Determine the actual
PCI/provider/infrastructure-interface binding; do not assume ARPT/en2 or infer
DMA from a device label. No supplied tool installs/loads a Kext or changes
EFI, root patches, Wi-Fi power, services or routes.

Optional read-only boundary collection: `COLLECT_BE200.sh` accepts a simple
stage name and discovers PCI providers when no provider override is supplied.
For a focused manual join failure, `COLLECT_BE200_JOIN.sh` additionally requires
the **actual verified** `WIFI_IFACE` and `IOREG_WIFI_SERVICE` values; it stops
on missing/mismatched/ambiguous bindings. Set those two environment variables
before invoking it. Python3 is needed for the optional resolver/decoder.
Do not run collection/scans/System Profiler concurrently with throughput tests.

For an optional full comparison, the unchanged r10 common runner is included
under TOOLS with its profiles. After installing/rebooting Alpha2, from the
extracted package directory run:

```sh
CANDIDATE_KEXT="$PWD/AirportItlwm.kext" CANDIDATE_LABEL=BE200-P8 \
  DEVICE_LABEL=BE200 WIFI_IFACE= IOREG_WIFI_SERVICE= \
  /bin/bash ./TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh BE200-P8 mapped
```

Run the existing wired peer separately with `iperf3 -s -p 5201`, its Wi-Fi
manually off. The client prompts for its current IPv4 and verifies the actual
Wi-Fi binding/route/loaded identity and Mode2+AppleVTD. These arguments CHECK,
not select, boot/DMA settings. Other supported systems must select their actual
verified mode. All original three-round14-case commands/pauses/output guards are
unchanged. Preserve failed/slow result ZIPs too.

**Known limits:** Find My remains unresolved. KGP reports successful Wi-Fi OFF/ON
and manual Sleep/Wake tests with P8R1 (USER-OBSERVED); this is not long-term or
exhaustive recovery qualification. Alpha1's earlier observations remain historical.
This one benchmark qualifies the BE200 environment;
AX201/AX210/AX211 remain active shared1.2.0 development targets. Full Wi-Fi7,
MLO,6GHz,BE201,AirDrop/AutoUnlock/fullAWDL are not claimed. More performance
optimization is planned; UDP loss, high-rate UDP sender limits and TCP RX
variation/retransmissions remain recorded.

Bluetooth is separate: the earlier demonstrated setup used
[BlueToolFixup2.7.2](https://github.com/acidanthera/BrcmPatchRAM/releases) by
acidanthera and [lshbluesky IntelBluetoothFirmware2.5.1](https://github.com/lshbluesky/IntelBluetoothFirmware/releases/tag/v2.5.1), IntelUSB8087:0036.

Rollback: restore the complete previous known-working bundle and its matching
boot configuration, then reboot manually. Swapping Kexts does not undo existing
root patches. Alpha1 remains available unchanged in release history.
