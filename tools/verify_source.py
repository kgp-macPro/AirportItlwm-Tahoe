#!/usr/bin/env python3
"""Read-only frozen-input and target checks; does not compile the kext."""
import argparse, hashlib, json, plistlib, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def verify_files(root, manifest):
    for name, expected in manifest.items():
        path = root / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError("Frozen input differs or is absent: " + name)
def verify_sdk():
    sdk = ROOT / "MacKernelSDK"
    manifest = json.loads((ROOT / "tools/sdk_sha256.json").read_text())
    verify_files(sdk, manifest)
    actual = {str(p.relative_to(sdk)) for p in sdk.rglob("*") if p.is_file() and ".git" not in p.relative_to(sdk).parts}
    if actual != set(manifest):
        raise RuntimeError("Unexpected SDK files; preserve existing work and inspect it")
    if (sdk / ".git").exists():
        pin = json.loads((ROOT / "tools/sdk_pin.json").read_text())
        for expr, expected in [("HEAD", pin["commit"]), ("HEAD^{tree}", pin["tree"])]:
            actual = subprocess.check_output(["git", "--no-optional-locks", "-C", str(sdk), "rev-parse", expr], text=True).strip()
            if actual != expected:
                raise RuntimeError("SDK Git pin mismatch")
        if subprocess.check_output(["git", "--no-optional-locks", "-C", str(sdk), "status", "--porcelain", "--untracked-files=all"]):
            raise RuntimeError("SDK worktree not clean")
def verify_project():
    raw = subprocess.check_output(["plutil", "-convert", "json", "-o", "-", str(ROOT / "itlwm.xcodeproj/project.pbxproj")])
    project = json.loads(raw); objects = project["objects"]
    targets = {o["name"]: o for o in objects.values() if o.get("isa") == "PBXNativeTarget"}
    expected = {"AirportItlwm-High Sierra", "AirportItlwm-Mojave", "AirportItlwm-Catalina", "AirportItlwm-Big Sur", "AirportItlwm-Monterey", "AirportItlwm-Ventura", "AirportItlwm-Sonoma14.0", "AirportItlwm-Sonoma14.4", "AirportItlwm-Tahoe", "itlwm"}
    if not expected <= set(targets):
        raise RuntimeError("Missing upstream/product target: " + str(expected - set(targets)))
    tahoe, ventura = targets["AirportItlwm-Tahoe"], targets["AirportItlwm-Ventura"]
    def phase(target, kind):
        return [objects[p] for p in target["buildPhases"] if objects[p]["isa"] == kind]
    # Same resolved fileRefs/settings, including the inherited inert entry.
    def membership(target):
        return [objects[f] for f in phase(target, "PBXSourcesBuildPhase")[0]["files"]]
    assert membership(tahoe) == membership(ventura)
    required = {"USE_APPLE_SUPPLICANT", "AIRPORT", "__IO80211_TARGET=__MAC_13_0", "__PRIVATE_SPI__"}
    configs = objects[tahoe["buildConfigurationList"]]["buildConfigurations"]
    assert {objects[c]["name"] for c in configs} == {"Debug", "Release"}
    for cid in configs:
        settings = objects[cid]["buildSettings"]
        assert settings["MODULE_VERSION"] == "1.0.0"
        assert settings["PRODUCT_NAME"] == "AirportItlwm"
        assert settings["PRODUCT_BUNDLE_IDENTIFIER"] == "com.zxystd.AirportItlwm"
        assert settings["GCC_PREFIX_HEADER"] == "itlwm/PrivateSPI.pch"
        definitions = set(settings["GCC_PREPROCESSOR_DEFINITIONS"])
        assert required <= definitions and not any("IO80211FAMILY_V2" in x or "__MAC_26_0" in x for x in definitions)
        with (ROOT / settings["INFOPLIST_FILE"]).open("rb") as f: info = plistlib.load(f)
        assert "com.apple.iokit.IO80211FamilyLegacy" in info["OSBundleLibraries"]
    project_settings = [objects[x]["buildSettings"] for x in objects[objects[project["rootObject"]]["buildConfigurationList"]]["buildConfigurations"]]
    assert all(x.get("ARCHS") == "x86_64" for x in project_settings)
    assert any(objects[objects[d]["target"]].get("name") == "fw_gen" for d in tahoe["dependencies"])
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--sdk", action="store_true"); args = ap.parse_args()
    verify_files(ROOT, json.loads((ROOT / "tools/frozen_source_sha256.json").read_text()))
    verify_project()
    if args.sdk: verify_sdk()
    print("PASS: frozen source, legacy target/version/architecture" + (", pinned SDK" if args.sdk else ""))
if __name__ == "__main__": main()
