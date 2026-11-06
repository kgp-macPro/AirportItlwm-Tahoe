#!/usr/bin/env python3
"""Fail closed if the 2.0.0 protected production or test-reference bytes differ."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];m=json.loads((r/'tools/release_2_0_0_inputs.json').read_text())
def sha(b):return hashlib.sha256(b).hexdigest()
for p,h in m['files'].items():assert sha((r/p).read_bytes())==h,('2.0.0 protected input mismatch',p)
for p,h in m['r9_files'].items():
 b=(r/p).read_bytes()
 if p=='itlwm.xcodeproj/project.pbxproj':
  assert b.count(b'MODULE_VERSION = 2.0.0;')==2
  b=b.replace(b'MODULE_VERSION = 2.0.0;',b'MODULE_VERSION = 1.0.0;')
 assert sha(b)==h,('Functional input differs from tested R9',p)
for p,h in m['reference_files'].items():assert sha((r/p).read_bytes())==h,('Regression reference changed',p)
# Refuse unlisted tracked/untracked code in protected production directories.
paths=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=r).decode().split('\0')
for p in paths:
 if p and any(p.startswith(x+'/')for x in m['protected_directories']):assert p in m['files'],('Unlisted production input',p)
print('PASS',len(m['files']),'protected production/SDK/firmware inputs; R9 equivalence except two Tahoe version values;',len(m['reference_files']),'test references')
