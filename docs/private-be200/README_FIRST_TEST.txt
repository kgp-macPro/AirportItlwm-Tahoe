PRIVATE BE200 GL92 P5 — FIRST HARDWARE TEST (not a performance qualification)

STATUS: P5 experimental; not installed/loaded or physically tested by Codex.
P4 physically passed PNVM and published ARPT/AirportItlwm/en2. Wi-Fi could be
enabled and KGP rediscovered the network after forgetting it; join failed.
Saved P4 transport shows repeated fault3/EPROTO100,46positive stops,47ALIVEs,
46packet publications and zero TX retirements. Last MLD probe AUTH_READY is not
over-air authentication success and was not lifetime-stamped in P4.
P5 separates packet TX responses from host command responses before dispatch,
as required by pinned Linux's TX_CMD no-reclaim contract. Strict command ledger,
owned packet queue/window/finality protections remain. A bounded history records
control steps,pre-RUN TX status,auth/assoc status,first faults and lifetime stamps.
P4 did not preserve its rejected wire header; the initiating TX reply is a strong
inference,not captured proof. P5 must be tested; no association success is claimed.
All previous candidates/evidence remain preserved.
This remains an experimental first-infrastructure build,not a proved Wi-Fi fix.
Initial target: discrete Intel 8086:272B, GL-C0/FM-C0. BE201 is not admitted.
Single-link 2.4/5GHz infrastructure, legacy Ventura frontend on the established
Tahoe framework environment. No claim of 802.11be, EHT/HE, MLO, 6GHz, AWDL,
Continuity, sleep/wake reliability, high throughput or broader qualification.
Start with an existing WPA2/CCMP network. Do not begin with a 3x14 campaign.

1. Follow INSTALL_AND_ROLLBACK.txt. KGP alone installs and reboots. Retain a
   known-good boot path. Read BUILD_IDENTITY.txt and verify Desktop executable.
2. After reboot, run:
   /bin/bash "$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/COLLECT_BE200.sh" boot
   This invokes the boundary collector. It only reads local state and
   writes a small private evidence ZIP; it never starts traffic or changes EFI.
   Check loaded UUID in kmutil output against BUILD_IDENTITY.txt. A Desktop
   hash alone is not proof of the loaded executable. Stop on identity mismatch.
   In DECODED.json, inspect providers as well as snapshots: boot failures can
   retain stage/error/sync-region/raw-IOReturn on ARPT without a watchdog record.
   Do not delete/recreate the network service to work around attach failure.
   If no usable interface, stop here and return the boot ZIP; do not run traffic.
   If an actual Wi-Fi hardware port/interface exists but the network service is
   absent after its earlier deletion, KGP may add that service manually in System
   Settings. Service recreation cannot repair an attach failure.
3. Before ONE manual connection attempt, run:
   /bin/bash "$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/COLLECT_BE200_JOIN.sh"
   The collector saves BEFORE,streams focused logs,then asks you to join in the
   normal Wi-Fi UI. It never initiates a scan/join,changes settings or sends
   traffic. Press Return after the UI reports success/failure,within120seconds.
   It waits2seconds for existing watchdog publication,takes AFTER and prints
   ONE ZIP path under ~/Desktop/AirportItlwm/EVIDENCE/BE200_JOIN/.
   Keep unsuccessful attempts. Do not delete/recreate services or forget the
   network for this capture; record prior attempts/security/band without passwords.
   If no association,stop here and return that ZIP. Do not diagnose DHCP yet.
   Check connection_histories in decoded JSON: each event includes same-host
   uptime ns,generation/reset epoch. 'AUTH_READY' means setup returned,not over-air
   authentication. TX status1=success;0x82=noACK per pinned firmware API. Packet
   completion alone does not prove peer receipt. RX-auth/assoc status and subsequent
   MLD RUN steps distinguish progress. Key counters are submissions,not acceptance.
   Schema201/1 retains64events and reports overwritten history; absence or gaps
   remain unavailable evidence. No SSID/MAC/key/data bytes are in driver history.
