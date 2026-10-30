# Shared runner r10 — corrected P8R1 / private AX ZC30

The BE200-P8 label now pins the corrected P8R1 bytes; it cannot accept the older failed P8 UUID/hash. Original P8/r9 remains frozen under reference/r9 and the original evidence. This is KGP's explicit one-time directory/profile replacement authorization. P7/r8/r7 profiles/references and measurement protocol remain unchanged. All names/paths/modes stay independently checked and parameterized.

# Shared 3×14 runner — r9 private P8/ZC29 profile extension

The r8 P7 baseline is complete and preserved unchanged, including all profile resources under `reference/r8/`. r8 SHA256 `ad488030e076fc825538bc310bc3b62374ee08340659b34cf150d43acd26d398`. r9 adds the private BE200-P8 identity and ZC29 marker/schema routing only; it does not alter the traffic matrix, evaluation, ordering, pauses or online step. The frozen P7 profile remains byte-equivalent as JSON data. Optional P8 counters are decoded only for that registry candidate; P7 reserved fields stay reserved.

Use the current commands in `START_COMMANDS.txt` after manual P8 installation/reboot. No hardware outcome is established by these offline tests. The following r8 documentation is retained for historical invocation details; references to a future first P7 baseline are superseded by the saved completed P7/r8 campaign.

# Shared 3×14 runner — r8 BE200 profile extension

This is the maintained common runner, not a BE200-only fork. The published r7 reference is preserved unchanged under `reference/`, SHA256 `be9c07168daf7b8ffd6f6c71ba4f62117d1e28334c4a64a9a063b2a115cc19ed`. Existing Beta releases, setup payloads and their embedded r7 are not repacked or edited. Do not call a new r8 campaign byte-identical to a historical r7 campaign.

## First BE200 baseline — KGP controls execution

Use the already installed Alpha 1/P7. Do not change/rebuild the driver for this baseline. Its profile pins loaded UUID `3610B51E-B7CD-3822-94BD-61EA7C138D99`, executable SHA256 `5bfe3548f2aaf10d8d3b34bc979e7c1f8454dd2e32067d42adce45aefb99a880`, Info.plist identity and its own marker. A BE200 label cannot impersonate ZC28. Expected mode is checked, never selected by the runner.

On the SERVER Mac, manually turn Wi-Fi off and connect the wired Ethernet adapter. Use the existing iperf3. Copy `AirportItlwm_IPERF3_WIRED_SERVER.sh` to the peer and run it, or paste the complete equivalent command in `START_COMMANDS.txt`. The helper displays hardware ports, prompts for the actual wired BSD interface, verifies Wi-Fi is off and the wired link/IPv4 are available, then listens with `iperf3 -s -B <wiredIPv4> -p 5201`. It never changes settings. Ctrl-C stops it. No current server IP or interface is assumed.

On the HACKINTOSH, with P7 connected and IPv4 configured:

```sh
IPERF_SERVER= TEST_GAP_SECONDS=2 BENCHMARK_PROFILE=be200 CANDIDATE_LABEL=BE200-ALPHA1 DEVICE_LABEL=BE200 WIFI_IFACE= IOREG_WIFI_SERVICE= CANDIDATE_KEXT="$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/AirportItlwm.kext" /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" BE200-ALPHA1 mapped
```

This forces the normal current-server-IPv4 prompt. `mapped` requires actual driver Mode 2 AND active AppleVTD. KGP's last observed en2/ARPT names are not baked into this command: actual PCI/controller/infrastructure binding is discovered. Optional `WIFI_IFACE=en2 IOREG_WIFI_SERVICE=ARPT` overrides are checked against that binding; other names, including en0/CNVW or en17/WIFI, work when actually bound. The BE200 profile additionally checks the driver's config-space identity `8086:272B`. DEVICE_LABEL is descriptive text, never evidence of hardware identity.

## Unchanged measurement protocol

`AIRPORTITLWM_3X14_NO_PROFILER_V1`: three rounds, each TCP TX P1, RX P1, TX P4, RX P4 (8 seconds each), then UDP TX/RX at 100/250/400/500/600 Mbps (5 seconds each, 1200-byte payload), with the original 2-second gaps. The final sequential `networkQuality -I <verified Wi-Fi> -s -c` and preceding 3-second pause are unchanged. run_test, all actual traffic commands, the case loop, online evaluator, original client JSON/individual intervals, server-output extraction and receiver-goodput/loss/retransmission evaluation are unchanged from r7. The optional original nondefault gap setting remains supported and is recorded as non-comparable with the default pause protocol.

All iperf3 traffic uses the original `-B <verifiedWiFiIPv4>%<verifiedBSD>` binding. The interface-scoped server route and unchanged Wi-Fi address are checked at the original points; networkQuality uses `-I` and its reported interface is checked. Ethernet is not a fallback. Keep other traffic/scans/profiler queries out of the measurement. Do not use system_profiler as preparation or perform automatic warm-up/recovery to hide failures.

