#!/bin/bash
# Manual ONE-attempt collector. Never scans/joins/changes settings/starts traffic.
set -u
umask 077
ROOT="$(cd "$(dirname "$0")" && pwd)"
PARENT="${BE200_OUTPUT_PARENT:-$HOME/Desktop/AirportItlwm/EVIDENCE/BE200_JOIN}"
mkdir -p "$PARENT" || exit 1
OUT="$(mktemp -d "$PARENT/P7_JOIN_$(date +%Y%m%d_%H%M%S)_XXXXXX")" || exit 1
LOG_PID=''
PCAP_PID=''
if [ -z "${WIFI_IFACE:-}" ] || [ -z "${IOREG_WIFI_SERVICE:-}" ]; then
  printf 'STOP: Set WIFI_IFACE and IOREG_WIFI_SERVICE from the actual binding; no default interface or PCI node is selected.\n' >&2
  exit 2
fi
cleanup() { if [ -n "$PCAP_PID" ]; then kill -INT "$PCAP_PID" 2>/dev/null || true; wait "$PCAP_PID" 2>/dev/null; printf "%s\n" "$?" > "$OUT/TCPDUMP.exit"; PCAP_PID=''; fi; if [ -n "$LOG_PID" ]; then kill "$LOG_PID" 2>/dev/null || true; wait "$LOG_PID" 2>/dev/null || true; LOG_PID=''; fi; }
trap cleanup EXIT
trap 'printf "Interrupted; preserving capture.\n" >&2' INT TERM
snapshot() {
  local stage="$1"
  local previous=''
  if [ "$stage" = AFTER ]; then previous="$OUT/BEFORE.binding.json"; fi
  /bin/date '+%Y-%m-%dT%H:%M:%S%z' > "$OUT/$stage.time.txt"
  /usr/sbin/ioreg -a -l -r -n "$IOREG_WIFI_SERVICE" > "$OUT/$stage.plist" 2> "$OUT/$stage.ioreg.err"
  python3 "$ROOT/resolve_be200_interface.py" "$OUT/$stage.plist" "$OUT/$stage.binding.json" "$OUT/$stage.selected.plist" --iface "$WIFI_IFACE" --provider "$IOREG_WIFI_SERVICE" --previous "$previous" > "$OUT/$stage.resolved-iface.txt" 2> "$OUT/$stage.binding.err" || return 1
  /sbin/ifconfig "$WIFI_IFACE" > "$OUT/$stage.ifconfig.txt" 2>&1
  /sbin/ifconfig -a > "$OUT/$stage.context-ifconfig-all.txt" 2>&1
  /usr/sbin/scutil > "$OUT/$stage.dynamic-store.txt" 2>&1 <<EOF
show State:/Network/Global/IPv4
show State:/Network/Interface/$WIFI_IFACE/Link
show State:/Network/Interface/$WIFI_IFACE/IPv4
show State:/Network/Interface/$WIFI_IFACE/IPv6
show State:/Network/Interface/$WIFI_IFACE/AirPort
quit
EOF
  /usr/sbin/networksetup -getairportnetwork "${WIFI_IFACE}" > "$OUT/$stage.network.txt" 2>&1
  if command -v python3 >/dev/null 2>&1; then python3 "$ROOT/decode_be200.py" "$OUT/$stage.plist" > "$OUT/$stage.decoded.json" 2> "$OUT/$stage.decode.err"; fi
}
(/bin/date; /usr/sbin/sysctl kern.boottime kern.bootsessionuuid; /usr/bin/sw_vers) > "$OUT/SYSTEM.txt" 2>&1
/usr/bin/kmutil showloaded -v > "$OUT/LOADED.txt" 2>&1
/usr/sbin/networksetup -listallhardwareports > "$OUT/HARDWARE_PORTS.txt" 2>&1
/usr/sbin/networksetup -listnetworkserviceorder > "$OUT/SERVICE_ORDER.txt" 2>&1
/usr/bin/log show --last 20m --style compact --info --debug --predicate 'process == "configd" OR subsystem CONTAINS[c] "IPConfiguration"' > "$OUT/IP_BOOT_CONTEXT.log" 2> "$OUT/IP_BOOT_CONTEXT.err"
if ! command -v python3 >/dev/null 2>&1 || ! snapshot BEFORE; then
  printf 'ABORT: no verified unique candidate/provider/infrastructure interface; see BEFORE.binding.err. No capture started.\n' > "$OUT/COLLECTION_STATUS.txt"
  (cd "$OUT" && find . -type f ! -name SHA256SUMS.txt -exec /usr/bin/shasum -a 256 {} \;) > "$OUT/SHA256SUMS.txt"
  (cd "$PARENT" && /usr/bin/zip -q -r "$OUT.zip" "$(basename "$OUT")")
  printf 'STOP; preserve %s\n' "$OUT.zip" >&2; exit 2