4. Discover the actual Wi-Fi interface and binding (not inferred from card name):
   networksetup -listallhardwareports
   ioreg -p IOService -w0 | /usr/bin/grep -i -B 6 -A 6 AirportItlwm
   Set this shell variable to the observed interface, then read DHCP/router:
   WIFI_IFACE=en2
   CLIENT_IP="$(ipconfig getifaddr "$WIFI_IFACE")"
   ipconfig getpacket "$WIFI_IFACE"
   GATEWAY="$(ipconfig getoption "$WIFI_IFACE" router)"
   printf 'Wi-Fi=%s IPv4=%s Gateway=%s
' "$WIFI_IFACE" "$CLIENT_IP" "$GATEWAY"
   If IPv4/router is absent, stop traffic testing and collect the failure.
   Confirm the interface-scoped route; do not change routing automatically:
   route -n get -ifscope "$WIFI_IFACE" "$GATEWAY"
   ping -S "$CLIENT_IP" -c 4 "$GATEWAY"
   arp -an
5. On the already-wired peer (Wi-Fi off), start the existing iperf3 server:
   iperf3 -s
   On the BE200 client, prompt for its CURRENT IPv4 (do not assume .33/.161):
   read -r 'SERVER_IP?Wired iperf3 server IPv4: '
   route -n get -ifscope "$WIFI_IFACE" "$SERVER_IP"
   iperf3 -c "$SERVER_IP" -p 5201 -4 -B "$CLIENT_IP%$WIFI_IFACE" -t 4 -P 1 --get-server-output
   iperf3 -c "$SERVER_IP" -p 5201 -4 -B "$CLIENT_IP%$WIFI_IFACE" -R -t 4 -P 1 --get-server-output
   These are separate directional connectivity checks, not a throughput claim.
   Save complete console output, including failures, not just rates.
6. Collect after traffic with the same bounded collector, then retain all stage
   ZIPs plus output/short notes (card PCI identity, SSID band/security without
   password, actual interface, mapper state, wired peer, boot time, sequence).
   /bin/bash "$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/COLLECT_BE200.sh" after-traffic
   No new dependencies or continuous IORegistry sampling are required.

How the evidence distinguishes failures:
- Missing PCI match/stage: identity or framework/provider attach.
- Nonzero MSI-X setup / absent enabled capability: provider/SDK/interrupt setup.
- BOOT_SYNC_INDEX=8/REGION=complete/RESULT=0: boot regions synchronized; this
  alone proves neither firmware boot nor an interrupt. Failure index0..7 gives
  the region and raw IOReturn. LAST_FAILURE32 identifies sync/backing failure.
- ROM/IML/ALIVE stage/error: firmware loading, hardware identity or boot transport.
- Parsed ALIVE with poll hits but no filter/action: do not claim working IRQs.
- ItlwmPnvmName must be iwlwifi-gl-c0-fm-c0.pnvm. DescFound/InflateOk/HwMatch,
  FailStep/ReadError/NotifySeen/WaitError/NotifyStatus distinguish lookup, parse,
  expected completion and wait failure. NotifyStatus is opaque; valid only when
  NotifySeen=1. NotifySeen=1/WaitError=0 means the expected completion arrived,
  not independently proved RF calibration. Follow subsequent NVM/interface stages.
  FailStep is parser progress, not alone evidence of a failure.
- KGP_BE200_INIT_* are actual private lifetime IRQ/RX/ALIVE/PNVM counters; the inherited
  ItlwmBootIrq* fields do not observe every private path. ALIVE_SKU0/1/2 is the
  parsed firmware SKU,not a card label. Neither counter proves peer traffic.
- KGP_BE200_CONNECTION: bounded control history exported by existing watchdog
  outside gate. Raw packet/command rejection and first fault retain lifetime.
  Only rare pre-RUN TX/management transitions get timestamps; no bulk-data logging.
- PNVM/NVM timeout/status: exact SKU/payload/API or command/notification delivery.
- Scan version/channels/completion plus RX counters: scan request vs result path.
- MLD step/error and submitted-key count: auth/link/station/key transaction;
  async key submission is not firmware acceptance proof.
- IPv4/ARP absent despite association: control-port/RX/TX/data/keying not yet proven.
- TX publications vs completions/RX MPDUs locate the next boundary, not peer receipt.

If the device fails, retain output and rollback. Do not bypass admission, bounds,
mode, epoch or DMA checks. Do not repeat boot cycles or broad benchmarks merely
to hide a failure. No autonomous testing was performed in preparation.
