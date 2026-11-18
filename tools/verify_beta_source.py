#!/usr/bin/env python3
"""Verify published R49/BLE inputs; no runtime or hardware qualification."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1]
m=json.loads((r/'tools/release_2_1_0_beta_1_inputs.json').read_text())
overlay_path=r/'tools/bluetoolfixup_2_7_6_inputs.json'
overlay=json.loads(overlay_path.read_text())['files'] if overlay_path.exists() else {}
assert all(p.startswith(('bluetooth/BlueToolFixup-Intel/','tools/bluetooth-intel/')) for p in overlay), 'Hotfix may not override WLAN/SDK/firmware inputs'
expected={**m['files'],**overlay}
for p,v in expected.items():
    assert hashlib.sha256((r/p).read_bytes()).hexdigest()==v['sha256'],('Original input differs',p)
tracked=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=r).decode().split('\0')
for p in tracked:
    if p and any(p.startswith(d+'/') for d in m['protected_directories']):
        assert p in expected,('Unlisted production input',p)
print('Verified',len(expected),'pinned inputs, including explicit BTF overlay; R49 WLAN/SDK/firmware unchanged. No hardware claim.')
