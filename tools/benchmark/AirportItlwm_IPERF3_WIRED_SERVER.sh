#!/bin/bash
# Read-only interface checks, then a user-started wired iperf3 server.
set -euo pipefail
if [[ "${1:-}" == --help ]]; then
  echo 'Run on the wired peer. Requires iperf3; prompts for wired BSD interface.'
  echo 'Optional WIRED_IFACE and IPERF_PORT. Wi-Fi must already be OFF manually.'
  echo 'No settings are changed. Ctrl-C stops the server.'
  exit 0
fi
command -v iperf3 >/dev/null || { echo 'STOP: install/use the existing iperf3 first'; exit 1; }
port="${IPERF_PORT:-5201}"
[[ "$port" =~ ^[0-9]+$ ]] && (( 10#$port>=1 && 10#$port<=65535 )) || { echo 'STOP: invalid IPERF_PORT'; exit 1; }
ports="$(networksetup -listallhardwareports)"
printf '%s\n' "$ports"
iface="${WIRED_IFACE:-}"
if [[ -z "$iface" ]]; then
  printf '\nBSD interface of the WIRED Ethernet adapter (for example en5): ' > /dev/tty
  IFS= read -r iface < /dev/tty
fi
[[ "$iface" =~ ^en[0-9]+$ ]] || { echo 'STOP: invalid wired BSD interface'; exit 1; }
wifi_devices="$(printf '%s\n' "$ports" | awk '/^Hardware Port: (Wi-Fi|AirPort)/ {wifi=1;next} wifi && /^Device:/ {print $2;wifi=0}')"
for wifi in $wifi_devices; do
  [[ "$iface" != "$wifi" ]] || { echo 'STOP: selected interface is Wi-Fi, not the wired peer'; exit 1; }
  power="$(networksetup -getairportpower "$wifi")"
  printf '%s\n' "$power"
  printf '%s\n' "$power" | grep -Eq ':[[:space:]]*Off[[:space:]]*$' || {
    echo 'STOP: turn the SERVER Mac Wi-Fi OFF manually, then rerun'; exit 1;
  }
done
ip="$(ipconfig getifaddr "$iface")" || { echo 'STOP: selected wired adapter has no IPv4'; exit 1; }
[[ "$ip" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'STOP: invalid wired IPv4'; exit 1; }
state="$(ifconfig "$iface")"
printf '%s\n' "$state" | grep -Eq 'status:[[:space:]]*active' || { echo 'STOP: wired link is not active'; exit 1; }
printf '\nWired server: %s / %s:%s\nEnter this IPv4 when the CLIENT runner prompts.\n' "$iface" "$ip" "$port"
printf 'COMMAND=iperf3 -s -B %q -p %q\n' "$ip" "$port"
exec iperf3 -s -B "$ip" -p "$port"
