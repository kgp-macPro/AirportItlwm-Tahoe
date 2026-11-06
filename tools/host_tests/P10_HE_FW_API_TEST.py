#!/usr/bin/env python3
"""Read the bundled AX firmware TLVs; never load firmware or contact a device."""
from pathlib import Path
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[2] / "itlwm/firmware"
FIRMWARE = {
    "AX201-Qu": "iwlwifi-Qu-b0-hr-b0-68.ucode",
    "AX201-QuC0": "iwlwifi-Qu-c0-hr-b0-68.ucode",
    "AX201-QuZ": "iwlwifi-QuZ-a0-hr-b0-68.ucode",
    "AX210-Ty": "iwlwifi-ty-a0-gf-a0-68.ucode",
    "AX211-So": "iwlwifi-so-a0-gf-a0-68.ucode",
}


def versions(path):
    data = path.read_bytes()
    offset = 88  # Intel TLV firmware header
    result = []
    while offset < len(data):
        assert offset + 8 <= len(data), path
        kind, size = struct.unpack_from("<II", data, offset)
        offset += 8
        assert offset + size <= len(data), path
        if kind == 48:  # IWX_UCODE_TLV_CMD_VERSIONS
            payload = data[offset:offset + size]
            assert size % 4 == 0, path
            result = [tuple(payload[i:i + 4]) for i in range(0, size, 4)]
        offset += (size + 3) & ~3
    assert offset == len(data) and result, path
    return data, result


for label, name in FIRMWARE.items():
    raw, rows = versions(ROOT / name)
    he = [v for v in rows if v[:2] == (7, 5)]
    tlc = [v for v in rows if v[:2] == (15, 5)]
    assert he == [(7, 5, 2, 0)], (label, he)
    assert tlc == [(15, 5, 3, 0)], (label, tlc)
    print(label, name, "sha256", hashlib.sha256(raw).hexdigest(),
          "HE-context-v2", "TLC-v3")
print("PASS bundled AX firmware command-version TLVs; firmware acceptance is untested")
