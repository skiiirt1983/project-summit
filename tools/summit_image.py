#!/usr/bin/env python3
"""Read-only checks for Summit firmware images. Never flashes hardware.

Pattern adapted from LyricFlash tools/check_build.py and validate_full_image.py,
for the ESP32-P4 and the fixed 32 MB Summit partition table.

Boot rules enforced here:
- The main app lives only in ota_0. factory is reserved for the recovery app.
- Every USB install writes an initialised otadata that selects ota_0. A blank
  otadata makes the bootloader fall back to factory.
- Two chip-revision variants are built: rev3 (ESP32-P4 v3.x, default) and
  rev1 (v1.x, CONFIG_ESP32P4_SELECTS_REV_LESS_V3). They are not interchangeable.
"""
import binascii
import csv
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS_CSV = ROOT / "firmware" / "partitions.csv"
CMAKELISTS = ROOT / "firmware" / "CMakeLists.txt"

PROJECT = "summit"
BOARD_ID = "waveshare-p4-wifi6-lcd5"
CHIP_ID_ESP32P4 = 0x0012          # esp_app_format.h: ESP_CHIP_ID_ESP32P4
IMAGE_MAGIC = 0xE9
APP_DESC_MAGIC = 0xABCD5432
BOOTLOADER_OFFSET = 0x2000        # ESP32-P4 second-stage bootloader
TABLE_OFFSET = 0x8000
TABLE_ENTRY_MAGIC = b"\xaa\x50"
FLASH_SIZE = 32 * 1024 * 1024
MAIN_APP_SLOT = "ota_0"
MIN_HEADROOM = 0.30               # ARCHITECTURE.md: >=30% headroom in an 8 MB slot

# Chip-revision families: (min_chip_rev_full range, max_chip_rev_full) in the image header.
VARIANTS = {
    "rev3": {"min_range": (300, 399), "max": 399},
    "rev1": {"min_range": (100, 199), "max": 199},
}

APP_TYPE = {"app": 0x00, "data": 0x01}
OTA_IMG_UNDEFINED = 0xFFFFFFFF    # boots without a pending-verify cycle
SECTOR = 0x1000


class ImageError(ValueError):
    pass


def hardware_id(variant):
    if variant not in VARIANTS:
        raise ImageError(f"unknown chip-revision variant {variant!r}")
    return f"{BOARD_ID}-{variant}"


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


def otadata_selecting_ota0(partitions):
    """otadata image that makes the bootloader boot ota_0 (seq 1), with sector 1 blank."""
    size = partitions["otadata"][3]
    seq = struct.pack("<I", 1)
    crc = binascii.crc32(seq, 0xFFFFFFFF) & 0xFFFFFFFF   # same as ESP-IDF otatool.py
    entry = seq + b"\xff" * 20 + struct.pack("<II", OTA_IMG_UNDEFINED, crc)
    return entry + b"\xff" * (size - len(entry))


def _cstr(b):
    return b.split(b"\0", 1)[0].decode("ascii", "strict")


def header_info(img):
    if len(img) < 0x20 or img[0] != IMAGE_MAGIC:
        raise ImageError("not an Espressif image")
    chip, _, min_full, max_full = struct.unpack_from("<HBHH", img, 12)
    return {"chip_id": chip, "min_chip_rev_full": min_full, "max_chip_rev_full": max_full}


def check_variant(info, variant, what):
    if info["chip_id"] != CHIP_ID_ESP32P4:
        raise ImageError(f"{what}: wrong chip id {info['chip_id']:#x}, expected ESP32-P4")
    v = VARIANTS[variant]
    lo, hi = v["min_range"]
    if not lo <= info["min_chip_rev_full"] <= hi or info["max_chip_rev_full"] != v["max"]:
        raise ImageError(
            f"{what}: chip revision {info['min_chip_rev_full']}..{info['max_chip_rev_full']} "
            f"is not the {variant} variant")


def app_info(app):
    info = header_info(app)
    if len(app) < 0x120 or struct.unpack_from("<I", app, 0x20)[0] != APP_DESC_MAGIC:
        raise ImageError("missing ESP-IDF app descriptor at 0x20")
    return dict(info, version=_cstr(app[0x30:0x50]), project=_cstr(app[0x50:0x70]))


def check_app(app, version, partitions, variant):
    info = app_info(app)
    check_variant(info, variant, "app")
    if info["project"] != PROJECT:
        raise ImageError(f"wrong project {info['project']!r}")
    if info["version"] != version:
        raise ImageError(f"image version {info['version']} != PROJECT_VER {version}")
    slot = partitions[MAIN_APP_SLOT][3]
    if len(app) > slot:
        raise ImageError(f"app {len(app)} bytes does not fit {MAIN_APP_SLOT} ({slot})")
    headroom = 1 - len(app) / slot
    if headroom < MIN_HEADROOM:
        raise ImageError(f"only {headroom:.0%} headroom in {MAIN_APP_SLOT}; need {MIN_HEADROOM:.0%}")
    return dict(info, bytes=len(app), slot=MAIN_APP_SLOT, slot_bytes=slot,
                headroom=round(headroom, 4))


def table_from_image(full):
    rows = {}
    for i in range(95):
        e = full[TABLE_OFFSET + i * 32:TABLE_OFFSET + (i + 1) * 32]
        if e[:2] != TABLE_ENTRY_MAGIC:
            break
        _, typ, sub, off, size, label, _ = struct.unpack("<2sBBII16sI", e)
        rows[_cstr(label)] = (typ, off, size)
    return rows


def check_full(full, app, partitions, variant):
    app_offset = partitions[MAIN_APP_SLOT][2]
    if not app_offset < len(full) <= FLASH_SIZE:
        raise ImageError(f"bad full image length {len(full)}")
    check_variant(header_info(full[BOOTLOADER_OFFSET:]), variant,
                  f"bootloader at {BOOTLOADER_OFFSET:#x}")
    table = table_from_image(full)
    expected = {n: (APP_TYPE[t], o, s) for n, (t, _, o, s) in partitions.items()}
    if table != expected:
        raise ImageError(f"partition table in image differs from partitions.csv: {table}")
    _, _, od_off, od_size = partitions["otadata"]
    if full[od_off:od_off + od_size] != otadata_selecting_ota0(partitions):
        raise ImageError("otadata in full image does not select ota_0 (blank otadata boots factory)")
    _, _, f_off, f_size = partitions["factory"]
    if any(b != 0xFF for b in full[f_off:f_off + f_size]):
        raise ImageError("full image writes to factory; the main app belongs in ota_0 only")
    if full[app_offset:app_offset + len(app)] != app:
        raise ImageError(f"app at ota_0 ({app_offset:#x}) differs from the OTA app")
    return {"bytes": len(full), "app_partition": MAIN_APP_SLOT, "app_offset": hex(app_offset)}


def check_usb_parts(parts, partitions, allow_nvs):
    """parts: [(offset, length)]. The keep-settings path must not touch nvs."""
    _, _, n_off, n_size = partitions["nvs"]
    for off, length in parts:
        if off + length > FLASH_SIZE:
            raise ImageError(f"write at {off:#x} runs past end of flash")
        if not allow_nvs and off < n_off + n_size and off + length > n_off:
            raise ImageError(f"keep-settings write at {off:#x} overlaps nvs")
