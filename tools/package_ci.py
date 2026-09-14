#!/usr/bin/env python3
"""Package an explicitly UNQUALIFIED hosted build; never the official release."""
import hashlib, json, plistlib, subprocess, sys, zipfile
from pathlib import Path
bundle = Path(sys.argv[1]); output = Path(sys.argv[2]); output.mkdir(parents=True, exist_ok=True)
with (bundle / "Contents/Info.plist").open("rb") as f: info = plistlib.load(f)
assert bundle.name == "AirportItlwm.kext"
assert info["CFBundleIdentifier"] == "com.zxystd.AirportItlwm"
assert info["CFBundleExecutable"] == "AirportItlwm"
assert info["CFBundleVersion"] == info["CFBundleShortVersionString"] == "1.0.0"
exe = bundle / "Contents/MacOS/AirportItlwm"
assert subprocess.check_output(["lipo", "-archs", str(exe)], text=True).strip() == "x86_64"
identity = {"status": "CI BUILD — NOT PHYSICALLY VALIDATED", "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "uuid": subprocess.check_output(["dwarfdump", "--uuid", str(exe)], text=True).strip()}
(output / "CI_IDENTITY.json").write_text(json.dumps(identity, indent=2) + "\n")
with zipfile.ZipFile(output / "CI-BUILD-NOT-PHYSICALLY-VALIDATED.zip", "w", compression=zipfile.ZIP_DEFLATED) as z:
    z.writestr("CI_BUILD_NOT_PHYSICALLY_VALIDATED.txt", "This is a hosted CI build, not the physically qualified 1.0.0 release binary.\n")
    for p in sorted(bundle.rglob("*")):
        if p.is_file() and p.name != ".DS_Store": z.write(p, str(Path(bundle.name) / p.relative_to(bundle)))
    z.write(output / "CI_IDENTITY.json", "CI_IDENTITY.json")
print(identity["status"])
