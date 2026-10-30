#!/usr/bin/env python3
"""Verify tested P8R1 production inputs and vendored reference fixtures; no deployment."""
from pathlib import Path
import hashlib,json
R=Path(__file__).resolve().parents[1]
for name in ['be200_p8r1_build_inputs_sha256.json','be200_reference_sha256.json']:
 data=json.loads((R/'tools'/name).read_text())
 for p,h in data['files'].items():
  assert hashlib.sha256((R/p).read_bytes()).hexdigest()==h,('Frozen P8R1/reference input changed',p)
print('PASS: tested P8R1 production/SDK/firmware bytes and pinned offline references')
