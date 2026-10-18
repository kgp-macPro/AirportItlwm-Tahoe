#!/bin/bash
# ZC19 derived from portable runner SHA256 00cf96aadddc0a130ef4c030bc80d4bea293bdf1e2e96a631c0fa1968956f15e
# Only schema19/1 guard added; no extra snapshot, traffic, timer or system_profiler.
# AirportItlwm — reusable 3x14 physical benchmark, complete iperf3 raw logs,
# and ONE final Internet-based Apple networkQuality test.
# macOS Tahoe / Bash 3.2 compatible. Does not modify EFI, driver or network settings.
#
# Usage:
#   /bin/bash AirportItlwm_3x14_PORTABLE.sh ZC18
#   /bin/bash AirportItlwm_3x14_PORTABLE.sh ZC8
#   /bin/bash AirportItlwm_3x14_PORTABLE.sh "/absolute/path/to/AirportItlwm.kext"
#
# Prompts for the wired peer's IPv4 unless IPERF_SERVER is explicitly provided.
# For another machine (e.g. Luiz's AX211) set WIFI_IFACE, DEVICE_LABEL,
# EXPECTED_MODE and, if needed, AIRPORTITLWM_ROOT or an absolute kext path.
#
# ZC8 uses its legacy marker/mode/opaque PC1 guard; ZC13+ retain the strict KGP_ZC10 guard.
# ZC8 KGP_TX_2I is mandatory in mapped mode, optional/recorded in conventional mode.
# Defaults reproduce the original 3x14 case matrix: TCP 8s P1/P4, UDP 5s
# 1200-byte 100/250/400/500/600M, each direction, 2s between tests.
set -Eeuo pipefail

