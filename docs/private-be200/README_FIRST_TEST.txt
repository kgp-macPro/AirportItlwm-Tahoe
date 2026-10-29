PRIVATE BE200 P6 — link handoff correction; HARDWARE UNTESTED

P5 first reached scan/auth/association/RSN but en2 had no IPv4.
After KGP manually toggled OFF/ON, SAME P5 acquired a DHCP ACK/IPv4; no P6 hardware proof. configd saw Link INACTIVE.
P6 corrects frontend event ordering; it does not assert working DHCP/data yet.
See BUILD_IDENTITY.txt and P6_LINK_HANDOFF.md. Preserve P5 and all bad outcomes.

1. KGP only: back up the currently booted complete AirportItlwm.kext and EFI.
   Replace that one existing bundle with this complete AirportItlwm.kext (no merge),
   retain the same established legacy environment/boot configuration; reboot.
   No automatic installation/loading/settings changes are performed by our tools.
2. Check loaded UUID against BUILD_IDENTITY; do not identify loaded source by
   Desktop files. Run COLLECT_BE200.sh for boot evidence before changing anything.
3. If Wi-Fi interface is present, start the ONE-attempt collector BEFORE a manual
   connection attempt (if already associated, record that; no arbitrary forget/renew):

   sudo /usr/bin/env BE200_OUTPUT_PARENT="$HOME/Desktop/AirportItlwm/EVIDENCE/BE200_JOIN" WIFI_IFACE=en2 IOREG_WIFI_SERVICE=ARPT BE200_PASSIVE_CAPTURE=1 /bin/bash "$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/COLLECT_BE200_JOIN.sh"

   sudo is only for passive BPF/log access; the collector never changes network
   settings, renews DHCP, joins, generates traffic or installs anything. Capture
   is limited to en2 ARP/DHCP,128packets,768bytes,at most the120second prompt. It
   records tcpdump drops/errors and en0 route context. Without sudo, omit passive
   capture but preserve any permission error; absence of capture is not no packets.
   Make ONE manual join in Settings if needed; wait for outcome then press Return.
   Return the exact printed P6_JOIN...zip, including failures. No password in notes.
   en2/ARPT above are KGP's VERIFIED P5 binding, not Kext assumptions. On another
   machine inspect the IOService tree and use its actual primary infrastructure
   BSD name and PCI node. Both overrides are required for this targeted collector,
   checked against 8086:272b/candidate marker -> AirportItlwm -> primary interface;
   no Ethernet/AWDL fallback. Each snapshot records PCI/driver/interface paths and
   IDs in BEFORE/AFTER.binding.json. Full automatic discovery is not a prerequisite
   for this frontend fix. Read the binding errors on any abort; do not bypass.
4. Judge separately: scan/auth/RSN, SystemConfiguration Link Active, DHCP exchange,
   IPv4/netmask/router. If no lease, stop; do not diagnose from Connected alone.
   No new broad throughput campaign. IPv6/linklocal is another observation.
5. Only after a valid en2 address/router, KGP may manually test gateway and a wired
   peer with traffic explicitly bound to en2. Ethernet en0 remains separate: a
   browser/networkQuality/default-route success does not prove BE200 Internet.
   Report the IPv4/gateway/interface and actual packet evidence first so the next
   minimum useful bidirectional test can be selected. No AWDL/MLO/6GHz qualification.

Rollback: KGP restores the previously backed-up entire P5 bundle at the same
existing EFI location and reboots. Do not merge bundles or change framework/
patch/boot settings for this comparison. All earlier candidates remain archived.