## Guard and observer changes (not traffic changes)

One passive full IOService tree is captured BEFORE tests for device discovery. At the same eight original boundaries, retain the raw named-provider capture, select the pinned provider/controller/interface, verify loaded UUID and unchanged candidate bytes, then apply the original profile-specific health checks. There is no extra packet capture, per-packet logging, live polling or 1-Hz registry sampling. These added boundary checks have an observer cost and are recorded separately from the unchanged traffic protocol; they do not prove r7/r8 runtime equivalence on hardware.

The BE200 profile checks BOTH inherited KGP_ZC19 19/1 and BZ 200/1 schemas, agreeing DMA mode/generation/reset lifetime, current fault/error/admission/RUN/MSI-X state, unchanged campaign epochs, non-regressing counters and FINAL publication greater than INITIAL. Equal adjacent publication counters remain legitimate. Optional connection history 201/1 is decoded when present; absence is explicitly UNAVAILABLE_NOT_ZERO and overwritten history is counted. Missing required health, wrong identity/binding/mode/route or a changed epoch stops the run with originals archived. No fault/mode checks are disabled.

External/laobamac still does NOT receive fabricated KGP mode/health certification: declared AppleVTD configurations are recorded, actual external DMA mode remains unverified, numerical KGP telemetry unavailable. ZC8's opaque legacy telemetry and optional conventional TX diagnostic rule remain intact. A device's retail name does not assign its DMA mode.

## Existing calls and parameters

All old ZC8/ZC13/ZC18/ZC19–ZC28 calls, mapped/conventional argument or EXPECTED_MODE forms, custom absolute candidate paths and CANDIDATE_LABEL forms remain supported. LAOBAMAC 44/47/53 mapped/conventional, old LAOBAMAC mapped/conventional defaults and advanced explicit external profiles remain supported. Examples:

```sh
DEVICE_LABEL=AX210 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28 mapped
DEVICE_LABEL=AX211 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC8 conventional
DEVICE_LABEL=AX201 EXPECTED_MODE=conventional CANDIDATE_LABEL=ZC19 /bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" "/absolute/path/AirportItlwm.kext"
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 44 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 47 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC mapped
```

These are syntax examples, not hardware-mode assignments or qualification. Choose actual interface/provider overrides and expected DMA mode for the tested system. AX201/AX211 labels do not independently identify hardware or prove a mode. The shared runner verifies what the selected driver actually reports.

Path/output parameters remain AIRPORTITLWM_ROOT, AIRPORTITLWM_TEST_DIR, an absolute candidate argument, CANDIDATE_KEXT, BE200_KEXT, LAOBAMAC_KEXT, IPERF_SERVER, IPERF_PORT, WIFI_IFACE, IOREG_WIFI_SERVICE, DEVICE_LABEL and EXPECTED_MODE. Optional overrides must correspond uniquely to the selected candidate's actual device. Nothing selects EFI or changes the driver's route.

## Results and provenance

Defaults: `~/Desktop/AirportItlwm/TESTS/<candidate>_<device>_3X14_<mode>_<timestamp>_<random>/` and the sibling `.zip`. The exact path is printed at completion. Keep incomplete/failed runs. Archive original client JSON and all intervals, separate server outputs or explicit missing-output notices, stderr, actual command files, full executed runner copy/hash, runner revision, effective settings, helper/profile copies and SHA256, selected/raw boundary records, binding and candidate identities, online raw capacity/directional RPM/latency JSON, status and complete SHA256SUMS with ZIP CRC checks. BE200 adds offline-derived BE200_CAMPAIGN_DIAGNOSTICS.json; it is not continuous timing evidence. ONLINE PASS means successful execution, not acceptable capacity/responsiveness.

No driver/source/firmware change or new kext build belongs to this runner update. The next physical step is KGP's local BE200 baseline before further performance changes. Existing AX210/ZC8–later RX contrasts and BE200 variability remain distinct observations; any common initiating mechanism is a hypothesis, not established by these tools or host tests.

Working revision: `shared-2026-10-30-r8-be200`. `--version` prints it and the actual executed script SHA256 without starting traffic. Hashes and local source commit are also in RUNNER_IDENTITY.json delivered beside the working runner; campaign copies/hashes remain authoritative.

Current working revision: `shared-2026-10-30-r9-p8`. Hardware comparison pending; do not call r9 byte-identical to r8.

Candidate directories are permanent siblings: P7 stays `BE200-EXPERIMENTAL`, P8 uses `BE200-EXPERIMENTAL-P8`; later deliveries use P9 etc. Never overwrite old candidates. The P8 shortcut defaults to its own directory; P7 retains its original default.
