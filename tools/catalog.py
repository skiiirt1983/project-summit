#!/usr/bin/env python3
"""Update catalog (format 1): build entries from CI build-info.json and validate catalogs.

Same rules as the device (firmware/services/update/src/catalog.cpp); see
docs/ARCHITECTURE.md "Update catalog". Used by the release workflow so a bad
catalog is rejected before any device sees it.

  catalog.py entry BUILD_INFO --url URL --channel stable|beta|alpha [--notes TEXT] [--released YYYY-MM-DD]
  catalog.py validate CATALOG_JSON
"""
import argparse
import json
import re
import sys
from pathlib import Path

FORMAT = 1
PROJECT = "summit"
PARTITION_LAYOUT = 1
SLOT_BYTES = 0x800000
ALLOWED_HOSTS = {"skiiirt1983.github.io"}
CHANNELS = ("stable", "beta", "alpha")
HARDWARE_ID = re.compile(r"^waveshare-p4-wifi6-lcd5-rev[13]$")
SEMVER = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-((?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*)(?:\.(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))*))?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
URL = re.compile(r"^https://([a-z0-9.-]+)/[^\s\\\"\x7f]+$")


def url_host(url):
    m = URL.match(url)
    return m.group(1) if m else ""


def entry(build_info, url, channel, notes="", released=""):
    info = json.loads(Path(build_info).read_text())
    e = {
        "version": info["version"],
        "channel": channel,
        "hardware_id": info["hardware_id"],
        "partition_layout": PARTITION_LAYOUT,
        "url": url,
        "size": info["ota"]["bytes"],
        "sha256": info["ota"]["sha256"],
        "released": released,
        "notes": notes,
    }
    errors = validate_build(e)
    if errors:
        raise ValueError("; ".join(errors))
    return e


def validate_build(b):
    errors = []
    v = b.get("version", "")
    if not isinstance(v, str) or len(v) > 31 or not SEMVER.match(v) or "+" in v:
        errors.append(f"bad version {v!r}")
    if b.get("channel") not in CHANNELS:
        errors.append(f"bad channel {b.get('channel')!r}")
    if not isinstance(b.get("hardware_id"), str) or not HARDWARE_ID.match(b["hardware_id"]):
        errors.append(f"bad hardware_id {b.get('hardware_id')!r}")
    if b.get("partition_layout") != PARTITION_LAYOUT:
        errors.append(f"partition_layout must be {PARTITION_LAYOUT}")
    url = b.get("url", "")
    host = url_host(url) if isinstance(url, str) else ""
    if host not in ALLOWED_HOSTS:
        errors.append(f"url must be https on {sorted(ALLOWED_HOSTS)}: {url!r}")
    size = b.get("size")
    if not isinstance(size, int) or isinstance(size, bool) or not 0 < size <= SLOT_BYTES:
        errors.append(f"bad size {size!r}")
    if not isinstance(b.get("sha256"), str) or not SHA256.match(b["sha256"]):
        errors.append("sha256 must be 64 lowercase hex")
    return errors


def validate(catalog):
    errors = []
    if catalog.get("format") != FORMAT:
        errors.append(f"format must be {FORMAT}")
    if catalog.get("project") != PROJECT:
        errors.append(f"project must be {PROJECT!r}")
    builds = catalog.get("builds")
    if not isinstance(builds, list):
        return errors + ["builds must be a list"]
    seen = set()
    for i, b in enumerate(builds):
        if not isinstance(b, dict):
            errors.append(f"builds[{i}] is not an object")
            continue
        errors += [f"builds[{i}] ({b.get('version')}): {e}" for e in validate_build(b)]
        key = (b.get("version"), b.get("hardware_id"))
        if key in seen:
            errors.append(f"builds[{i}]: duplicate {key}")  # releases are immutable (hard rule 7)
        seen.add(key)
    return errors


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("entry")
    e.add_argument("build_info")
    e.add_argument("--url", required=True)
    e.add_argument("--channel", required=True, choices=CHANNELS)
    e.add_argument("--notes", default="")
    e.add_argument("--released", default="")
    v = sub.add_parser("validate")
    v.add_argument("catalog")
    args = ap.parse_args()
    if args.cmd == "entry":
        print(json.dumps(entry(args.build_info, args.url, args.channel, args.notes, args.released), indent=2))
        return 0
    errors = validate(json.loads(Path(args.catalog).read_text()))
    for err in errors:
        print("ERROR:", err)
    print("PASS: catalog valid" if not errors else f"FAIL: {len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