usage() {
  cat <<'HELP'
Usage: /bin/bash AirportItlwm_3x14_PORTABLE.sh ZC8|ZC13|ZC18|/absolute/path/AirportItlwm.kext

When started, the script asks for the wired iperf3 server's CURRENT IPv4.
There is NO hard-coded or remembered server IP. Use dots, not commas.
To skip the interactive prompt, explicitly set IPERF_SERVER=192.168.178.33.

Optional environment variables (set for another host such as Luiz's AX211):
  WIFI_IFACE=en2                  Wi-Fi interface used by AirportItlwm
  DEVICE_LABEL=AX210             Name recorded in output/ZIP (e.g. AX211)
  EXPECTED_MODE=mapped           mapped or conventional
  IPERF_SERVER=<IPv4>            Optional non-interactive peer IPv4
  IPERF_PORT=5201                iperf3 port on the wired peer
  AIRPORTITLWM_ROOT=~/Desktop/AirportItlwm  Candidate directory root
  AIRPORTITLWM_TEST_DIR=...      Destination of originals and ZIPs
  CANDIDATE_LABEL=ZC8            For nonstandard absolute ZC8 kext paths
  IOREG_WIFI_SERVICE=ARPT        IORegistry node name (if different)
  TEST_GAP_SECONDS=2             Pause between original test cases

Candidate label ZC18 resolves to:
  $AIRPORTITLWM_ROOT/B1-ZC18-PERF/AirportItlwm.kext
An absolute kext path is also accepted for other folder layouts.
The loaded AirportItlwm x86_64 UUID MUST match the candidate on disk.
ZC8 uses verified legacy markers; KGP_TX_2I may be absent in conventional.
Later candidates require KGP_ZC10 10/2.

All 42 original cases and the final online networkQuality run are unchanged.
No system_profiler SPAirPortDataType or intrusive runtime sampling.
The wired iperf3 peer must be running `iperf3 -s` with its Wi-Fi OFF.
No automatic reconnect, reboot, EFI installation or network changes.
HELP
}
if [[ $# -ne 1 || "$1" == "--help" || "$1" == "-h" ]]; then
  usage
  if [[ $# -eq 1 && ( "$1" == "--help" || "$1" == "-h" ) ]]; then exit 0; fi
  exit 2
fi

ROOT="${AIRPORTITLWM_ROOT:-$HOME/Desktop/AirportItlwm}"
BASE="${AIRPORTITLWM_TEST_DIR:-$ROOT/TESTS}"
IFACE="${WIFI_IFACE:-en2}"
PORT="${IPERF_PORT:-5201}"
DEVICE="${DEVICE_LABEL:-AX210}"
MODE="${EXPECTED_MODE:-mapped}"
GAP_SECONDS="${TEST_GAP_SECONDS:-2}"
IOREG_SERVICE="${IOREG_WIFI_SERVICE:-ARPT}"

case "$MODE" in mapped|conventional) ;; *) echo 'STOP: EXPECTED_MODE must be mapped or conventional'; exit 2 ;; esac
[[ "$IFACE" =~ ^[a-zA-Z][a-zA-Z0-9]*$ ]] || { echo 'STOP: Invalid WIFI_IFACE'; exit 2; }
[[ "$IOREG_SERVICE" =~ ^[a-zA-Z0-9_,.-]+$ ]] || { echo 'STOP: Invalid IOREG_WIFI_SERVICE'; exit 2; }
[[ "$PORT" =~ ^[0-9]+$ ]] && (( 10#$PORT >= 1 && 10#$PORT <= 65535 )) || {
  echo 'STOP: IPERF_PORT must be an integer from 1 to 65535'; exit 2;
}
[[ "$GAP_SECONDS" =~ ^[0-9]+$ ]] || { echo 'STOP: Invalid TEST_GAP_SECONDS'; exit 2; }

# A server IP is mandatory for every run; never silently reuse the former
# 192.168.178.161, since the wired MacBook may now be 192.168.178.33.
valid_server_ipv4() {
  python3 - "$1" <<'PY_VALID_IP'
import ipaddress, sys
try:
    addr=ipaddress.IPv4Address(sys.argv[1])
    assert not (addr.is_multicast or addr.is_unspecified or addr.is_loopback)
    assert addr != ipaddress.IPv4Address('255.255.255.255')
except (ValueError, AssertionError):
    sys.exit(1)
PY_VALID_IP
}
if [[ -n "${IPERF_SERVER:-}" ]]; then
  SERVER="$IPERF_SERVER"
  if ! valid_server_ipv4 "$SERVER"; then
    echo "STOP: Invalid IPERF_SERVER IPv4: $SERVER (use dots, not commas)" >&2
    exit 2
  fi
else
  SERVER=''
  for attempt in 1 2 3; do
    if [[ -r /dev/tty ]]; then
      printf 'IPv4 des kabelgebundenen iperf3-Servers (z.B. 192.168.178.33): ' > /dev/tty
      IFS= read -r SERVER < /dev/tty || { echo 'STOP: IP input aborted' >&2; exit 2; }
    elif [[ -t 0 ]]; then
      printf 'IPv4 des kabelgebundenen iperf3-Servers (z.B. 192.168.178.33): '
      IFS= read -r SERVER || { echo 'STOP: IP input aborted' >&2; exit 2; }
    else
      echo 'STOP: No interactive terminal. Supply IPERF_SERVER=IPv4 explicitly.' >&2
      exit 2
    fi
    if valid_server_ipv4 "$SERVER"; then break; fi
    echo 'Ungültige IPv4-Adresse. Bitte Punkte statt Kommas verwenden.' >&2
    SERVER=''
  done
  if [[ -z "$SERVER" ]]; then
    echo 'STOP: No valid server IPv4 entered after three attempts' >&2
    exit 2
  fi
fi
printf 'iperf3 server: %s:%s | Wi-Fi: %s | device: %s | mode: %s\n' \
  "$SERVER" "$PORT" "$IFACE" "$DEVICE" "$MODE"

CANDIDATE_INPUT="$1"
if [[ "$CANDIDATE_INPUT" == */* ]]; then
  KEXT="${CANDIDATE_INPUT%/}"
  KEXT_PARENT="$(basename "$(dirname "$KEXT")")"
  if [[ "$KEXT_PARENT" =~ ^B1-(ZC[0-9]+)-PERF$ ]]; then
    LABEL="${BASH_REMATCH[1]}"
  else
    LABEL="$KEXT_PARENT"
  fi
else
  LABEL="$(printf '%s' "$CANDIDATE_INPUT" | tr '[:lower:]' '[:upper:]')"
  [[ "$LABEL" =~ ^ZC[0-9]+$ ]] || {
    echo "STOP: Use ZC<number> or an absolute AirportItlwm.kext path"; exit 2;
  }
  KEXT="$ROOT/B1-${LABEL}-PERF/AirportItlwm.kext"
fi
# For custom directory layouts, e.g. Luiz's frozen ZC8 stored elsewhere.
if [[ -n "${CANDIDATE_LABEL:-}" ]]; then
  LABEL="$(printf '%s' "$CANDIDATE_LABEL" | tr '[:lower:]' '[:upper:]')"
  [[ "$LABEL" =~ ^ZC[0-9]+$ ]] || { echo 'STOP: Invalid CANDIDATE_LABEL'; exit 2; }
fi
SAFE_LABEL="$(printf '%s' "$LABEL" | tr -c '[:alnum:]_-' '_')"
SAFE_DEVICE="$(printf '%s' "$DEVICE" | tr -c '[:alnum:]_-' '_')"
EXECUTABLE="$KEXT/Contents/MacOS/AirportItlwm"
[[ -f "$EXECUTABLE" ]] || { echo "STOP: Missing candidate executable: $EXECUTABLE"; exit 1; }

for program in python3 iperf3 xcrun ioreg kmutil route ipconfig networkQuality shasum; do
  command -v "$program" >/dev/null 2>&1 || { echo "STOP: Missing command: $program"; exit 1; }
done
[[ -x /usr/bin/ditto ]] || { echo 'STOP: macOS /usr/bin/ditto unavailable'; exit 1; }

# The archive UUID is verified AGAINST the loaded macOS driver, not the bundle version.
UUID_OUTPUT="$(xcrun dwarfdump --uuid "$EXECUTABLE" 2>&1)" || {
  echo 'STOP: Cannot inspect candidate Mach-O UUID'; exit 1;
}
UUID="$(printf '%s\n' "$UUID_OUTPUT" | awk '/^UUID: / && /\(x86_64\)/ {print $2; exit}')"
[[ -n "$UUID" ]] || { echo 'STOP: No x86_64 UUID in candidate'; exit 1; }

mkdir -p "$BASE"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="$(mktemp -d "$BASE/${SAFE_LABEL}_${SAFE_DEVICE}_3X14_${MODE}_${STAMP}_XXXXXX")"
ARCHIVE="$OUT.zip"
completed=0
stage="PREPARATION"
online_status="NOT_RUN"
finished_ok=0
INITIAL_HEALTH=""

finish() {
  local rc=$?
  trap - EXIT
  set +e
  {
    if (( rc == 0 && completed == 42 && finished_ok == 1 )); then
      echo 'STATUS=COMPLETE_42_AND_ONLINE'
    else
      echo 'STATUS=INCOMPLETE_OR_ONLINE_FAILURE'
    fi
    printf 'EXIT_CODE=%s\nCOMPLETED_IPERF_TESTS=%s\nEXPECTED_IPERF_TESTS=42\nONLINE_STATUS=%s\nLAST_STAGE=%s\n' \
      "$rc" "$completed" "$online_status" "$stage"
    date '+FINISHED_LOCAL=%Y-%m-%dT%H:%M:%S%z'
    date -u '+FINISHED_UTC=%Y-%m-%dT%H:%M:%SZ'
  } > "$OUT/RUN_STATUS.txt"

  python3 - "$OUT" <<'PY' > "$OUT/SUMMARY.csv" 2> "$OUT/SUMMARY_ERRORS.txt"
from pathlib import Path
import csv, json, re, sys
root=Path(sys.argv[1]); w=csv.writer(sys.stdout)
w.writerow(['round','test','start_timestamp','sender_mbps','receiver_mbps','tcp_retrans',
            'udp_loss_pct','udp_lost','udp_total','iperf_json','status'])
for p in sorted(root.glob('ROUND_*/T??_*.iperf3.json')):
    m=re.match(r'^(T\d\d_.+?)_(\d{8}_\d{6})\.iperf3\.json$', p.name)
    if not m:continue
    name, started=m.groups(); sender=receiver=retr=loss=lost=total=''; status='INCOMPLETE'
    try:
        j=json.loads(p.read_text(errors='replace'))
        end=j.get('end') or {}
        sent=end.get('sum_sent') or end.get('sum') or {}
        recv=end.get('sum_received') or end.get('sum') or {}
        if j.get('error'): raise ValueError(str(j['error']))
        if not sent or not recv: raise ValueError('Missing iperf3 end summaries')
        sender=round(float(sent['bits_per_second'])/1e6,3)
        receiver=round(float(recv['bits_per_second'])/1e6,3)
        retr=sent.get('retransmits','') if '_TCP_' in name else ''
        loss=recv.get('lost_percent','') if '_UDP_' in name else ''
        lost=recv.get('lost_packets','') if '_UDP_' in name else ''
        total=recv.get('packets','') if '_UDP_' in name else ''
        status='OK'
    except Exception as e:
        print(f'{p}: {e}',file=sys.stderr)
    w.writerow([p.parent.name,name,started,sender,receiver,retr,loss,lost,total,
                str(p.relative_to(root)),status])
PY

  # All raw files are retained even after an early failure. ZIP is outside OUT.
  (
    cd "$OUT" || exit 1
    find . -type f ! -name 'SHA256SUMS.txt' -print0 | xargs -0 shasum -a 256 > SHA256SUMS.txt
  )
  /usr/bin/ditto -c -k --keepParent "$OUT" "$ARCHIVE" > /dev/null 2>&1
  local zip_rc=$?
  if [[ $zip_rc -eq 0 ]]; then
    python3 - "$ARCHIVE" <<'PY'
import zipfile,sys
with zipfile.ZipFile(sys.argv[1]) as z:
    bad=z.testzip()
    if bad: raise SystemExit('ZIP CRC ERROR: '+bad)
PY
    if [[ $? -ne 0 ]]; then
      echo 'WARNUNG: ZIP CRC validation failed; use the preserved original folder.'
    fi
  else
    echo 'WARNUNG: ZIP creation failed; original folder is preserved.'
  fi
  echo
  echo "Originale Daten: $OUT"
  echo "Abschlussstatus: $OUT/RUN_STATUS.txt"
  if [[ -f "$ARCHIVE" ]]; then
    echo "ZIP ZUM HOCHLADEN: $ARCHIVE"
    shasum -a 256 "$ARCHIVE"
  fi
}
trap finish EXIT
trap 'echo "Abbruch durch Benutzer (SIGINT)"; exit 130' INT
trap 'echo "Abbruch durch Signal (SIGTERM)"; exit 143' TERM

check_snapshot() {
  local plist="$1"; local ref="$2"
  python3 - "$plist" "$ref" "$MODE" "$LABEL" <<'PY'
import json,plistlib,struct,sys,hashlib
source, ref, expected_mode, label=sys.argv[1:]
with open(source,'rb') as f:root=plistlib.load(f)
blobs=[]
zc19=[]
legacy_fields={k:[] for k in ('KGP_RX_EXPERIMENT','KGP_ZC5_DMA_MODE','KGP_PC1','KGP_TX_2I')}
def walk(o):
    if isinstance(o,dict):
        if isinstance(o.get('KGP_ZC19'),bytes):zc19.append(o['KGP_ZC19'])
        b=o.get('KGP_ZC10')
        if isinstance(b,bytes):blobs.append(b)
        for k in legacy_fields:
            if k in o: legacy_fields[k].append(o[k])
        for v in o.values():walk(v)
    elif isinstance(o,list):
        for v in o:walk(v)
walk(root)
if label=='ZC19':
    if blobs or len(zc19)!=1 or len(zc19[0])!=256:
        sys.exit('STOP: expected exactly one ZC19 256-byte health record, no ZC10 schema')
    w=struct.unpack('<32Q',zc19[0])
    if w[:3]!=(19,1,256) or not w[3] or not w[4]:sys.exit('STOP: invalid/unpublished ZC19 schema')
    if w[5]!=(2 if expected_mode=='mapped' else 1):sys.exit('STOP: wrong ZC19 actual DMA mode')
    if legacy_fields['KGP_RX_EXPERIMENT']!=['AirportItlwm-Tahoe-B1-ZC19-PERF']:
        sys.exit('STOP: wrong ZC19 marker')
    if any(w[i] for i in [8,9,10,13,14]):sys.exit('STOP: ZC19 PC1/TX/command/suspend/RX fault state')
    health={'Format':'ZC19_19_1','Mode':w[5],'Generation':w[6],'Pc1Fault':w[8],
        'TxBlocked':w[9],'CommandFrozen':w[10],'Pc1Reset':w[11],'CommandEpoch':w[12],
        'Pc1Suspended':w[13],'RxFault':w[14],'Publication':w[3],'NowNs':w[4],
        'BA':list(w[16:28]),'PublishMiss':w[28],'PublishFailures':w[29]}
    if ref!='NONE':
        old=json.load(open(ref))
        if any(old.get(k)!=health[k] for k in ['Mode','Generation','Pc1Reset','CommandEpoch']):
            sys.exit('STOP: ZC19 DMA mode/generation/reset/command epoch changed')
        if health['Publication']<=old.get('Publication',0):sys.exit('STOP: stale ZC19 health snapshot')
    with open(source.rsplit('.',1)[0]+'.health.json','w') as f:json.dump(health,f,indent=2)
    print('ZC19 health',health)
    sys.exit(0)
if label=='ZC8':
    # ZC8 predates KGP_ZC10. Never silently skip its legacy runtime identity/mode guard.
    # KGP_PC1 is mandatory in BOTH modes; KGP_TX_2I must exist in mapped mode.
    # Conventional ZC8 has been observed without KGP_TX_2I. That telemetry is
    # optional only there: its absence is explicitly recorded, not treated as zero.
    # Numeric PC1/reset counters are NOT decoded from these opaque legacy blobs.
    if blobs:sys.exit('STOP: unexpected KGP_ZC10 in frozen ZC8 candidate')
    for k in ('KGP_RX_EXPERIMENT','KGP_ZC5_DMA_MODE','KGP_PC1'):
        if len(legacy_fields[k])!=1:
            sys.exit(f'STOP: expected exactly one {k} property in ZC8, got {len(legacy_fields[k])}')
    tx_count=len(legacy_fields['KGP_TX_2I'])
    if tx_count>1 or (expected_mode=='mapped' and tx_count!=1):
        sys.exit(f'STOP: expected {"exactly one" if expected_mode=="mapped" else "at most one"} KGP_TX_2I property in ZC8 {expected_mode}, got {tx_count}')
    marker=legacy_fields['KGP_RX_EXPERIMENT'][0]
    mode=legacy_fields['KGP_ZC5_DMA_MODE'][0]
    want='mapped RX/TX' if expected_mode=='mapped' else 'original Dexter RX/TX'
    if marker!='AirportItlwm-Tahoe-B1-ZC8-PERF':
        sys.exit(f'STOP: wrong legacy driver marker: {marker!r}')
    if mode!=want:sys.exit(f'STOP: wrong legacy DMA mode: {mode!r} (wanted {want!r})')
    pc1=legacy_fields['KGP_PC1'][0]
    tx=legacy_fields['KGP_TX_2I'][0] if tx_count else None
    if not isinstance(pc1,bytes) or len(pc1)!=80:
        sys.exit('STOP: malformed ZC8 KGP_PC1 OSData (expected 80 bytes)')
    if tx_count and (not isinstance(tx,bytes) or len(tx)!=128):
        sys.exit('STOP: malformed ZC8 KGP_TX_2I OSData (expected 128 bytes)')
    health={'Format':'ZC8_LEGACY_OPAQUE', 'Marker':marker, 'Mode':mode,
            'PC1_Bytes':len(pc1),'TX_Bytes':len(tx) if tx_count else None,
            'PC1_SHA256':hashlib.sha256(pc1).hexdigest(),
            'TX_SHA256':hashlib.sha256(tx).hexdigest() if tx_count else None,
            'TX_Telemetry_Present':bool(tx_count),
            'TX_Telemetry_Status':'PRESENT_OPAQUE' if tx_count else 'ABSENT_OPTIONAL_CONVENTIONAL',
            'NumericFaultAndResetCountersVerified':False}
    if ref!='NONE':
        old=json.load(open(ref))
        if (old.get('Marker'),old.get('Mode'))!=(marker,mode):
            sys.exit('STOP: legacy identity/DMA mode changed')
        if old.get('TX_Telemetry_Present')!=health['TX_Telemetry_Present']:
            sys.exit('STOP: ZC8 TX telemetry presence changed during this run; preserve evidence')
    health_file=source.rsplit('.',1)[0]+'.health.json'
    with open(health_file,'w') as f:json.dump(health,f,indent=2)
    tx_status='TX_2I present (opaque)' if tx_count else 'TX_2I absent (optional in conventional)'
    print(f'ZC8 LEGACY: verified marker, {mode}, mandatory PC1; {tx_status}; '
          'numeric PC1/reset counters NOT decoded (raw snapshots retained)')
    sys.exit(0)
if len(blobs)!=1 or len(blobs[0])!=10912:
    sys.exit(f'STOP: expected exactly one 10912-byte KGP_ZC10 OSData, found {len(blobs)}')
w=struct.unpack_from('<72Q',blobs[0])
keys=['Schema','Version','Bytes','Mode','Generation','Pc1Fault','TxBlocked',
      'Pc1Reset','CommandFrozen','ResetSerial']
vals=[w[0],w[1],w[2],w[5],w[6],w[21],w[22],w[23],w[24],w[63]]
health=dict(zip(keys,vals))
health_file=source.rsplit('.',1)[0]+'.health.json'
with open(health_file,'w') as f:json.dump(health,f,indent=2)
print(f"KGP_ZC10 {w[0]}/{w[1]} Mode={w[5]} Generation={w[6]} "
      f"Pc1Fault={w[21]} TxBlocked={w[22]} CommandFrozen={w[24]} ResetSerial={w[63]}")
if (w[0],w[1],w[2])!=(10,2,10912):sys.exit('STOP: unsupported telemetry schema')
want=2 if expected_mode=='mapped' else 1
if w[5]!=want or w[21] or w[22] or w[24]:
    sys.exit('STOP: unexpected DMA mode or safety fault')
if ref!='NONE':
    old=json.load(open(ref))
    if (health['Generation']!=old['Generation'] or
        health['ResetSerial']!=old['ResetSerial'] or
        health['Pc1Reset']!=old['Pc1Reset']):
        sys.exit('STOP: generation/reset changed; preserve all evidence')
PY
}

capture() {
  local file="$1"
  ioreg -a -l -r -n "$IOREG_SERVICE" > "$file"
  check_snapshot "$file" "${INITIAL_HEALTH:-NONE}"
}

check_network() {
  local current_ip
  current_ip="$(ipconfig getifaddr "$IFACE" 2>/dev/null)" || {
    echo 'STOP: Wi-Fi has no IPv4'; return 1;
  }
  [[ "$current_ip" == "$WIFI_IP" ]] || {
    echo "STOP: Wi-Fi IP changed: $WIFI_IP -> $current_ip"; return 1;
  }
  route -n get -ifscope "$IFACE" "$SERVER" > "$OUT/ROUTE_LAST.txt"
  grep -Eq "^[[:space:]]*interface:[[:space:]]*${IFACE}$" "$OUT/ROUTE_LAST.txt" || {
    echo 'STOP: iperf peer route no longer uses Wi-Fi'; return 1;
  }
}

run_test() {
  local name="$1"; shift
  local stamp base raw stderr meta summary rc
  stamp="$(date +%Y%m%d_%H%M%S)"
  base="$rounddir/${name}_${stamp}"
  raw="$base.iperf3.json"
  stderr="$base.stderr.log"
  meta="$base.command.txt"
  summary="$base.summary.txt"
  stage="${rounddir##*/}/$name"
  local cmd=(iperf3 -c "$SERVER" -p "$PORT" -B "${WIFI_IP}%${IFACE}" -4 "$@" --get-server-output -J)

  {
    date '+START_LOCAL=%Y-%m-%dT%H:%M:%S%z'
    date -u '+START_UTC=%Y-%m-%dT%H:%M:%SZ'
    printf 'COMMAND='
    printf '%q ' "${cmd[@]}"
    printf '\n'
  } > "$meta"
  printf '%q ' "${cmd[@]}" >> "$OUT/COMMANDS.txt"; printf '\n' >> "$OUT/COMMANDS.txt"

  echo
  echo "=== $stage ==="
  set +e
  "${cmd[@]}" > "$raw" 2> "$stderr"
  rc=$?
  set -e
  {
    printf 'EXIT_CODE=%s\n' "$rc"
    date '+END_LOCAL=%Y-%m-%dT%H:%M:%S%z'
    date -u '+END_UTC=%Y-%m-%dT%H:%M:%SZ'
  } >> "$meta"

  if [[ $rc -ne 0 ]]; then
    echo "STOP: iperf3 failed (exit $rc): $stage; raw logs preserved"
    tail -n 30 "$stderr" || true
    return 1
  fi
  if ! python3 - "$raw" "$summary" <<'PY'
import json,sys
raw,out=sys.argv[1:]
try:
    j=json.load(open(raw)); end=j['end']
    if j.get('error'):raise ValueError(j['error'])
    s=end.get('sum_sent') or end.get('sum')
    r=end.get('sum_received') or end.get('sum')
    if not s or not r:raise ValueError('missing sender/receiver summaries')
    lines=[f"Sender: {s['bits_per_second']/1e6:.2f} Mbps",
           f"Receiver: {r['bits_per_second']/1e6:.2f} Mbps"]
    proto=(j.get('start') or {}).get('test_start',{}).get('protocol','').upper()
    if proto!='UDP' and 'retransmits' in s:
        lines.append(f"TCP retransmits: {s['retransmits']}")
    if 'lost_percent' in r:
        lines.append(f"UDP reported loss: {r['lost_percent']:.3f}% ({r.get('lost_packets','?')}/{r.get('packets','?')})")
    server_json=j.get('server_output_json')
    server_text=j.get('server_output_text')
    prefix=out.removesuffix('.summary.txt') if hasattr(out,'removesuffix') else out[:-len('.summary.txt')]
    if isinstance(server_json,dict):
        with open(prefix+'.server_output.json','w') as f:
            json.dump(server_json,f,indent=2)
        lines.append('Original server output: separate .server_output.json')
    elif isinstance(server_text,str):
        open(prefix+'.server_output.txt','w').write(server_text)
        lines.append('Original server output: separate .server_output.txt')
    else:
        open(prefix+'.server_output_missing.txt','w').write(
            'No server_output_json/server_output_text supplied by iperf3.\n'
            'The entire original client JSON and stderr are preserved.\n')
        lines.append('Server output unavailable; recorded explicitly')
    lines.append('All original per-interval client data are preserved in .iperf3.json.')
    open(out,'w').write('\n'.join(lines)+'\n')
except Exception as e:
    print('Invalid/incomplete iperf3 JSON:',e,file=sys.stderr)
    sys.exit(1)
PY
  then
    echo "STOP: iperf3 JSON incomplete despite exit=0: $raw"
    return 1
  fi
  completed=$((completed+1))
  cat "$summary"
  sleep "$GAP_SECONDS"
}

stage="PREFLIGHT"
{
  date '+LOCAL=%Y-%m-%dT%H:%M:%S%z'
  date -u '+UTC=%Y-%m-%dT%H:%M:%SZ'
  sw_vers
  sysctl kern.boottime
  iperf3 --version | head -n 2
  networksetup -listallhardwareports
  ifconfig "$IFACE"
  kmutil showloaded --list-only -v
  echo '=== Concurrent build/compiler processes (informational) ==='
  ps -axo pid,pcpu,pmem,comm | grep -Ei 'xcodebuild|clang|ld|codex' || true
} > "$OUT/PREFLIGHT.txt" 2>&1

LOADED="$(grep -F 'com.zxystd.AirportItlwm' "$OUT/PREFLIGHT.txt" || true)"
if [[ -z "$LOADED" || "$LOADED" != *"$UUID"* ]]; then
  echo "STOP: expected candidate UUID $UUID NOT loaded"
  echo "$LOADED"
  exit 1
fi

{
  printf 'CANDIDATE=%s\nKEXT_PATH=%s\nEXPECTED_UUID=%s\n' "$LABEL" "$KEXT" "$UUID"
  printf 'SHA256_EXECUTABLE='; shasum -a 256 "$EXECUTABLE"
  printf 'SHA256_INFO_PLIST='; shasum -a 256 "$KEXT/Contents/Info.plist"
  printf 'LOADED_RECORD=%s\n' "$LOADED"
  printf 'DEVICE_LABEL=%s\nWIFI_IFACE=%s\nIPERF_SERVER=%s\nIPERF_PORT=%s\nEXPECTED_MODE=%s\n' \
    "$DEVICE" "$IFACE" "$SERVER" "$PORT" "$MODE"
  printf 'IOREG_WIFI_SERVICE=%s\nAIRPORTITLWM_ROOT=%s\nAIRPORTITLWM_TEST_DIR=%s\n' \
    "$IOREG_SERVICE" "$ROOT" "$BASE"
} > "$OUT/CANDIDATE_IDENTITY.txt"

if [[ "$MODE" == 'mapped' ]]; then
  ioreg -r -c AppleVTD -l > "$OUT/APPLEVTD.txt"
  if ! grep -q 'registered, matched, active' "$OUT/APPLEVTD.txt"; then
    echo 'STOP: active AppleVTD not established'; exit 1
  fi
else
  ioreg -r -c AppleVTD -l > "$OUT/APPLEVTD.txt" || true
fi

WIFI_IP="$(ipconfig getifaddr "$IFACE" 2>/dev/null)" || {
  echo 'STOP: Wi-Fi IPv4 missing'; exit 1;
}
check_network
cp "$OUT/ROUTE_LAST.txt" "$OUT/ROUTE_INITIAL.txt"

cat > "$OUT/TEST_PARAMETERS.txt" <<EOF
Candidate: $LABEL
Candidate UUID (loaded and source bundle): $UUID
Wi-Fi device: $DEVICE
Wi-Fi interface/IP: $IFACE / $WIFI_IP
Wired iperf3 peer: $SERVER:$PORT
Mode: $MODE
Matrix: 3 rounds x (TCP TX P1/RX P1/TX P4/RX P4 each 8s;
        UDP TX and RX at 100/250/400/500/600 Mbps, payload 1200B, 5s each)
Between iperf3 tests: ${GAP_SECONDS}s
Logs: original full iperf3 -J --get-server-output JSON, stderr, metadata, text summary
ONLINE FINAL TEST: macOS networkQuality -I $IFACE -s -c (real Internet, upload + download)
NetworkQuality measures Internet service+Wi-Fi and is NOT the wired iperf3 peer test.
Ensure wired iperf3 peer is running iperf3 -s with Wi-Fi OFF.
IORegistry Wi-Fi service: $IOREG_SERVICE
Results directory: $BASE
Server IP selected at runtime, or supplied explicitly via IPERF_SERVER.
If Xcode/Clang is active on this Mac, note possible CPU contention in PREFLIGHT.txt.
No Wi-Fi toggle, hardware recovery, EFI/install or network configuration change.
Radio profiler deliberately NOT invoked: ZC18 TCP TX P1 A/B/A showed substantial perturbation.
EOF

stage="INITIAL_SNAPSHOT"
capture "$OUT/INITIAL.plist"
INITIAL_HEALTH="$OUT/INITIAL.health.json"

for round in 1 2 3; do
  printf -v n '%02d' "$round"
  rounddir="$OUT/ROUND_$n"
  mkdir "$rounddir"
  stage="ROUND_$n/BEFORE"
  echo
  echo "========== ROUND $round / 3 — $LABEL $DEVICE =========="
  check_network
  capture "$rounddir/BEFORE.plist"

  run_test T01_TCP_TX_P1 -t 8 -P 1
  run_test T02_TCP_RX_P1 -t 8 -P 1 -R
  run_test T03_TCP_TX_P4 -t 8 -P 4
  run_test T04_TCP_RX_P4 -t 8 -P 4 -R

  num=5
  for rate in 100 250 400 500 600; do
    printf -v testno 'T%02d' "$num"
    run_test "${testno}_UDP_TX${rate}" -u -b "${rate}M" -l 1200 -t 5
    num=$((num+1))
    printf -v testno 'T%02d' "$num"
    run_test "${testno}_UDP_RX${rate}" -u -b "${rate}M" -l 1200 -t 5 -R
    num=$((num+1))
  done

  stage="ROUND_$n/AFTER"
  capture "$rounddir/AFTER.plist"
  echo "ROUND $round completed (14 iperf3 tests)"
done

# Mandatory FINAL ONLINE Internet-based network test; do not route via wired X550.
stage="FINAL_ONLINE_NETWORKQUALITY"
check_network
sleep 3

echo
echo "=== FINAL ONLINE INTERNET TEST via $IFACE (networkQuality) ==="
echo 'This performs a real Internet upload/download test, not a local iperf3 test.'
{
  date '+START_LOCAL=%Y-%m-%dT%H:%M:%S%z'
  date -u '+START_UTC=%Y-%m-%dT%H:%M:%SZ'
  printf 'COMMAND=networkQuality -I %q -s -c\n' "$IFACE"
} > "$OUT/ONLINE_NETWORKQUALITY_COMMAND.txt"
set +e
networkQuality -I "$IFACE" -s -c > "$OUT/ONLINE_NETWORKQUALITY.json" 2> "$OUT/ONLINE_NETWORKQUALITY.stderr.log"
nq_rc=$?
set -e
{
  printf 'EXIT_CODE=%s\n' "$nq_rc"
  date '+END_LOCAL=%Y-%m-%dT%H:%M:%S%z'
  date -u '+END_UTC=%Y-%m-%dT%H:%M:%SZ'
} >> "$OUT/ONLINE_NETWORKQUALITY_COMMAND.txt"
if [[ "$nq_rc" -ne 0 ]]; then
  online_status="FAILED_EXIT_${nq_rc}"
  echo "ONLINE test failed (exit $nq_rc), raw output preserved. Local 42-test series is still valid."
  exit 1
fi
if ! python3 - "$OUT/ONLINE_NETWORKQUALITY.json" "$OUT/ONLINE_NETWORKQUALITY.txt" "$IFACE" <<'PY'
import json,sys
src,dest,iface=sys.argv[1:]
try:
    o=json.load(open(src))
    if not isinstance(o,dict):raise ValueError('not a JSON object')
    reported=o.get('interface_name')
    if reported is not None and reported!=iface:
        raise ValueError(f'networkQuality used {reported!r}, expected {iface!r}')
    v=lambda name: (f'{o[name]/1e6:.2f} Mbps' if isinstance(o.get(name),(int,float)) else 'N/A')
    lines=[f'Interface requested: {iface}', f'Interface reported: {reported or "not reported"}',
           'Internet DOWNLOAD: '+v('dl_throughput'),
           'Internet UPLOAD: '+v('ul_throughput'),
           'Responsiveness (RPM): '+str(o.get('responsiveness','N/A')),
           'Base RTT (ms): '+str(o.get('base_rtt','N/A')),
           'Original JSON is preserved as ONLINE_NETWORKQUALITY.json.']
    open(dest,'w').write('\n'.join(lines)+'\n')
except Exception as e:
    print('networkQuality JSON failure:',e,file=sys.stderr)
    sys.exit(1)
PY
then
  online_status='FAILED_JSON_OR_INTERFACE'
  echo 'STOP: online test output unusable or wrong interface; original preserved.'
  exit 1
fi
online_status='PASS'
cat "$OUT/ONLINE_NETWORKQUALITY.txt"

stage="FINAL_SNAPSHOT"
check_network
capture "$OUT/FINAL.plist"

finished_ok=1
stage="COMPLETE"
echo
echo '=== BENCHMARK FINISHED: 42/42 original iperf3 cases + final ONLINE networkQuality ==='
echo "Raw iperf3 logs, full intervals and server output saved under: $OUT"
