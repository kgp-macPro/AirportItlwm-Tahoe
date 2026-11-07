# AirportItlwm-Tahoe 2.0.0 benchmark guide — tools r13

[Download all five runners and resources](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/download/v2.0.0/AirportItlwm-Tahoe-Benchmark-Tools-r13.zip)
· [Release/checksums](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v2.0.0) · [Installation](../../INSTALL.md)

**OCLP-CustoMac 3.0.0+ and matching Ventura Legacy patches are required.**
See [AppleVTD versus actual DMA mode](../../INSTALL.md#required-framework-and-applevtd-environment).
Use the 2.0.0 release as the active starting point. Old 3×14/r12 and comparison
builds remain [historical downloads](../HISTORY.md), not a second conflicting
current installation path. Retained laobamac comparison builds do not support BE200.

## Required peer link, before every comparison

| Wi-Fi test mode | Required wired peer link for comparable campaigns |
|---|---|
| VHT | **1000Base-T / 1 Gbit/s** |
| HE | **2500Base-T / 2.5 Gbit/s** |
| HE + BF opt-in | **2500Base-T / 2.5 Gbit/s** |

**Do not use 5000Base-T or 10GBase-T for these comparison campaigns.** The OWC
adapter supports 10 GbE but must be limited to the appropriate mode. Verify and
record the *negotiated* media on the wired peer. These are this project's
comparison-method conditions, not general IEEE requirements for HE.

Identify the OWC Ethernet interface on the **wired second Mac**, select the
appropriate speed manually in its network hardware settings, and record:

```sh
networksetup -listallhardwareports
SERVER_IFACE=enX                 # replace with the actual wired interface
ifconfig "$SERVER_IFACE"         # preserve the media/negotiated speed in your notes
```

Do not assume 10 GbE because of the adapter name. AP channel 44/160 MHz is the reference
setup, not a hardcoded requirement of every supported AP. Keep other testbed
settings fixed and record mode, width, OS/firmware/patches and both interfaces.
Disable the peer's Wi-Fi and the Wi-Fi client's competing wired path manually
when reproducing KGP's isolated topology; the runner never changes interfaces.

## Server start

On the wired Mac, use its actual IPv4. For KGP's setup it was 192.168.178.197.
The MacBook Pro ran iperf 3.22 (cJSON 1.7.15).

```sh
SERVER_IP=192.168.178.197        # replace for your wired Mac
iperf3 -s -B "$SERVER_IP" -p 5201
```

Keep this terminal running. Record version and negotiated Ethernet mode. No
router port forwarding or Internet server is needed for this LAN measurement.

## Client start and identity reference

Manually install/reboot with the chosen mode first: VHT (no opt-in), HE
`-itlwmhe`, or HE+BF `-itlwmhe -itlwmbf`; no `-novht`. Confirm negotiated PHY/width.
Unzip the full tools package; do not copy an individual shell file without its
`runner_profiles/` and helper resources. Set **your** Wi-Fi interface from
`networksetup -listallhardwareports`; KGP's en2 is not universal.

Example for AX210 mapped (the server IPv4 is prompted when IPERF_SERVER is unset):

```sh
TOOLS="$HOME/Downloads/AirportItlwm-Tahoe-Benchmark-Tools-r13"
KEXT="$HOME/Downloads/AirportItlwm-Tahoe-2.0.0/AirportItlwm.kext"
WIFI_IFACE=en2 DEVICE_LABEL=AX210 CANDIDATE_LABEL=AX210-2.0.0 EXPECTED_MODE=mapped \
  /bin/bash "$TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" "$KEXT"
```

Before this VHT example, verify **1000Base-T** at the wired peer. For HE or
HE+BF, verify **2500Base-T**, then choose an extended runner as appropriate:

```sh
# HE/HE+BF comparison: wired peer MUST be 2500Base-T, never 5000/10GBase-T.
WIFI_IFACE=en2 DEVICE_LABEL=AX210 CANDIDATE_LABEL=AX210-2.0.0 EXPECTED_MODE=mapped \
  /bin/bash "$TOOLS/AirportItlwm_3x20_UDP1300_PORTABLE_IP_PROMPT.sh" "$KEXT"
```

For BE200 add `BENCHMARK_PROFILE=be200`, use `DEVICE_LABEL=BE200` and
`CANDIDATE_LABEL=BE200-2.0.0`, mapped. For AX201/CNVW, follow the separate
[mapped and conventional commands below](#ax201cnvw-mapped-and-conventional-comparisons). For an AX211
use its matching `AX211-2.0.0` profile and **observed** runtime mode; new HE/BF
hardware validation remains pending. Profiles are not a device-identification
shortcut: the runner verifies the actual Intel PCI provider, controller and
infrastructure interface. Do not force mapped or bypass a guard to pass.

`KEXT` points to the exact complete on-disk bundle already loaded. The runner
checks loaded UUID, executable/Info hashes, marker, source/profile, binding,
IP/interface route, actual DMA and faults. The path does not install it.
`IPERF_SERVER=your.server.ipv4` bypasses the prompt; changing an expected mode
cannot change the actual driver mode. Missing optional AX diagnosis cannot
block BE200. Retained P10/R9 profiles have their original different pins.

## AX201/CNVW: mapped and conventional comparisons

Fulvio’s successful historical ZC28 campaign records **actual mapped Mode 2**
on PCI `8086:7a70`, subsystem `8086:0074`, under CNVW. The previous blanket
“AX201/CNVW conventional only” instruction is withdrawn. These historical results
retain their original driver/framework context; they do not qualify the new
2.0.0 HE/BF code on AX201. [Evidence and correction history](../AX201_DMA.md).

Use the full **r13** package and profile **AX201-2.0.0** with the already loaded
2.0.0 bundle as the identity reference. The runner discovers and verifies the
actual PCI → controller → Wi-Fi interface chain. Fulvio’s historical interface
was en0; add `WIFI_IFACE=en0` only if that is still the verified interface on
his system. Neither that name nor CNVW determines the DMA route.

The original r13 archive remains unchanged. Its `START_COMMANDS.txt` contains an
obsolete AX201/CNVW conventional-only sentence; **the corrected commands below
supersede that sentence**. Keep the original runner and all its safety checks.

Fulvio’s main command for his **mapped** configuration:

```sh
TOOLS="$HOME/Downloads/AirportItlwm-Tahoe-Benchmark-Tools-r13"
KEXT="$HOME/Downloads/AirportItlwm-Tahoe-2.0.0/AirportItlwm.kext"
BENCHMARK_PROFILE=kgp DEVICE_LABEL=AX201 CANDIDATE_LABEL=AX201-2.0.0 EXPECTED_MODE=mapped \
  /bin/bash "$TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" "$KEXT"
```

Separate command for an already configured **conventional** comparison:

```sh
BENCHMARK_PROFILE=kgp DEVICE_LABEL=AX201 CANDIDATE_LABEL=AX201-2.0.0 EXPECTED_MODE=conventional \
  /bin/bash "$TOOLS/AirportItlwm_3x20_PORTABLE_IP_PROMPT.sh" "$KEXT"
```

`EXPECTED_MODE=mapped` requires actual driver Mode 2; `conventional` requires
Mode 1. **It is an assertion, not a mode switch.** A mismatch must stop the run;
do not change the expectation just to bypass a rejection. AppleVTD being active
is a separate observation. Keep UUID/hash, device/interface, DMA and health guards.
For these standard VHT comparisons, verify **1000Base-T** on the wired peer;
HE/HE+BF comparisons use **2500Base-T** and the matching runner, not 5/10 GbE.
The server IP is prompted, unless `IPERF_SERVER` is explicitly supplied.

## Matrices and timing

Every variant is **3 rounds  × 20 = 60 iperf3 tests**, followed by one separate
online `networkQuality -I <Wi-Fi-interface> -s -c` test. Each round starts with
the same four TCP cases: TX P1, RX P1, TX P4, RX P4, each 8 s. UDP cases are 5 s,
1200-byte payloads; tests are separated by 2 s.

| Runner suffix | UDP TX targets per round (Mbit/s) | UDP RX targets per round (Mbit/s) |
|---|---|---|
| Standard |100,250,400,500,600,700,800,900|100,250,400,500,600,700,800,900|
| UDP1200 |300,500,600,600,600,600,600,600|300,500,700,900,1000,1100,1200,1200|
| UDP1300 |same TX600-limited list|300,500,700,900,1000,1100,1200,1300|
| UDP1600 |same TX600-limited list|300,500,700,900,1100,1300,1500,1600|
| UDP1700 |same TX600-limited list|300,500,700,900,1100,1300,1500,1700|

UDP1700 is a stress test, not a promise of loss-free 1700 Mbit/s. UDP1200 was a
provisional investigation ceiling, not a fix. Matrices/protocol identifiers
stay separate; do not pool standard and stress runs or cherry-pick best rounds.

## Interpret and return results

TX means the tested Wi-Fi client sends to the wired peer; RX is reverse.
Use receiver throughput for the main tables. Keep offered UDP target, actual
sender rate, actual receiver rate and reported loss separate. A CPU-limited
sender that sends 600 at a 900 target is not a 900-Mbit/s transmission test.
TCP retransmits are sender TCP counters; firmware/MAC retries and BA counters
are different. CPU percentages and intervals help identify sender ceilings
and stalls, but do not alone localize a driver/RF/AP cause. PHY/MCS reporting
is firmware feedback, not payload goodput or every-packet measurement.

The default output is under `~/Desktop/AirportItlwm/TESTS/` unless
`AIRPORTITLWM_TEST_DIR` overrides it. The runner prints its complete ZIP.
Return that **entire ZIP**, including RUN_STATUS, full JSON/intervals/stderr,
summary, INITIAL/round/FINAL identities and health, protocol/runner/resources,
online output, original checksum manifest, and any FAILURE captures. On a
failure, preserve partial data and guard errors; never edit them to claim 60.
Include notes/screenshot for PHY/width, bootargs and actual wired media.
Review whole-machine registry/boot data for privacy before public posting;
the release's public results asset includes audited iperf JSON and selected
identity/mode evidence, not unreviewed full registries or private handovers.

A complete run is not automatically loss-free or a long-duration stability
qualification. [2.0.0 results](../results/2.0.0.md) retain remaining losses and
retransmits. [Tested Bluetooth configuration](../../README.md#tested-bluetooth-configuration)
is separate from the Wi-Fi driver.
