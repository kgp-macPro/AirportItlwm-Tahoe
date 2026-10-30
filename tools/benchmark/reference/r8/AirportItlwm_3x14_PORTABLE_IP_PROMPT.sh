#!/bin/bash
# Maintained SHARED entry point; traffic protocol AIRPORTITLWM_3X14_NO_PROFILER_V1.
# Derived from portable runner SHA256 00cf96aadddc0a130ef4c030bc80d4bea293bdf1e2e96a631c0fa1968956f15e
# r8: additional BE200-ALPHA1 profile; generic binding/identity checks; frozen r7 retained.
# r4: explicit external-build validation profile; KGP guard contracts unchanged.
# r5: simple LAOBAMAC/ZCxx plus mapped/conventional arguments; traffic unchanged.
# r6: explicit LAOBAMAC build 44/47/53 selection; existing calls remain supported.
# Traffic, order, durations, snapshot schedule and raw evaluation stay r3.
# External identity checks occur at the eight existing boundaries only.
# AirportItlwm — reusable 3x14 physical benchmark, complete iperf3 raw logs,
# and ONE final Internet-based Apple networkQuality test.
# macOS Tahoe / Bash 3.2 compatible. Does not modify EFI, driver or network settings.
#
# Usage:
#   /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh ZC18
#   /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh ZC8
#   /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh "/absolute/path/to/AirportItlwm.kext"
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
PROFILE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/runner_profiles"
PROFILE_HELPER="$PROFILE_DIR/shared_profile.py"

