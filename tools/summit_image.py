#!/usr/bin/env python3
"""Read-only checks for Summit firmware images. Never flashes hardware.

Pattern adapted from LyricFlash tools/check_build.py and validate_full_image.py,
for the ESP32-P4 and the fixed 32 MB Summit partition table.
"""
import csv
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS_CSV = ROOT / "firmware" / "partitions.csv"
CMAKELISTS = ROOT / "firmware" / "CMakeLists.txt"

PROJECT = "summit"
CHIP_ID_ESP32P4 = 0x0012          # esp_app_format.h: ESP_CHIP_ID_ESP32P4
IMAGE_MAGIC = 0xE9
APP_DESC_MAGIC = 0xABCD5432
BOOTLOADER_OFFSET = 0x2000        # ESP32-P4 second-stage bootloader
TABLE_OFFSET = 0x8000
TABLE_ENTRY_MAGIC = b"\xaa\x50"
FLASH_SIZE = 32 * 1024 * 1024
OTA_SLOT = "ota_0"
MIN_HEADROOM = 0.30               # ARCHITECTURE.md: >=30% headroom in an 8 MB slot

APP_TYPE = {"app": 0x00, "data": 0x01}


class ImageError(ValueError):
    pass


def read_partitions(path=PARTITIONS_CSV):
    rows = {}
    with Path(path).open() as f:
        lines = (l for l in f if l.strip() and not l.lstrip().startswith("#"))
        for p in csv.reader(lines):
            p = [x.strip() for x in p]
            rows[p[0]] = (p[1], p[2], int(p[3], 0), int(p[4], 0))
    return rows


def project_version(path=CMAKELISTS):
    m = re.search(r'set\(PROJECT_VER\s+"([^"]+)"\)', Path(path).read_text())
    if not m:
        raise ImageError("PROJECT_VER missing from firmware/CMakeLists.txt")
    return m.group(1)


def _cstr(b):
    return b.split(b"\0", 1)[0].decode("ascii", "strict")


def app_info(app):
    if len(app) < 0x120 or app[0] != IMAGE_MAGIC:
        raise ImageError("not an Espressif application image")
    (magic,) = struct.unpack_from("<I", app, 0x20)
    if magic != APP_DESC_MAGIC:
        raise ImageError("missing ESP-IDF app descriptor at 0x20")
    return {
        "chip_id": struct.unpack_from("<H", app, 12)[0],
        "version": _cstr(app[0x30:0x50]),
        "project": _cstr(app[0x50:0x70]),
    }


def check_app(app, version, partitions):
    info = app_info(app)
    if info["chip_id"] != CHIP_ID_ESP32P4:
        raise ImageError(f"wrong chip id {info['chip_id']:#x}, expected ESP32-P4")
    if info["project"] != PROJECT:
        raise ImageError(f"wrong project {info['project']!r}")
    if info["version"] != version:
        raise ImageError(f"image version {info['version']} != PROJECT_VER {version}")
    slot = partitions[OTA_SLOT][3]
    if len(app) > slot:
        raise ImageError(f"app {len(app)} bytes does not fit {OTA_SLOT} ({slot})")
    headroom = 1 - len(app) / slot
    if headroom < MIN_HEADROOM:
        raise ImageError(f"only {headroom:.0%} headroom in {OTA_SLOT}; need {MIN_HEADROOM:.0%}")
    return dict(info, bytes=len(app), slot_bytes=slot, headroom=round(headroom, 4))


def table_from_image(full):
    rows = {}
    for i in range(95):
        e = full[TABLE_OFFSET + i * 32:TABLE_OFFSET + (i + 1) * 32]
        if e[:2] != TABLE_ENTRY_MAGIC:
            break
        _, typ, sub, off, size, label, _ = struct.unpack("<2sBBII16sI", e)
        rows[_cstr(label)] = (typ, off, size)
    return rows


def check_full(full, app, app_offset, partitions):
    if not app_offset < len(full) <= FLASH_SIZE:
        raise ImageError(f"bad full image length {len(full)}")
    if full[BOOTLOADER_OFFSET] != IMAGE_MAGIC:
        raise ImageError(f"no bootloader at {BOOTLOADER_OFFSET:#x}")
    if struct.unpack_from("<H", full, BOOTLOADER_OFFSET + 12)[0] != CHIP_ID_ESP32P4:
        raise ImageError("bootloader is not built for ESP32-P4")
    table = table_from_image(full)
    expected = {n: (APP_TYPE[t], o, s) for n, (t, _, o, s) in partitions.items()}
    if table != expected:
        raise ImageError(f"partition table in image differs from partitions.csv: {table}")
    hit = [n for n, (t, _, o, s) in partitions.items() if t == "app" and o == app_offset]
    if not hit:
        raise ImageError(f"app offset {app_offset:#x} is not an app partition")
    if len(app) > partitions[hit[0]][3]:
        raise ImageError(f"app does not fit the {hit[0]} partition it is flashed to")
    if full[app_offset:app_offset + len(app)] != app:
        raise ImageError(f"full image app at {app_offset:#x} differs from OTA app")
    return {"bytes": len(full), "app_partition": hit[0], "app_offset": hex(app_offset)}
