#!/usr/bin/env python3
"""Validate an ESP-IDF build and package one chip-revision variant.

Usage: package_firmware.py BUILD_DIR OUT_DIR VARIANT [GIT_SHA]

Expects BUILD_DIR/summit.bin, BUILD_DIR/summit-full.bin (merged by
tools/ci_build.sh) and BUILD_DIR/otadata-ota0.bin. Writes to OUT_DIR:
  summit-<ver>-<variant>-ota.bin       app only: update engine / web upload
  summit-<ver>-<variant>-full.bin      full USB install at 0x0 (erases settings)
  summit-<ver>-<variant>-otadata-ota0.bin
  manifest-<variant>.json              ESP Web Tools: full install
  manifest-<variant>-preserve.json     ESP Web Tools: keep settings (app + otadata only)
  SHA256SUMS.txt, build-info.json, summit-<ver>-<variant>-firmware.zip
Packaging only: nothing is published here.
"""
import hashlib
import json
import shutil
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import summit_image as si  # noqa: E402


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def names(version, variant):
    base = f"{si.PROJECT}-{version}-{variant}"
    return {
        "ota": f"{base}-ota.bin",
        "full": f"{base}-full.bin",
        "otadata": f"{base}-otadata-ota0.bin",
        "manifest": f"manifest-{variant}.json",
        "preserve": f"manifest-{variant}-preserve.json",
        "zip": f"{base}-firmware.zip",
    }


def manifest(version, variant, parts, preserve):
    return {
        "name": f"Project Summit {version} ({si.hardware_id(variant)})"
                + (" - keep settings" if preserve else " - full install, erases settings"),
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-P4",
                    "parts": [{"path": p, "offset": o} for o, p in parts]}],
    }


def package(build, out, variant, git_sha="local"):
    build, out = Path(build), Path(out)
    parts = si.read_partitions()
    version = si.project_version()
    n = names(version, variant)
    app = (build / f"{si.PROJECT}.bin").read_bytes()
    full = (build / f"{si.PROJECT}-full.bin").read_bytes()
    otadata = (build / "otadata-ota0.bin").read_bytes()
    if otadata != si.otadata_selecting_ota0(parts):
        raise si.ImageError("otadata-ota0.bin does not select ota_0")

    app_checks = si.check_app(app, version, parts, variant)
    full_checks = si.check_full(full, app, parts, variant)

    ota_off, od_off = parts[si.MAIN_APP_SLOT][2], parts["otadata"][2]
    full_parts = [(0, n["full"])]
    keep_parts = [(od_off, n["otadata"]), (ota_off, n["ota"])]
    si.check_usb_parts([(0, len(full))], parts, allow_nvs=True)
    si.check_usb_parts([(od_off, len(otadata)), (ota_off, len(app))], parts, allow_nvs=False)

    out.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(build / f"{si.PROJECT}.bin", out / n["ota"])
    shutil.copyfile(build / f"{si.PROJECT}-full.bin", out / n["full"])
    shutil.copyfile(build / "otadata-ota0.bin", out / n["otadata"])
    (out / n["manifest"]).write_text(
        json.dumps(manifest(version, variant, full_parts, False), indent=2) + "\n")
    (out / n["preserve"]).write_text(
        json.dumps(manifest(version, variant, keep_parts, True), indent=2) + "\n")
    binaries = (n["ota"], n["full"], n["otadata"])
    sums = {b: sha256(out / b) for b in binaries}
    (out / "SHA256SUMS.txt").write_text("".join(f"{h}  {b}\n" for b, h in sums.items()))
    info = {
        "project": si.PROJECT,
        "version": version,
        "git_sha": git_sha,
        "hardware_id": si.hardware_id(variant),
        "chip_rev_family": variant,
        "chip": "ESP32-P4",
        "ota": dict(app_checks, file=n["ota"], sha256=sums[n["ota"]]),
        "full": dict(full_checks, file=n["full"], sha256=sums[n["full"]], flash_offset="0x0"),
        "keep_settings": {"manifest": n["preserve"],
                          "writes": {hex(o): p for o, p in keep_parts}},
        "hardware_tested": False,
    }
    (out / "build-info.json").write_text(json.dumps(info, indent=2) + "\n")
    with zipfile.ZipFile(out / n["zip"], "w", zipfile.ZIP_DEFLATED) as z:
        for f in zip_members(n):
            z.write(out / f, f)
    verify(out, version, variant)
    return info


def zip_members(n):
    return [n["ota"], n["full"], n["otadata"], n["manifest"], n["preserve"],
            "SHA256SUMS.txt", "build-info.json"]


def verify(out, version, variant):
    out = Path(out)
    n = names(version, variant)
    listed = dict(reversed(l.split(maxsplit=1)) for l in
                  (out / "SHA256SUMS.txt").read_text().splitlines())
    for b in (n["ota"], n["full"], n["otadata"]):
        if listed.get(b) != sha256(out / b):
            raise si.ImageError(f"checksum mismatch for {b}")
    info = json.loads((out / "build-info.json").read_text())
    if info["hardware_id"] != si.hardware_id(variant) or info["chip_rev_family"] != variant:
        raise si.ImageError("build-info hardware_id / chip_rev_family mismatch")
    with zipfile.ZipFile(out / n["zip"]) as z:
        if z.testzip() is not None:
            raise si.ImageError("zip CRC error")
        for f in zip_members(n):
            if z.read(f) != (out / f).read_bytes():
                raise si.ImageError(f"zip member differs: {f}")


if __name__ == "__main__":
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: package_firmware.py BUILD_DIR OUT_DIR VARIANT [GIT_SHA]")
    result = package(*sys.argv[1:])
    print("PASS: firmware packaged and verified (not hardware tested)")
    print(json.dumps(result, indent=2))
