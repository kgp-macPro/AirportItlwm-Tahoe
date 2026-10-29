#!/bin/bash
# Boundary-only private collector. No traffic, configuration, live loading or polling.
set -u
umask 077
STAGE="${1:-manual}"
case "$STAGE" in *[!A-Za-z0-9_-]*|'') echo 'Use a simple stage name.' >&2; exit 2;; esac
ROOT="$(cd "$(dirname "$0")" && pwd)"
PARENT="$HOME/Desktop/AirportItlwm/EVIDENCE/BE200_FIRST_TEST"
mkdir -p "$PARENT" || exit 1
OUT="$(mktemp -d "$PARENT/${STAGE}_$(date +%Y%m%d_%H%M%S)_XXXXXX")" || exit 1
(date; date -u; /usr/bin/sw_vers; /usr/bin/uname -a; /usr/sbin/sysctl kern.boottime) > "$OUT/SYSTEM.txt" 2>&1
/usr/bin/kmutil showloaded -v > "$OUT/LOADED.txt" 2>&1
/usr/sbin/networksetup -listallhardwareports > "$OUT/HARDWARE_PORTS.txt" 2>&1
/sbin/ifconfig -a > "$OUT/IFCONFIG.txt" 2>&1
/usr/sbin/ioreg -p IOService -w0 -l > "$OUT/BINDING_TREE.txt" 2>&1
if [ -n "${IOREG_WIFI_SERVICE:-}" ]; then
    /usr/sbin/ioreg -a -r -n "$IOREG_WIFI_SERVICE" > "$OUT/IOREG.plist" 2> "$OUT/IOREG.err"
else
    /usr/sbin/ioreg -a -r -c IOPCIDevice > "$OUT/IOREG.plist" 2> "$OUT/IOREG.err"
fi
/usr/sbin/ioreg -r -c AppleVTD -l > "$OUT/APPLEVTD.txt" 2>&1
/usr/bin/log show --last 15m --style compact --predicate 'process == "kernel" AND (eventMessage CONTAINS[c] "itlwm" OR eventMessage CONTAINS[c] "BE200")' > "$OUT/KERNEL_15MIN.txt" 2>&1
if [ -n "${WIFI_IFACE:-}" ]; then
    /usr/sbin/ipconfig getpacket "$WIFI_IFACE" > "$OUT/DHCP.txt" 2>&1
    /usr/sbin/ipconfig getoption "$WIFI_IFACE" router > "$OUT/ROUTER.txt" 2>&1
fi
for f in BUILD_IDENTITY.txt README_FIRST_TEST.txt INSTALL_AND_ROLLBACK.txt COLLECT_BE200.sh decode_be200.py; do
    if [ -f "$ROOT/$f" ]; then cp -p "$ROOT/$f" "$OUT/$f"; fi
done
if [ -f "$ROOT/AirportItlwm.kext/Contents/MacOS/AirportItlwm" ]; then
    /usr/bin/shasum -a 256 "$ROOT/AirportItlwm.kext/Contents/MacOS/AirportItlwm" > "$OUT/DESKTOP_EXECUTABLE_SHA256.txt"
fi
if command -v python3 >/dev/null 2>&1 && [ -f "$ROOT/decode_be200.py" ]; then
    python3 "$ROOT/decode_be200.py" "$OUT/IOREG.plist" > "$OUT/DECODED.json" 2> "$OUT/DECODE.err"
fi
printf '%s\n' 'Add untouched terminal output and notes separately. A Desktop hash is not loaded identity.' > "$OUT/NOTES_REQUIRED.txt"
(cd "$OUT" && find . -type f ! -name SHA256SUMS.txt -exec /usr/bin/shasum -a 256 {} \;) > "$OUT/SHA256SUMS.txt"
(cd "$PARENT" && /usr/bin/zip -q -r "$OUT.zip" "$(basename "$OUT")") || exit 1
printf 'Saved %s\n' "$OUT.zip"
/usr/bin/shasum -a 256 "$OUT.zip"
