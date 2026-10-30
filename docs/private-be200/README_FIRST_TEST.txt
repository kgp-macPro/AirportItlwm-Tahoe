PRIVATE P8 / AX ZC29 — CORRECTIVE TEST CANDIDATE, NOT HARDWARE VALIDATED

Use the UUID/SHA256/source commit in the adjacent BUILD_IDENTITY.txt. The same
binary uses BE200 P8 identification on BE200 and private ZC29 identification on
other admitted IWX devices. It is not the unchanged public Alpha1/P7 or ZC28.

Required environment remains the restored Ventura legacy wireless frameworks
and matching Retro-OCLP-CustoMac root patches / supported boot configuration.
Do not remove patches, change mapper settings or install another framework as
part of this comparison. DEVICE_LABEL and EXPECTED_MODE do not select hardware
or DMA. Existing admission remains authoritative; no AX201 retail-only rule.

INSTALL / ROLLBACK (KGP MANUALLY)
1. Preserve the complete working P7 bundle and the active boot configuration.
   The delivery also preserves the previous Desktop bundle/notes in an archive.
2. Replace the WHOLE AirportItlwm.kext at the SAME existing EFI location with
   this complete bundle; do not merge Contents or leave duplicate enabled
   AirportItlwm/itlwm entries. This directory is preparation, not installation.
3. Reboot manually. Check the loaded UUID against BUILD_IDENTITY, not only the
   Desktop executable. If boot/scan/join/DHCP fails, preserve the failure data.
4. Rollback: restore the complete prior P7 bundle in that same existing location
   and reboot manually with the previous unchanged boot/patch configuration.
   P7 UUID3610B51E-B7CD-3822-94BD-61EA7C138D99,
   executableSHA2565bfe3548f2aaf10d8d3b34bc979e7c1f8454dd2e32067d42adce45aefb99a880.

FIRST USE / THROUGHPUT (KGP MANUALLY)
- Record first-boot autojoin, IPv4/router and prior traffic. No undocumented
  preconditioning, profiler query or parallel scan during traffic.
- Start the existing wired iperf3 peer with Wi-Fi manually OFF:
      iperf3 -s -p 5201
  The common runner prompts for its CURRENT IPv4. No hard-coded peer address.
- Use the exact P8 start command supplied in START_COMMANDS.txt beside the
  bundle. It uses the maintained common runner with a new pinned P8 profile;
  the original P7/r8 and frozen beta/r7 references are retained unchanged.
  Actual PCI/controller/infrastructure-interface/loaded UUID/DMA/route checks
  remain enabled. en2/ARPT are KGP's observed binding, not general assumptions.
- Run the original3x14 matrix/order/durations/pauses/online step once. Preserve
  the complete raw result ZIP, every interval/stream, stderr and server output,
  including slow/failed attempts. Existing P7/r8 is the comparison baseline.
- Optional separate recovery regression: record a manual OFF/ON and a manual
  Sleep/Wake after the first workload, preserving success and failure separately.
  These are user-controlled steps, not actions performed by these tools.

FOCUSED FAILURE COLLECTION
COLLECT_BE200_JOIN.sh remains manual/passive. For a connection failure, supply
the verified actual names as WIFI_IFACE and IOREG_WIFI_SERVICE. The resolver
checks the P8 PCI->controller->infrastructure interface association; it never
substitutes Ethernet/AWDL. No automatic scan, reconnect or configuration change.
Enable passive DHCP/ARP capture only for a specific connection/IP failure, not
parallel to the normal throughput campaign. Original P7 collection tools remain
archived with P7; this copy expects P8.

INTERPRETATION
P8 fixes a source-supported QoS/BA-offload integration mismatch, RX credit
callback ordering/lifetime, aggregate completion ownership and RX bounds.
The RX scheduling benefit and all throughput targets remain hardware-untested.
New bounded counters separate some RX drops and TX queue use; counters are not
an over-air trace or proof of peer receipt. See P8_CORRECTIONS.md for scope,
known counterexamples, pinned references and active AX investigations.
