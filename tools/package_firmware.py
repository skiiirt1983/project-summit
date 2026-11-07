#!/usr/bin/env python3
"""Validate an ESP-IDF build and package the OTA + full USB images.

Usage: package_firmware.py BUILD_DIR OUT_DIR

Expects BUILD_DIR/summit.bin, BUILD_DIR/summit-full.bin (idf.py merge-bin) and
BUILD_DIR/flasher_args.json. Writes to OUT_DIR:
  summit-<ver>-ota.bin    app only: for the update engine / web upload
  summit-<ver>-full.bin   merged image flashed at 0x0 (erases settings)
  SHA256SUMS.txt, build-info.json, summit-<ver>-firmware.zip
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


def package(build, out, git_sha="local"):
    build, out = Path(build), Path(out)
    parts = si.read_partitions()
    version = si.project_version()
    app = (build / f"{si.PROJECT}.bin").read_bytes()
    full = (build / f"{si.PROJECT}-full.bin").read_bytes()
    flasher = json.loads((build / "flasher_args.json").read_text())
    app_offset = int(flasher["app"]["offset"], 0)

    app_checks = si.check_app(app, version, parts)
    full_checks = si.check_full(full, app, app_offset, parts)

    out.mkdir(parents=True, exist_ok=True)
    ota_name = f"{si.PROJECT}-{version}-ota.bin"
    full_name = f"{si.PROJECT}-{version}-full.bin"
    shutil.copyfile(build / f"{si.PROJECT}.bin", out / ota_name)
    shutil.copyfile(build / f"{si.PROJECT}-full.bin", out / full_name)
    sums = {n: sha256(out / n) for n in (ota_name, full_name)}
    (out / "SHA256SUMS.txt").write_text("".join(f"{h}  {n}\n" for n, h in sums.items()))
    info = {
        "project": si.PROJECT,
        "version": version,
        "git_sha": git_sha,
        "hardware_id": "waveshare-p4-wifi6-lcd5",
        "chip": "ESP32-P4",
        "ota": dict(app_checks, file=ota_name, sha256=sums[ota_name]),
        "full": dict(full_checks, file=full_name, sha256=sums[full_name], flash_offset="0x0"),
        "hardware_tested": False,
    }
    (out / "build-info.json").write_text(json.dumps(info, indent=2) + "\n")
    with zipfile.ZipFile(out / f"{si.PROJECT}-{version}-firmware.zip", "w",
                         zipfile.ZIP_DEFLATED) as z:
        for n in (ota_name, full_name, "SHA256SUMS.txt", "build-info.json"):
            z.write(out / n, n)
    verify(out, version)
    return info


def verify(out, version):
    out = Path(out)
    names = [f"{si.PROJECT}-{version}-ota.bin", f"{si.PROJECT}-{version}-full.bin"]
    listed = dict(reversed(l.split(maxsplit=1)) for l in
                  (out / "SHA256SUMS.txt").read_text().splitlines())
    for n in names:
        if listed.get(n) != sha256(out / n):
            raise si.ImageError(f"checksum mismatch for {n}")
    with zipfile.ZipFile(out / f"{si.PROJECT}-{version}-firmware.zip") as z:
        if z.testzip() is not None:
            raise si.ImageError("zip CRC error")
        for n in names + ["SHA256SUMS.txt", "build-info.json"]:
            if z.read(n) != (out / n).read_bytes():
                raise si.ImageError(f"zip member differs: {n}")


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: package_firmware.py BUILD_DIR OUT_DIR [GIT_SHA]")
    result = package(*sys.argv[1:])
    print("PASS: firmware packaged and verified (not hardware tested)")
    print(json.dumps(result, indent=2))
