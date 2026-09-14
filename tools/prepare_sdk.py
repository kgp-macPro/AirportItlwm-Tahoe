#!/usr/bin/env python3
"""Acquire only the pinned standalone SDK; refuse to overwrite existing work."""
import json, os, subprocess, tempfile
from pathlib import Path
from verify_source import ROOT, verify_sdk
pin = json.loads((ROOT / "tools/sdk_pin.json").read_text())
dest = ROOT / "MacKernelSDK"
if dest.exists() or dest.is_symlink():
    if dest.is_symlink(): raise RuntimeError("Refusing an SDK symlink")
    verify_sdk()
    print("PASS: existing exact pinned SDK")
else:
    # Stage outside the destination; failed acquisition does not replace anything.
    with tempfile.TemporaryDirectory(prefix="airport-sdk-") as temp:
        staged = Path(temp) / "MacKernelSDK"
        subprocess.run(["git", "init", "--quiet", str(staged)], check=True)
        def git(*args): return subprocess.check_output(["git", "-C", str(staged), *args], text=True).strip()
        git("remote", "add", "origin", pin["url"])
        git("fetch", "--depth=1", "origin", pin["commit"])
        git("checkout", "--detach", "FETCH_HEAD")
        if git("rev-parse", "HEAD") != pin["commit"] or git("rev-parse", "HEAD^{tree}") != pin["tree"]:
            raise RuntimeError("Fetched SDK identity mismatch")
        from verify_source import verify_files
        verify_files(staged, json.loads((ROOT / "tools/sdk_sha256.json").read_text()))
        # copytree refuses an existing destination, including a concurrently created one.
        import shutil
        shutil.copytree(staged, dest, symlinks=True)
    verify_sdk()
    print("PASS: acquired exact pinned SDK")
