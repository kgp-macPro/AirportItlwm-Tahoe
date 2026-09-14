#!/usr/bin/env python3
"""Run the retained host models in disposable directories, never a kernel build."""
import subprocess, tempfile
from pathlib import Path
from verify_source import ROOT, main as verify_main
verify_main()
compiler = subprocess.check_output(["xcrun", "--find", "clang++"], text=True).strip()
sdk = subprocess.check_output(["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
cases = [("PC1_HOST_TEST", "address,undefined"), ("PC1_HOST_TEST", "thread"), ("POWER_CYCLE_HOST_TEST", "address,undefined"), ("GATE_DRAIN_TEST", "thread")]
with tempfile.TemporaryDirectory(prefix="airport-host-tests-") as tmp:
    for i, (name, sanitizer) in enumerate(cases):
        directory = Path(tmp) / str(i); directory.mkdir()
        exe = directory / "test"
        command = [compiler, "-isysroot", sdk, "-std=c++17", "-O1", "-g", "-fsanitize=" + sanitizer, str(ROOT / "tools/host_tests" / (name + ".cpp")), "-o", str(exe)]
        subprocess.run(command, check=True)
        subprocess.run([str(exe)], cwd=directory, check=True, timeout=120)
        print("PASS:", name, sanitizer, flush=True)