usage() {
  cat <<'HELP'
Usage: /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh BE200-ALPHA1|ZC28|ZC27|ZC26|ZC25|ZC24|ZC23|ZC22|ZC21|ZC20|ZC19|ZC8|ZC13|ZC18|/absolute/path/AirportItlwm.kext

Simple calls:
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh BE200-ALPHA1 mapped
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh BE200-ALPHA1 conventional
BE200-ALPHA1 explicitly selects the frozen Alpha 1/P7 profile (not ZC28).
BE200_KEXT or CANDIDATE_KEXT may override its default BE200-EXPERIMENTAL bundle.
An absolute path plus BENCHMARK_PROFILE=be200 CANDIDATE_LABEL=BE200-ALPHA1 also works.
Absent WIFI_IFACE/IOREG_WIFI_SERVICE are discovered from the actual PCI/controller/
infrastructure-interface binding, for ALL profiles. Overrides are checked, never guessed.
DEVICE_LABEL is output text, not hardware identification. P7 UUID/SHA belong only
in its candidate profile. Existing ZC/laobamac calls and guards are retained.
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh LAOBAMAC 44 mapped
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh LAOBAMAC 47 conventional
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh LAOBAMAC 53 mapped
Explicit numbered builds expect ~/Desktop/Laobamac/<number>/AirportItlwm.kext.
Build 44 = d695c97, build 47 = bbe368e, build 53 = 738d4c6.
LAOBAMAC_KEXT can override the numbered candidate path.
The earlier two-argument LAOBAMAC calls retain their r5 defaults below.
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh LAOBAMAC mapped
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh LAOBAMAC conventional
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh ZC26 mapped
  /bin/bash AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh ZC26 conventional
LAOBAMAC defaults to ~/Desktop/AirportItlwm/LAOBAMAC-738d4c6/AirportItlwm.kext
and the source declaration 738d4c61b5a64433b27c1ab09dba94916ab77d8d.
Optional LAOBAMAC_KEXT and LAOBAMAC_SOURCE_REF override those for another build.
The selected mode is a user-declared configuration for an external build,
NOT a DMA-mode switch or driver-verified KGP mode. No KGP health is fabricated.

When started, the script asks for the wired peer's CURRENT IPv4.
There is NO hard-coded or remembered server IP. Use dots, not commas.
To skip the interactive prompt, explicitly set IPERF_SERVER=192.168.178.33.

Optional environment variables (set for another host such as Luiz's AX211):
  WIFI_IFACE=<BSD name>           Optional checked interface override; otherwise discover
  DEVICE_LABEL=AX210             Name recorded in output/ZIP (e.g. AX211)
  EXPECTED_MODE=mapped           KGP: mapped or conventional
  BENCHMARK_PROFILE=kgp          kgp (default), external, or be200 candidate profile
  EXPECTED_MODE=unverified       Required/default for external profile
  EXTERNAL_SOURCE_REF=<40hex>    User-declared source commit, external only
  CANDIDATE_LABEL=LAOBAMAC_...    Safe external label, must not be ZC<number>
  IPERF_SERVER=<IPv4>            Optional non-interactive peer IPv4
  IPERF_PORT=5201                iperf3 port on the wired peer
  AIRPORTITLWM_ROOT=~/Desktop/AirportItlwm  Candidate directory root
  AIRPORTITLWM_TEST_DIR=...      Destination of originals and ZIPs
  CANDIDATE_LABEL=ZC8            For nonstandard absolute ZC8 kext paths
  IOREG_WIFI_SERVICE=<name/path> Optional checked PCI parent override; otherwise discover
  CANDIDATE_KEXT=/absolute/...   Override selected candidate path without changing its guard
  TEST_GAP_SECONDS=2             Pause between original test cases

Candidate label ZC18 resolves to:
  $AIRPORTITLWM_ROOT/B1-ZC18-PERF/AirportItlwm.kext
An absolute kext path is also accepted for other folder layouts.
The loaded AirportItlwm x86_64 UUID MUST match the candidate on disk.
The LAOBAMAC shortcut supplies the external profile and default path automatically.
Advanced external calls require BENCHMARK_PROFILE=external plus an ABSOLUTE kext path
and EXTERNAL_SOURCE_REF=<40-hex-commit>. Missing KGP diagnostics are explicitly
UNAVAILABLE, never healthy/zero or a verified mapped/conventional DMA mode.
External profile checks loaded UUID and unchanged candidate hashes at the SAME
eight existing snapshot boundaries. It rejects KGP driver markers. Source ref
is a user declaration, not proof that a downloaded binary matches that commit.
ZC8 uses verified legacy markers; KGP_TX_2I may be absent in conventional.
ZC19/ZC21/ZC22/ZC25 require KGP_ZC19 schema19/1; ZC10–18 use KGP_ZC10 10/2.

All 42 original cases and the final online networkQuality run are unchanged.
No system_profiler SPAirPortDataType or intrusive runtime sampling.
The wired iperf3 peer must be running `iperf3 -s` with its Wi-Fi OFF.
No automatic reconnect, reboot, EFI installation or network changes.
HELP
}
if [[ $# -eq 1 && "$1" == --version ]]; then
  echo 'shared-2026-10-30-r8-be200'
  shasum -a 256 "${BASH_SOURCE[0]}"
  exit 0
fi
# r8 candidate shortcut; this changes only profile/path selection, not traffic.
if [[ $# -ge 1 && "$1" == BE200-ALPHA1 ]]; then
  [[ "${BENCHMARK_PROFILE:-be200}" == be200 ]] || { echo 'STOP: BE200-ALPHA1 conflicts with BENCHMARK_PROFILE'; exit 2; }
  BENCHMARK_PROFILE=be200
  DEVICE_LABEL="${DEVICE_LABEL:-BE200}"
  if [[ $# -eq 2 ]]; then
    case "$2" in mapped|conventional) EXPECTED_MODE="$2"; set -- "$1" ;; *) echo 'STOP: expected mapped or conventional'; exit 2 ;; esac
  fi
fi
# r6 numbered shortcut expansion BEGIN
LAOBAMAC_BUILD_NUMBER=""
if [[ $# -eq 3 && ( "$1" == LAOBAMAC || "$1" == laobamac ) ]]; then
  case "$3" in mapped|conventional) ;; *) echo 'STOP: third argument must be mapped or conventional'; exit 2 ;; esac
  LAOBAMAC_BUILD_NUMBER="$2"
  case "$LAOBAMAC_BUILD_NUMBER" in
    44) LAOBAMAC_SELECTED_REF=d695c971420cc8af0daa286dcd3297e8236c0a63 ;;
    47) LAOBAMAC_SELECTED_REF=bbe368ec7d55a5395d08c729e88eeaabebae8255 ;;
    53) LAOBAMAC_SELECTED_REF=738d4c61b5a64433b27c1ab09dba94916ab77d8d ;;
    *) echo 'STOP: supported LAOBAMAC build numbers are 44, 47 and 53'; exit 2 ;;
  esac
  if [[ -n "${LAOBAMAC_SOURCE_REF:-}" && "$LAOBAMAC_SOURCE_REF" != "$LAOBAMAC_SELECTED_REF" ]]; then
    echo 'STOP: LAOBAMAC_SOURCE_REF conflicts with the selected numbered build'; exit 2
  fi
  LAOBAMAC_SOURCE_REF="$LAOBAMAC_SELECTED_REF"
  LAOBAMAC_KEXT="${LAOBAMAC_KEXT:-$HOME/Desktop/Laobamac/$LAOBAMAC_BUILD_NUMBER/AirportItlwm.kext}"
  set -- "$1" "$3"
fi
# r6 numbered shortcut expansion END
# r5 shortcut expansion BEGIN
EXTERNAL_DECLARED_MODE="${EXTERNAL_DECLARED_MODE:-unspecified}"
if [[ $# -eq 2 ]]; then
  case "$2" in mapped|conventional) ;; *) echo 'STOP: second argument must be mapped or conventional'; exit 2 ;; esac
  if [[ "$1" == LAOBAMAC || "$1" == laobamac ]]; then
    EXTERNAL_DECLARED_MODE="$2"
    BENCHMARK_PROFILE=external
    EXPECTED_MODE=unverified
    EXTERNAL_SOURCE_REF="${LAOBAMAC_SOURCE_REF:-738d4c61b5a64433b27c1ab09dba94916ab77d8d}"
    CANDIDATE_LABEL="LAOBAMAC_${LAOBAMAC_BUILD_NUMBER:+${LAOBAMAC_BUILD_NUMBER}_}${EXTERNAL_SOURCE_REF:0:7}_${EXTERNAL_DECLARED_MODE}"
    set -- "${LAOBAMAC_KEXT:-${AIRPORTITLWM_ROOT:-$HOME/Desktop/AirportItlwm}/LAOBAMAC-${EXTERNAL_SOURCE_REF:0:7}/AirportItlwm.kext}"
  elif [[ "$1" =~ ^[Zz][Cc][0-9]+$ ]]; then
    EXPECTED_MODE="$2"
    set -- "$1"
  fi
fi
# r5 shortcut expansion END
if [[ $# -ne 1 || "$1" == "--help" || "$1" == "-h" ]]; then
  usage
  if [[ $# -eq 1 && ( "$1" == "--help" || "$1" == "-h" ) ]]; then exit 0; fi
  exit 2
fi

ROOT="${AIRPORTITLWM_ROOT:-$HOME/Desktop/AirportItlwm}"
BASE="${AIRPORTITLWM_TEST_DIR:-$ROOT/TESTS}"
IFACE="${WIFI_IFACE:-}"
PORT="${IPERF_PORT:-5201}"
DEVICE="${DEVICE_LABEL:-AX210}"
PROFILE="${BENCHMARK_PROFILE:-kgp}"
case "$PROFILE" in kgp|external|be200) ;; *) echo 'STOP: BENCHMARK_PROFILE must be kgp, external or be200'; exit 2 ;; esac
if [[ "$PROFILE" == external ]]; then
  case "$EXTERNAL_DECLARED_MODE" in unspecified|mapped|conventional) ;; *) echo 'STOP: invalid external declared configuration'; exit 2 ;; esac
  MODE="${EXPECTED_MODE:-unverified}"
  [[ "$MODE" == unverified ]] || { echo 'STOP: external profile requires EXPECTED_MODE=unverified; actual DMA mode cannot be certified'; exit 2; }
  [[ "$1" == /* ]] || { echo 'STOP: external profile requires an absolute kext path'; exit 2; }
  [[ "${EXTERNAL_SOURCE_REF:-}" =~ ^[a-fA-F0-9]{40}$ ]] || { echo 'STOP: external EXTERNAL_SOURCE_REF must be a 40-hex source commit declaration'; exit 2; }
else
  MODE="${EXPECTED_MODE:-mapped}"
fi
GAP_SECONDS="${TEST_GAP_SECONDS:-2}"
IOREG_SERVICE="${IOREG_WIFI_SERVICE:-}"

if [[ "$PROFILE" != external ]]; then
  case "$MODE" in mapped|conventional) ;; *) echo 'STOP: EXPECTED_MODE must be mapped or conventional'; exit 2 ;; esac
fi
[[ -z "$IFACE" || "$IFACE" =~ ^[a-zA-Z][a-zA-Z0-9]*$ ]] || { echo 'STOP: Invalid WIFI_IFACE'; exit 2; }
[[ -z "$IOREG_SERVICE" || "$IOREG_SERVICE" =~ ^[a-zA-Z0-9_,.-]+$ || "$IOREG_SERVICE" == IOService:/* ]] || { echo 'STOP: Invalid IOREG_WIFI_SERVICE'; exit 2; }
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

if [[ "$PROFILE" == external ]]; then
  printf 'External declared configuration: %s | actual driver DMA mode: unverified\n' "$EXTERNAL_DECLARED_MODE"
fi
CANDIDATE_INPUT="$1"
if [[ "$CANDIDATE_INPUT" == */* ]]; then
  KEXT="${CANDIDATE_INPUT%/}"
  KEXT_PARENT="$(basename "$(dirname "$KEXT")")"
  if [[ "$KEXT_PARENT" == B1-ZC28-SERVICE ]]; then
    LABEL=ZC28
  elif [[ "$KEXT_PARENT" == B1-ZC27-STABILITY ]]; then
    LABEL=ZC27
  elif [[ "$KEXT_PARENT" == B1-ZC20-TX-CMD-UC ]]; then
    LABEL=ZC20
  elif [[ "$KEXT_PARENT" =~ ^B1-(ZC[0-9]+)-PERF$ ]]; then
    LABEL="${BASH_REMATCH[1]}"
  else
    LABEL="$KEXT_PARENT"
  fi
else
  LABEL="$(printf '%s' "$CANDIDATE_INPUT" | tr '[:lower:]' '[:upper:]')"
  if [[ "$PROFILE" == be200 && "$LABEL" != BE200-ALPHA1 ]]; then
    echo 'STOP: be200 profile requires BE200-ALPHA1 or an absolute candidate path'; exit 2;
  fi
  [[ "$LABEL" =~ ^ZC[0-9]+$ || ( "$PROFILE" == be200 && "$LABEL" == BE200-ALPHA1 ) ]] || {
    echo "STOP: Use ZC<number>, BE200-ALPHA1 or an absolute AirportItlwm.kext path"; exit 2;
  }
  if [[ "$PROFILE" == be200 ]]; then
    KEXT="${BE200_KEXT:-$ROOT/BE200-EXPERIMENTAL/AirportItlwm.kext}"
  elif [[ "$LABEL" == ZC28 ]]; then
    KEXT="$ROOT/B1-ZC28-SERVICE/AirportItlwm.kext"
  elif [[ "$LABEL" == ZC27 ]]; then
    KEXT="$ROOT/B1-ZC27-STABILITY/AirportItlwm.kext"
  elif [[ "$LABEL" == ZC20 ]]; then
    KEXT="$ROOT/B1-ZC20-TX-CMD-UC/AirportItlwm.kext"
  else
    KEXT="$ROOT/B1-${LABEL}-PERF/AirportItlwm.kext"
  fi
fi
# For custom directory layouts, e.g. Luiz's frozen ZC8 stored elsewhere.
if [[ "$PROFILE" == external ]]; then
  LABEL="$(printf '%s' "${CANDIDATE_LABEL:-EXTERNAL}" | tr '[:lower:]' '[:upper:]')"
  [[ "$LABEL" =~ ^[A-Z0-9][A-Z0-9_.-]{0,63}$ && ! "$LABEL" =~ ^ZC[0-9]+$ ]] || {
    echo 'STOP: external label must be 1-64 safe characters and cannot impersonate ZC<number>'; exit 2;
  }
elif [[ "$PROFILE" == be200 ]]; then
  LABEL="$(printf '%s' "${CANDIDATE_LABEL:-BE200-ALPHA1}" | tr '[:lower:]' '[:upper:]')"
  [[ "$LABEL" == BE200-ALPHA1 ]] || { echo 'STOP: unsupported BE200 candidate profile'; exit 2; }
elif [[ -n "${CANDIDATE_LABEL:-}" ]]; then
  LABEL="$(printf '%s' "$CANDIDATE_LABEL" | tr '[:lower:]' '[:upper:]')"
  [[ "$LABEL" =~ ^ZC[0-9]+$ ]] || { echo 'STOP: Invalid CANDIDATE_LABEL'; exit 2; }
fi
if [[ -n "${CANDIDATE_KEXT:-}" ]]; then
  [[ "$CANDIDATE_KEXT" == /* ]] || { echo 'STOP: CANDIDATE_KEXT must be absolute'; exit 2; }
  KEXT="$CANDIDATE_KEXT"
fi
SAFE_LABEL="$(printf '%s' "$LABEL" | tr -c '[:alnum:]_-' '_')"
SAFE_DEVICE="$(printf '%s' "$DEVICE" | tr -c '[:alnum:]_-' '_')"
EXECUTABLE="$KEXT/Contents/MacOS/AirportItlwm"
[[ -f "$EXECUTABLE" ]] || { echo "STOP: Missing candidate executable: $EXECUTABLE"; exit 1; }

for program in python3 iperf3 xcrun ioreg kmutil route ipconfig networkQuality shasum; do
  command -v "$program" >/dev/null 2>&1 || { echo "STOP: Missing command: $program"; exit 1; }
done
[[ -x /usr/bin/ditto ]] || { echo 'STOP: macOS /usr/bin/ditto unavailable'; exit 1; }

for resource in shared_profile.py decode_be200.py profiles.json; do
  [[ -f "$PROFILE_DIR/$resource" ]] || { echo "STOP: missing runner resource $PROFILE_DIR/$resource"; exit 1; }
done
# The archive UUID is verified AGAINST the loaded macOS driver, not the bundle version.
UUID_OUTPUT="$(xcrun dwarfdump --uuid "$EXECUTABLE" 2>&1)" || {
  echo 'STOP: Cannot inspect candidate Mach-O UUID'; exit 1;
}
UUID="$(printf '%s\n' "$UUID_OUTPUT" | awk '/^UUID: / && /\(x86_64\)/ {print $2; exit}')"
[[ -n "$UUID" ]] || { echo 'STOP: No x86_64 UUID in candidate'; exit 1; }
BUNDLE_ID='com.zxystd.AirportItlwm'
EXTERNAL_EXE_SHA=''
EXTERNAL_INFO_SHA=''
if [[ "$PROFILE" == external ]]; then
  BUNDLE_ID="$(python3 - "$KEXT/Contents/Info.plist" <<'PY_EXTERNAL_BUNDLE'
import plistlib,re,sys
p=plistlib.load(open(sys.argv[1],'rb'))
b=p.get('CFBundleIdentifier','')
if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',b) or p.get('CFBundleExecutable')!='AirportItlwm':
    raise SystemExit('STOP: invalid external AirportItlwm bundle metadata')
print(b)
PY_EXTERNAL_BUNDLE
  )" || exit 1
  EXTERNAL_EXE_SHA="$(shasum -a 256 "$EXECUTABLE" | awk '{print $1}')"
  EXTERNAL_INFO_SHA="$(shasum -a 256 "$KEXT/Contents/Info.plist" | awk '{print $1}')"
fi

# Freeze candidate bytes for every profile; existing external behavior is retained.
if [[ "$PROFILE" != external ]]; then
  EXTERNAL_EXE_SHA="$(shasum -a 256 "$EXECUTABLE" | awk '{print $1}')"
  EXTERNAL_INFO_SHA="$(shasum -a 256 "$KEXT/Contents/Info.plist" | awk '{print $1}')"
fi
mkdir -p "$BASE"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="$(mktemp -d "$BASE/${SAFE_LABEL}_${SAFE_DEVICE}_3X14_${MODE}_${STAMP}_XXXXXX")"
ARCHIVE="$OUT.zip"
completed=0
stage="PREPARATION"
online_status="NOT_RUN"
finished_ok=0
INITIAL_HEALTH=""
PREVIOUS_HEALTH=""

# Fixed traffic protocol unchanged from frozen ZC22; revision covers tooling only.
PROTOCOL_VERSION='AIRPORTITLWM_3X14_NO_PROFILER_V1'
RUNNER_REVISION='shared-2026-10-30-r8-be200'
RUNNER_SOURCE="${BASH_SOURCE[0]}"
record_protocol() {
  python3 - "$RUNNER_SOURCE" "$OUT" "$PROTOCOL_VERSION" "$RUNNER_REVISION" "$1" \
    "$LABEL" "$KEXT" "$UUID" "$DEVICE" "$IFACE" "${WIFI_IP:-}" "$SERVER" \
    "$PORT" "$MODE" "$GAP_SECONDS" "$IOREG_SERVICE" "$ROOT" "$BASE" "$BASH_VERSION" \
    "$PROFILE" "${EXTERNAL_SOURCE_REF:-}" "$BUNDLE_ID" "$EXTERNAL_DECLARED_MODE" "$PROFILE_DIR" <<'PY_PROTOCOL'
from pathlib import Path
import datetime,hashlib,json,sys,os
(source,folder,version,revision,phase,label,kext,uuid,device,iface,wifi_ip,server,
 port,mode,gap,service,root,base,bash,profile,declared_commit,bundle_id,declared_dma,resources)=sys.argv[1:]
p=Path(folder);src=Path(source).resolve();copy=p/'EXECUTED_RUNNER.sh'
if not copy.exists():copy.write_bytes(src.read_bytes())
copy_sha=hashlib.sha256(copy.read_bytes()).hexdigest()
source_sha=hashlib.sha256(src.read_bytes()).hexdigest()
provenance=p/'RUNNER_PROVENANCE.json'
old=json.loads(provenance.read_text()) if provenance.exists() else {}
expected=old.get('runner_sha256',copy_sha)
valid=(copy_sha==expected==source_sha)
resource_copy=p/'RUNNER_RESOURCES';resource_copy.mkdir(exist_ok=True)
resource_hashes={}
for name in ['shared_profile.py','decode_be200.py','profiles.json']:
    source_file=Path(resources)/name;copy_file=resource_copy/name
    if not copy_file.exists():copy_file.write_bytes(source_file.read_bytes())
    h=hashlib.sha256(copy_file.read_bytes()).hexdigest()
    current=hashlib.sha256(source_file.read_bytes()).hexdigest()
    valid=valid and h==current==old.get('resource_sha256',{}).get(name,h)
    resource_hashes[name]=h
events=old.get('records',[])+[{'phase':phase,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'source_sha256':source_sha,'copy_sha256':copy_sha,'unchanged':valid}]
provenance.write_text(json.dumps({'protocol_version':version,'runner_revision':revision,
 'invoked_source':str(src),'archived_copy':'EXECUTED_RUNNER.sh','runner_sha256':expected,
 'copy_matches_executed_source_at_checks':valid,'resource_sha256':resource_hashes,'records':events},indent=2)+'\n')
(p/'EXECUTED_RUNNER_SHA256.txt').write_text(expected+'  EXECUTED_RUNNER.sh\n')
matrix=[{'name':'TCP_'+direction+'_P'+str(streams),'seconds':8,'streams':streams,'reverse':direction=='RX'}
 for streams in (1,4) for direction in ('TX','RX')]
matrix += [{'name':'UDP_'+direction+str(rate),'seconds':5,'offered_mbps':rate,'payload_bytes':1200,'reverse':direction=='RX'}
 for rate in (100,250,400,500,600) for direction in ('TX','RX')]
settings={'protocol_version':version,'runner_revision':revision,'phase':phase,
 'candidate':label,'candidate_path':kext,'expected_loaded_uuid':uuid,'device_label':device,
 'wifi_interface':iface,'wifi_ipv4':wifi_ip or None,'server_ipv4':server,'server_port':int(port),
 'expected_actual_mode':mode if profile in ('kgp','be200') else None,
 'validation_profile':profile,'candidate_bundle_id':bundle_id,
 'declared_external_source_commit':declared_commit.lower() if profile=='external' else None,
 'external_source_build_correspondence_verified':False if profile=='external' else None,
 'declared_external_dma_configuration':declared_dma if profile=='external' else None,
 'external_actual_dma_mode_verified':False if profile=='external' else None,
 'kgp_guard_profile_enabled':profile in ('kgp','be200'),
 'be200_guard_profile_enabled':profile=='be200',
 'device_label_is_hardware_identification':False,
 'verified_binding_record':'DEVICE_BINDING.json',
 'requested_wifi_iface':os.environ.get('WIFI_IFACE'),
 'requested_ioreg_parent':os.environ.get('IOREG_WIFI_SERVICE'),
 'resource_sha256':resource_hashes,
 'numeric_kgp_health_available':profile in ('kgp','be200') and label!='ZC8',
 'external_boundary_identity_checks':profile=='external',
 'binding':(wifi_ip+'%'+iface) if wifi_ip else None,
 'rounds':3,'tests_per_round':matrix,'between_iperf_tests_seconds':int(gap),
 'matches_frozen_zc22_default_pauses':int(gap)==2,
 'iperf_fixed_options':['-4','--get-server-output','-J'],
 'final_online':['networkQuality','-I',iface,'-s','-c'],'before_online_pause_seconds':3,
 'ioreg_service':service,'snapshot_schedule':['INITIAL','R1_BEFORE','R1_AFTER','R2_BEFORE','R2_AFTER','R3_BEFORE','R3_AFTER','FINAL'],
 'candidate_root':root,'output_root':base,'bash_version':bash,
 'interpretation':'Receiver goodput is authoritative; original client JSON, intervals and separate server output are retained. ONLINE PASS means execution only.'}
(p/'EFFECTIVE_SETTINGS.json').write_text(json.dumps(settings,indent=2)+'\n')
(p/'BENCHMARK_PROTOCOL.txt').write_text(version+'\nRunner revision: '+revision+'\nRunner SHA256: '+expected+
 '\nTraffic/order/durations/raw evaluation equal frozen ZC22. Nondefault gap is explicitly recorded.\n'+
 'Changes r8: additional BE200 profile and generic binding/identity verification; frozen r7 retained.\n'+
 'Observer change: one full IOService discovery before tests; selected-provider and identity checks at the original eight boundaries only.\n'+
 'External: UUID/hash identity checks at the same eight boundaries; KGP DMA/epoch/fault telemetry unavailable.\n'+
 'Traffic/order/durations/online evaluation unchanged; no extra traffic, 1Hz sampling or profiler.\n')
if not valid:raise SystemExit('STOP: executed runner source/copy changed during collection; archive cannot be marked complete')
PY_PROTOCOL
}


finish() {
  local rc=$?
  trap - EXIT
  set +e
  record_protocol final
  if [[ $? -ne 0 ]]; then rc=1; finished_ok=0; fi
  if [[ "$PROFILE" == be200 ]]; then
    python3 "$PROFILE_HELPER" summary "$OUT"
    if [[ $? -ne 0 ]]; then rc=1; finished_ok=0; fi
  fi
  {
    if (( rc == 0 && completed == 42 && finished_ok == 1 )); then
      echo 'STATUS=COMPLETE_42_AND_ONLINE'
    else
      echo 'STATUS=INCOMPLETE_OR_ONLINE_FAILURE'
    fi
    printf 'EXIT_CODE=%s\nCOMPLETED_IPERF_TESTS=%s\nEXPECTED_IPERF_TESTS=42\nONLINE_STATUS=%s\nLAST_STAGE=%s\n' \
      "$rc" "$completed" "$online_status" "$stage"
    printf 'VALIDATION_PROFILE=%s\n' "$PROFILE"
    if [[ "$PROFILE" == external ]]; then
      echo 'ACTUAL_DMA_MODE_VERIFIED=false'
      echo 'NUMERIC_KGP_HEALTH_VERIFIED=false'
      echo 'COMPLETION_SCOPE=42_TESTS_ONLINE_AND_BOUNDARY_IDENTITY_ONLY'
    fi
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
  exit "$rc"
}
trap finish EXIT
trap 'echo "Abbruch durch Benutzer (SIGINT)"; exit 130' INT
trap 'echo "Abbruch durch Signal (SIGTERM)"; exit 143' TERM
record_protocol initial

check_snapshot() {
  local plist="$1"; local ref="$2"; local previous="$3"; local phase="$4"
  BENCHMARK_CANDIDATE_PROFILE_JSON="$PROFILE_DIR/profiles.json" BENCHMARK_PROFILE="$PROFILE" BENCHMARK_EXPECTED_UUID="$UUID" BENCHMARK_BUNDLE_ID="$BUNDLE_ID" BENCHMARK_EXPECTED_SERVICE="$IOREG_SERVICE" \
    python3 - "$plist" "$ref" "$previous" "$MODE" "$LABEL" "$phase" <<'PY'
import json,plistlib,struct,sys,hashlib,os
source, ref, previous, expected_mode, label, phase=sys.argv[1:]
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
profile=os.environ.get('BENCHMARK_PROFILE','kgp')
if profile not in ('kgp','external','be200'):sys.exit('STOP: invalid validation profile')
if profile=='external':
    if expected_mode!='unverified':sys.exit('STOP: external actual DMA mode is unverified')
    if phase not in ('boundary','final'):sys.exit('STOP: unknown external snapshot phase')
    if (not isinstance(root,list) or len(root)!=1 or not isinstance(root[0],dict) or
        root[0].get('IORegistryEntryName')!=os.environ.get('BENCHMARK_EXPECTED_SERVICE') or
        not isinstance(root[0].get('IORegistryEntryID'),int) or root[0]['IORegistryEntryID']<=0):
        sys.exit('STOP: external requires exactly one matching IORegistry device snapshot')
    if blobs or zc19 or any(legacy_fields.values()):
        sys.exit('STOP: external profile cannot bypass KGP driver guards')
    if label.startswith('ZC') and label[2:].isdigit():sys.exit('STOP: external label impersonates KGP candidate')
    identity=json.load(open(source.rsplit('.',1)[0]+'.identity.json'))
    if (identity.get('loaded_uuid')!=os.environ.get('BENCHMARK_EXPECTED_UUID','').upper() or
        identity.get('bundle_id')!=os.environ.get('BENCHMARK_BUNDLE_ID') or
        identity.get('candidate_hashes_unchanged') is not True):
        sys.exit('STOP: external boundary identity not verified')
    health={'Format':'EXTERNAL_NO_KGP_HEALTH_V1','ValidationProfile':'external',
            'LoadedUUID':identity['loaded_uuid'],'BundleIdentifier':identity['bundle_id'],
            'IORegistryDeviceID':root[0]['IORegistryEntryID'],
            'DeclaredSourceCommit':identity['declared_source_commit'],
            'Mode':None,'ActualDMAModeVerified':False,
            'NumericFaultAndResetCountersVerified':False,
            'GenerationEpochAndPublicationProgressVerified':False,
            'KGPHealthStatus':'UNAVAILABLE_NOT_ZERO',
            'SourceBuildCorrespondenceVerified':False,
            'BoundaryIdentityVerified':True}
    if ref!='NONE':
        old=json.load(open(ref))
        if any(old.get(k)!=health[k] for k in ('Format','LoadedUUID','BundleIdentifier','DeclaredSourceCommit','IORegistryDeviceID')):
            sys.exit('STOP: external identity declaration changed')
    with open(source.rsplit('.',1)[0]+'.health.json','w') as f:json.dump(health,f,indent=2)
    print('EXTERNAL: boundary UUID/hashes verified; actual DMA mode and KGP health UNAVAILABLE')
    sys.exit(0)
if label in ('ZC19','ZC20','ZC21','ZC22','ZC23','ZC24','ZC25','ZC26','ZC27','ZC28') or (profile=='be200' and label=='BE200-ALPHA1'):
    if blobs or len(zc19)!=1 or len(zc19[0])!=256:
        sys.exit('STOP: expected exactly one ZC19 256-byte health record, no ZC10 schema')
    w=struct.unpack('<32Q',zc19[0])
    if w[:3]!=(19,1,256) or not w[3] or not w[4]:sys.exit('STOP: invalid/unpublished ZC19 schema')
    if w[5]!=(2 if expected_mode=='mapped' else 1):sys.exit('STOP: wrong ZC19 actual DMA mode')
    marker=(json.load(open(os.environ['BENCHMARK_CANDIDATE_PROFILE_JSON']))[label]['marker'] if profile=='be200' else
            'AirportItlwm-Tahoe-B1-ZC28-SERVICE' if label=='ZC28' else
            'AirportItlwm-Tahoe-B1-ZC27-STABILITY' if label=='ZC27' else
            'AirportItlwm-Tahoe-B1-ZC20-TX-CMD-UC' if label=='ZC20' else 'AirportItlwm-Tahoe-B1-'+label+'-PERF')
    if legacy_fields['KGP_RX_EXPERIMENT']!=[marker]:
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
    # The existing 1000-ms watchdog may skip a busy gate. Adjacent boundary
    # captures can legitimately read the same published record; do not poll.
    # Compare with the LAST accepted snapshot, not only INITIAL, so a rollback
    # is rejected even when its counter is still greater than INITIAL's.
    if previous!='NONE':
        last=json.load(open(previous))
        if health['Publication']<last['Publication']:
            sys.exit('STOP: ZC19 health publication counter rolled back')
    if phase not in ('boundary','final'):
        sys.exit('STOP: unknown ZC19 snapshot phase')
    # Require observed progress across the campaign before marking COMPLETE.
    # FINAL may equal its immediate predecessor if both exceed INITIAL.
    # This proves some publication progress, not continuous watchdog liveness.
    if phase=='final':
        if ref=='NONE' or previous=='NONE':
            sys.exit('STOP: missing ZC19 campaign health reference')
        if health['Publication']<=old['Publication']:
            sys.exit('STOP: ZC19 health publication did not advance during campaign')
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
# Shared runner supports these established 10/2 identities explicitly.
if label not in ('ZC13','ZC18'):sys.exit('STOP: candidate has no supported health schema in this shared runner')
if legacy_fields['KGP_RX_EXPERIMENT']!=['AirportItlwm-Tahoe-B1-'+label+'-PERF']:
    sys.exit('STOP: wrong KGP_ZC10 candidate marker')
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
  local result=$?
  [[ "$result" -eq 0 ]] || return "$result"
  if [[ "$PROFILE" == be200 ]]; then
    python3 "$PROFILE_HELPER" be200-health "$plist" "${plist%.plist}.health.json" "$ref" "$previous" "$phase" "$LABEL" || return $?
  fi
}

external_identity_snapshot() {
  local file="$1"
  kmutil showloaded --list-only -v > "${file%.plist}.loaded.txt" 2>&1 || return $?
  python3 - "${file%.plist}.loaded.txt" "$EXECUTABLE" "$KEXT/Contents/Info.plist" \
    "$UUID" "$BUNDLE_ID" "$EXTERNAL_EXE_SHA" "$EXTERNAL_INFO_SHA" \
    "$EXTERNAL_SOURCE_REF" "${file%.plist}.identity.json" <<'PY_EXTERNAL_IDENTITY'
import hashlib,json,plistlib,re,sys
from pathlib import Path
loaded,exe,info,uuid,bundle,exe_sha,info_sha,commit,out=sys.argv[1:]
records=[line for line in Path(loaded).read_text().splitlines() if bundle in line.split()]
if len(records)!=1:raise SystemExit('STOP: external bundle must have exactly one loaded record')
uuids=re.findall(r'(?<![A-Fa-f0-9])[A-Fa-f0-9]{8}(?:-[A-Fa-f0-9]{4}){3}-[A-Fa-f0-9]{12}(?![A-Fa-f0-9])',records[0])
if len(uuids)!=1 or uuids[0].upper()!=uuid.upper():raise SystemExit('STOP: external loaded UUID mismatch')
actual_exe=hashlib.sha256(Path(exe).read_bytes()).hexdigest()
actual_info=hashlib.sha256(Path(info).read_bytes()).hexdigest()
p=plistlib.loads(Path(info).read_bytes())
if actual_exe!=exe_sha or actual_info!=info_sha or p.get('CFBundleIdentifier')!=bundle:
    raise SystemExit('STOP: external candidate changed during collection')
if not re.fullmatch(r'[a-fA-F0-9]{40}',commit):raise SystemExit('STOP: malformed external source declaration')
Path(out).write_text(json.dumps({'bundle_id':bundle,'loaded_uuid':uuid.upper(),
 'loaded_record':records[0],'candidate_hashes_unchanged':True,
 'executable_sha256':actual_exe,'info_plist_sha256':actual_info,
 'declared_source_commit':commit.lower(),'source_build_correspondence_verified':False},indent=2)+'\n')
PY_EXTERNAL_IDENTITY
}

capture() {
  local file="$1"; local phase="${2:-boundary}"
  ioreg -a -l -r -n "$IOREG_SERVICE" > "${file%.plist}.ioreg_raw.plist"
  python3 "$PROFILE_HELPER" resolve "${file%.plist}.ioreg_raw.plist" "$KEXT/Contents/Info.plist" \
    "${file%.plist}.binding.json" "$file" "$PROFILE" "$LABEL" "$IFACE" "$IOREG_SERVICE" "$OUT/DEVICE_BINDING.json" > /dev/null || return $?
  if [[ "$PROFILE" == external ]]; then
    external_identity_snapshot "$file" || return $?
  else
    kmutil showloaded --list-only -v > "${file%.plist}.loaded.txt" 2>&1 || return $?
    python3 "$PROFILE_HELPER" identity "${file%.plist}.loaded.txt" "$EXECUTABLE" "$KEXT/Contents/Info.plist" \
      "$UUID" "$BUNDLE_ID" "$EXTERNAL_EXE_SHA" "$EXTERNAL_INFO_SHA" "$PROFILE" "$LABEL" "${EXTERNAL_SOURCE_REF:-}" "${file%.plist}.identity.json" || return $?
  fi
  check_snapshot "$file" "${INITIAL_HEALTH:-NONE}" "${PREVIOUS_HEALTH:-NONE}" "$phase" || return $?
  PREVIOUS_HEALTH="${file%.plist}.health.json"
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

stage="DISCOVER_BOUND_INTERFACE"
ioreg -a -l -w0 -p IOService > "$OUT/DEVICE_DISCOVERY.plist"
python3 "$PROFILE_HELPER" resolve "$OUT/DEVICE_DISCOVERY.plist" "$KEXT/Contents/Info.plist" \
  "$OUT/DEVICE_BINDING.json" "$OUT/DISCOVERED_PROVIDER.plist" "$PROFILE" "$LABEL" "$IFACE" "$IOREG_SERVICE" NONE > /dev/null
IFACE="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["interface_name"])' "$OUT/DEVICE_BINDING.json")"
IOREG_SERVICE="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["provider_name"])' "$OUT/DEVICE_BINDING.json")"
record_protocol discovered
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

LOADED="$(grep -F "$BUNDLE_ID" "$OUT/PREFLIGHT.txt" || true)"
if [[ -z "$LOADED" || "$LOADED" != *"$UUID"* ]]; then
  echo "STOP: expected candidate UUID $UUID NOT loaded"
  echo "$LOADED"
  exit 1
fi

{
  printf 'CANDIDATE=%s\nKEXT_PATH=%s\nEXPECTED_UUID=%s\n' "$LABEL" "$KEXT" "$UUID"
  printf 'VALIDATION_PROFILE=%s\nBUNDLE_IDENTIFIER=%s\n' "$PROFILE" "$BUNDLE_ID"
  if [[ "$PROFILE" == external ]]; then
    printf 'DECLARED_SOURCE_COMMIT=%s\nDECLARED_DMA_CONFIGURATION=%s\n' "$EXTERNAL_SOURCE_REF" "$EXTERNAL_DECLARED_MODE"
    echo 'SOURCE_BUILD_CORRESPONDENCE_VERIFIED=false'
    echo 'ACTUAL_DMA_MODE_VERIFIED=false'
    echo 'NUMERIC_KGP_HEALTH_VERIFIED=false'
  fi
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
record_protocol prepared

cat > "$OUT/TEST_PARAMETERS.txt" <<EOF
Candidate: $LABEL
Candidate UUID (loaded and source bundle): $UUID
Wi-Fi device: $DEVICE
Wi-Fi interface/IP: $IFACE / $WIFI_IP
Wired iperf3 peer: $SERVER:$PORT
Mode: $MODE
Validation profile: $PROFILE
External declared source commit: ${EXTERNAL_SOURCE_REF:-N/A}
External declared DMA configuration (not verified): $EXTERNAL_DECLARED_MODE
External profile: no KGP DMA/fault/epoch/publication-health certification.
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
           'Download responsiveness (RPM): '+str(o.get('dl_responsiveness','N/A')),
           'Upload responsiveness (RPM): '+str(o.get('ul_responsiveness','N/A')),
           'Global responsiveness (RPM, when present): '+str(o.get('responsiveness','N/A')),
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
capture "$OUT/FINAL.plist" final

finished_ok=1
stage="COMPLETE"
echo
echo '=== BENCHMARK FINISHED: 42/42 original iperf3 cases + final ONLINE networkQuality ==='
echo "Raw iperf3 logs, full intervals and server output saved under: $OUT"
