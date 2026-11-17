#!/usr/bin/env python3
"""Verify published R49/BLE inputs; no runtime or hardware qualification."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1]
m=json.loads((r/'tools/release_2_1_0_beta_1_inputs.json').read_text())
for p,v in m['files'].items():
    assert hashlib.sha256((r/p).read_bytes()).hexdigest()==v['sha256'],('Original input differs',p)
tracked=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=r).decode().split('\0')
for p in tracked:
    if p and any(p.startswith(d+'/') for d in m['protected_directories']):
        assert p in m['files'],('Unlisted production input',p)
print('Verified',len(m['files']),'original R49/BLE/SDK/firmware inputs. No hardware claim.')
