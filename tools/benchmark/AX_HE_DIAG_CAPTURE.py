#!/usr/bin/env python3
"""Capture AX HE diagnostic IORegistry records without changing the network."""
import argparse
import ctypes
import datetime as dt
import json
import re
import struct
import subprocess
import time
from pathlib import Path


class Timebase(ctypes.Structure):
    _fields_=[("numer",ctypes.c_uint32),("denom",ctypes.c_uint32)]
mach=None
try:
    mach=ctypes.CDLL(None)
    mach.mach_absolute_time.restype=ctypes.c_uint64
    mach.mach_timebase_info.argtypes=[ctypes.POINTER(Timebase)]
    tb=Timebase()
    if mach.mach_timebase_info(ctypes.byref(tb)) or not tb.denom:mach=None
except (AttributeError,OSError):mach=None
def uptime_ns():
    return mach.mach_absolute_time()*tb.numer//tb.denom if mach else None

P = argparse.ArgumentParser()
P.add_argument("--output", required=True)
P.add_argument("--device", choices=("auto","ax","be200"), default="auto", help="AX fields optional; be200 skips AX-only decoding")
P.add_argument("--seconds", type=int, default=90)
P.add_argument("--interval", type=float, default=1.0)
a = P.parse_args()
if a.seconds < 1 or a.interval < 0.2:
    P.error("seconds must be positive and interval >= 0.2")
out = Path(a.output)
if out.exists():
    P.error("output already exists")
pattern = re.compile(r'"(KGP_AX_HE_DIAG|KGP_ZC28_SERVICE|KGP_ZC19|KGP_AX_BA_LIFECYCLE|KGP_AX_TX_RATE|KGP_AX_INPUT_HANDOFF)"\s*=\s*<([0-9a-fA-F]+)>')
deadline = time.monotonic() + a.seconds
with out.open("x") as f:
    while True:
        now = dt.datetime.now(dt.timezone.utc).isoformat()
        result = subprocess.run(
            ["ioreg", "-p", "IOService", "-r", "-c", "IOPCIDevice", "-l", "-w0"],
            capture_output=True, text=True, errors="replace", check=False)
        row = {"utc": now, "ioreg_exit": result.returncode,
               "clock_anchor_wall_ns": time.time_ns(), "clock_anchor_uptime_ns": uptime_ns(),
               "clock_anchor_kind": "mach_absolute_time_ns" if mach else "unavailable"}
        for name, hexdata in pattern.findall(result.stdout):
            if a.device == "be200" and name.startswith("KGP_AX_"):
                continue
            data = bytes.fromhex(hexdata)
            row[name] = {"hex": hexdata, "bytes": len(data)}
            if name == "KGP_AX_HE_DIAG" and len(data) == 768:
                w = struct.unpack("<96Q", data)
                row[name]["schema"] = list(w[:8])
                row[name]["stats"] = list(w[8:])
            if name == "KGP_ZC28_SERVICE" and len(data) == 256:
                w = struct.unpack("<32Q", data)
                row[name]["tlc_event_count"] = w[28]
                row[name]["tlc_last_raw_and_version"] = w[29]
            if name == "KGP_ZC19" and len(data) == 256:
                row[name]["u64le"] = list(struct.unpack("<32Q", data))
            if name == "KGP_AX_BA_LIFECYCLE" and len(data) == 1216:
                w = struct.unpack("<152Q", data)
                if w[:3] != (42, 1, 1216):
                    raise ValueError("Unknown AX BA schema")
                row[name]["header"] = list(w[:8])
                row[name]["tids"] = [{"tid":i,"net80211_state":v&255,"baid":(v>>8)&255,"window":(v>>16)&65535,"removed":(v>>32)&1} for i,v in enumerate(w[8:24])]
                events = []
                for i in range(16):
                    e = w[24+8*i:32+8*i]
                    if e[0]:
                        events.append({"ticket":e[0],"uptime_ns":e[1],"generation":e[2]>>32,"reset_epoch":e[2]&0xffffffff,"event":e[3]&255,"tid":(e[3]>>8)&255,"start":(e[3]>>16)&1,"error":e[4],"firmware_status":e[5],"pending_start":e[6]>>32,"pending_stop":e[6]&0xffffffff,"sessions":e[7]})
                row[name]["events"] = sorted(events,key=lambda e:e["ticket"])
            if name == "KGP_AX_TX_RATE" and len(data) in (128, 136):
                w = struct.unpack("<"+str(len(data)//8)+"Q", data)
                if w[:3] not in ((43, 1, 128), (43, 2, 136)):
                    raise ValueError("Unknown AX TX rate schema")
                row[name]["decoded"] = dict(zip(["schema","version","bytes","uptime_ns","valid","source","raw","age_ms","kbps","mcs","nss","bw_code","gi_code","generation","reset_epoch","reporting_epoch"],w[:16]))
                row[name]["decoded"]["fresh"] = w[16] if len(w) == 17 else None
                row[name]["availability_semantics"] = "coherent_last_known_current_connection" if w[1] == 2 else "R6_fresh_only"
            if name == "KGP_AX_INPUT_HANDOFF":
                if len(data)!=5248:raise ValueError("Unknown AX input handoff length")
                w=struct.unpack("<656Q",data)
                if w[:3]!=(44,1,5248):raise ValueError("Unknown AX input handoff schema")
                row[name]["header"]=list(w[:16]);events=[]
                ip=lambda v:".".join(str((v>>shift)&255) for shift in (24,16,8,0))
                for i in range(64):
                    e=w[16+i*10:26+i*10]
                    if not e[0]:continue
                    events.append({"ticket":e[0],"entry_ns":e[1],"return_ns":e[2],
                        "duration_ns":e[2]-e[1] if e[2]>=e[1] and e[2] else None,
                        "generation":e[3]>>32,"reset_epoch":e[3]&0xffffffff,
                        "tcp_seq":e[4]>>32,"tcp_ack":e[4]&0xffffffff,
                        "source":ip(e[5]>>32),"destination":ip(e[5]&0xffffffff),
                        "source_port":e[6]&65535,"destination_port":(e[6]>>16)&65535,
                        "tcp_flags":(e[6]>>32)&255,"ip_tos":(e[6]>>40)&255,"path":(e[6]>>48)&255,
                        "ip_bytes":e[7]&65535,"payload_bytes":(e[7]>>16)&65535,"batch_packets":e[7]>>32,
                        "payload_prefix_hex":f"{e[8]&0xffffffff:08x}","mbuf_flags":e[8]>>32,
                        "if_input_return":e[9]&0xffffffff})
                row[name]["events"]=sorted(events,key=lambda e:e["ticket"])
        row["ax_diagnostics"] = "not_applicable_be200" if a.device == "be200" else "available" if "KGP_AX_HE_DIAG" in row or "KGP_AX_TX_RATE" in row else "unavailable_optional"
        f.write(json.dumps(row, separators=(",", ":")) + "\n")
        f.flush()
        if time.monotonic() >= deadline:
            break
        time.sleep(a.interval)
print(out)
