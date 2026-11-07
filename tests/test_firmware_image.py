"""Host tests for tools/summit_image.py and tools/package_firmware.py.

Uses synthetic images that mimic ESP-IDF's layout; no ESP-IDF needed.
"""
import json
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package_firmware as pkg  # noqa: E402
import summit_image as si  # noqa: E402

PARTS = si.read_partitions()
VERSION = si.project_version()
APP_OFFSET = PARTS["factory"][2]  # where idf.py flashes the app today


def make_app(size=0x40000, chip=si.CHIP_ID_ESP32P4, project=si.PROJECT, version=VERSION):
    b = bytearray(size)
    b[0] = si.IMAGE_MAGIC
    struct.pack_into("<H", b, 12, chip)
    struct.pack_into("<I", b, 0x20, si.APP_DESC_MAGIC)
    b[0x30:0x30 + len(version)] = version.encode()
    b[0x50:0x50 + len(project)] = project.encode()
    return bytes(b)


def make_full(app, app_offset=APP_OFFSET, parts=PARTS):
    b = bytearray(b"\xff" * (app_offset + len(app)))
    b[si.BOOTLOADER_OFFSET] = si.IMAGE_MAGIC
    struct.pack_into("<H", b, si.BOOTLOADER_OFFSET + 12, si.CHIP_ID_ESP32P4)
    sub = {"nvs": 2, "ota": 0, "phy": 1, "coredump": 3, "littlefs": 0x83,
           "factory": 0, "ota_0": 0x10, "ota_1": 0x11}
    for i, (name, (t, s, o, size)) in enumerate(parts.items()):
        e = struct.pack("<2sBBII16sI", si.TABLE_ENTRY_MAGIC, si.APP_TYPE[t], sub[s],
                        o, size, name.encode(), 0)
        b[si.TABLE_OFFSET + i * 32:si.TABLE_OFFSET + (i + 1) * 32] = e
    b[app_offset:app_offset + len(app)] = app
    return bytes(b)


class AppChecks(unittest.TestCase):
    def test_valid_app(self):
        r = si.check_app(make_app(), VERSION, PARTS)
        self.assertEqual(r["project"], "summit")
        self.assertGreater(r["headroom"], 0.9)

    def test_rejects_wrong_chip(self):
        with self.assertRaisesRegex(si.ImageError, "chip"):
            si.check_app(make_app(chip=9), VERSION, PARTS)

    def test_rejects_wrong_project(self):
        with self.assertRaisesRegex(si.ImageError, "project"):
            si.check_app(make_app(project="lyricflash"), VERSION, PARTS)

    def test_rejects_version_mismatch(self):
        with self.assertRaisesRegex(si.ImageError, "version"):
            si.check_app(make_app(version="9.9.9"), VERSION, PARTS)

    def test_rejects_less_than_30_percent_headroom(self):
        with self.assertRaisesRegex(si.ImageError, "headroom"):
            si.check_app(make_app(size=int(0x800000 * 0.71)), VERSION, PARTS)

    def test_rejects_non_image(self):
        with self.assertRaises(si.ImageError):
            si.check_app(b"\0" * 0x200, VERSION, PARTS)


class FullChecks(unittest.TestCase):
    def test_valid_full(self):
        app = make_app()
        r = si.check_full(make_full(app), app, APP_OFFSET, PARTS)
        self.assertEqual(r["app_partition"], "factory")

    def test_accepts_app_in_ota_0(self):
        app, off = make_app(), PARTS["ota_0"][2]
        self.assertEqual(si.check_full(make_full(app, off), app, off, PARTS)["app_partition"], "ota_0")

    def test_rejects_changed_partition_table(self):
        app = make_app()
        moved = dict(PARTS, coredump=("data", "coredump", 0x10D000, 0x10000))
        with self.assertRaisesRegex(si.ImageError, "partition table"):
            si.check_full(make_full(app, parts=moved), app, APP_OFFSET, PARTS)

    def test_rejects_app_mismatch(self):
        app = make_app()
        full = bytearray(make_full(app))
        full[APP_OFFSET + 0x100] ^= 0xFF
        with self.assertRaisesRegex(si.ImageError, "differs"):
            si.check_full(bytes(full), app, APP_OFFSET, PARTS)

    def test_rejects_missing_bootloader(self):
        app = make_app()
        full = bytearray(make_full(app))
        full[si.BOOTLOADER_OFFSET] = 0xFF
        with self.assertRaisesRegex(si.ImageError, "bootloader"):
            si.check_full(bytes(full), app, APP_OFFSET, PARTS)

    def test_rejects_app_too_big_for_factory(self):
        app = make_app(size=PARTS["factory"][3] + 0x1000)
        with self.assertRaisesRegex(si.ImageError, "factory"):
            si.check_full(make_full(app), app, APP_OFFSET, PARTS)


class Packaging(unittest.TestCase):
    def test_package_and_verify(self):
        with tempfile.TemporaryDirectory() as d:
            build, out = Path(d, "build"), Path(d, "dist")
            build.mkdir()
            app = make_app()
            (build / "summit.bin").write_bytes(app)
            (build / "summit-full.bin").write_bytes(make_full(app))
            (build / "flasher_args.json").write_text(
                json.dumps({"app": {"offset": hex(APP_OFFSET), "file": "summit.bin"}}))
            info = pkg.package(build, out, "abc123")
            self.assertFalse(info["hardware_tested"])
            z = zipfile.ZipFile(out / f"summit-{VERSION}-firmware.zip")
            self.assertEqual(len(z.namelist()), 4)
            (out / f"summit-{VERSION}-ota.bin").write_bytes(make_app(size=0x40010))
            with self.assertRaisesRegex(si.ImageError, "checksum"):
                pkg.verify(out, VERSION)


if __name__ == "__main__":
    unittest.main(verbosity=1)
