PRIVATE BE200 GL92 P2 — FIRST HARDWARE TEST (not a performance qualification)

STATUS: P2 experimental; not installed/loaded or physically tested by Codex.
P1 physically failed firmware-context before a usable Wi-Fi interface. Its
original candidate and captured evidence are retained separately. P2 corrects
an invalid pre-ROM IODMACommand synchronization option, not a proved Wi-Fi fix.
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
3. Inspect the normal Wi-Fi UI: note whether networks appear, then manually
   join the chosen WPA2 network. Record any prior traffic, failure or delay.
   Do not include passwords or key material in notes. Capture scan/join result:
   /bin/bash "$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/COLLECT_BE200.sh" joined
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
- PNVM/NVM timeout/status: exact SKU/payload/API or command/notification delivery.
- Scan version/channels/completion plus RX counters: scan request vs result path.
- MLD step/error and submitted-key count: auth/link/station/key transaction;
  async key submission is not firmware acceptance proof.
- IPv4/ARP absent despite association: control-port/RX/TX/data/keying not yet proven.
- TX publications vs completions/RX MPDUs locate the next boundary, not peer receipt.

If the device fails, retain output and rollback. Do not bypass admission, bounds,
mode, epoch or DMA checks. Do not repeat boot cycles or broad benchmarks merely
to hide a failure. No autonomous testing was performed in preparation.