fi
: > "$OUT/ATTEMPT.log"
: > "$OUT/ATTEMPT.err"
/usr/bin/log stream --style compact --level debug --predicate 'process == "airportd" OR process == "configd" OR subsystem CONTAINS[c] "IPConfiguration" OR subsystem CONTAINS[c] "eapol" OR (process == "kernel" AND (eventMessage CONTAINS[c] "iwx" OR eventMessage CONTAINS[c] "BE200" OR eventMessage CONTAINS[c] "itlwm"))' > "$OUT/ATTEMPT.log" 2> "$OUT/ATTEMPT.err" &
LOG_PID=$!
if [ "${BE200_PASSIVE_CAPTURE:-0}" = 1 ]; then
  printf 'interface=%s; passive only; snaplen=768; max_packets=128; input timeout=120s; filter=ARP/DHCP\n' "${WIFI_IFACE}" > "$OUT/PASSIVE_CAPTURE.txt"
  : > "$OUT/TCPDUMP.out"
  : > "$OUT/TCPDUMP.err"
  /usr/sbin/tcpdump -n -p -U -i "${WIFI_IFACE}" -s 768 -c 128 -w "$OUT/WIFI_DHCP_ARP.pcap" 'arp or (udp and (port 67 or port 68))' > "$OUT/TCPDUMP.out" 2> "$OUT/TCPDUMP.err" &
  PCAP_PID=$!
else
  printf 'NOT REQUESTED. No packet-level absence may be inferred. Enable only for user-run capture; BPF may require sudo.\n' > "$OUT/PASSIVE_CAPTURE.txt"
fi
printf 'Collector ready. Make ONE manual connection attempt in Wi-Fi Settings.
Do not delete services or forget networks for this capture.
Press Return after success/failure (within 120 seconds), or Ctrl-C to stop.
'
# Input timeout bounds streaming; it is not a request to repeat attempts.
if IFS= read -r -t 120 ANSWER; then RESULT='user-finished'; else RESULT='input-timeout-or-interrupted'; fi
printf '%s\n' "$RESULT" > "$OUT/COLLECTION_STATUS.txt"
cleanup
/bin/date > "$OUT/ATTEMPT_END_TIME.txt"
/bin/sleep 2 # Allow existing watchdog publication; no repeated registry polling.
if ! snapshot AFTER; then
  printf 'ABORT: final binding unavailable/ambiguous; no continuous-flow conclusion.\n' > "$OUT/COLLECTION_STATUS.txt"
fi
/usr/sbin/ipconfig getpacket "${WIFI_IFACE}" > "$OUT/DHCP.txt" 2>&1
for file in BUILD_IDENTITY.txt README_FIRST_TEST.txt COLLECT_BE200_JOIN.sh decode_be200.py resolve_be200_interface.py; do cp -p "$ROOT/$file" "$OUT/$file" || exit 1; done
if [ "$EUID" = 0 ]; then
  case "${SUDO_UID:-}:${SUDO_GID:-}" in *[!0-9:]*|:|:*|*:) ;; *) /usr/sbin/chown -R "${SUDO_UID}:${SUDO_GID}" "$OUT";; esac
fi
printf 'Record attempt time,UI result,network band/security and prior attempts separately. No passwords. A Desktop identity is not loaded identity. No traffic was generated by this collector.\n' > "$OUT/NOTES_REQUIRED.txt"
(cd "$OUT" && find . -type f ! -name SHA256SUMS.txt -exec /usr/bin/shasum -a 256 {} \;) > "$OUT/SHA256SUMS.txt"
(cd "$PARENT" && /usr/bin/zip -q -r "$OUT.zip" "$(basename "$OUT")") || exit 1
printf 'Return this unchanged ZIP: %s\n' "$OUT.zip"
if [ "$EUID" = 0 ]; then
  case "${SUDO_UID:-}:${SUDO_GID:-}" in *[!0-9:]*|:|:*|*:) ;; *) /usr/sbin/chown "${SUDO_UID}:${SUDO_GID}" "$OUT.zip";; esac
fi
/usr/bin/shasum -a 256 "$OUT.zip"
