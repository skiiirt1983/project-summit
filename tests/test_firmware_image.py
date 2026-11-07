"""Host tests for tools/summit_image.py, make_flash_images.py and package_firmware.py.

Uses synthetic images that mimic ESP-IDF's layout; no ESP-IDF needed.
Header values mirror a real CI build (rev3: chip 0x12, chip rev 301..399).
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
import make_flash_images as mfi  # noqa: E402
import package_firmware as pkg  # noqa: E402
import summit_image as si  # noqa: E402

PARTS = si.read_partitions()
VERSION = si.project_version()
OTA0 = PARTS["ota_0"][2]
FACTORY = PARTS["factory"][2]
OTADATA = PARTS["otadata"][2]
REVS = {"rev3": (301, 399), "rev1": (100, 199)}


def header(b, variant, chip=si.CHIP_ID_ESP32P4, revs=None):
    lo, hi = revs or REVS[variant]
    b[0] = si.IMAGE_MAGIC
    struct.pack_into("<HBHH", b, 12, chip, lo // 100, lo, hi)


def make_app(size=0x40000, variant="rev3", project=si.PROJECT, version=VERSION, **kw):
    b = bytearray(size)
    header(b, variant, **kw)
    struct.pack_into("<I", b, 0x20, si.APP_DESC_MAGIC)
    b[0x30:0x30 + len(version)] = version.encode()
    b[0x50:0x50 + len(project)] = project.encode()
    return bytes(b)


def make_full(app, app_offset=OTA0, parts=PARTS, variant="rev3", otadata=None):
    b = bytearray(b"\xff" * (max(app_offset, OTA0) + len(app)))
    boot = bytearray(0x100)
    header(boot, variant)
    b[si.BOOTLOADER_OFFSET:si.BOOTLOADER_OFFSET + len(boot)] = boot
    sub = {"nvs": 2, "ota": 0, "phy": 1, "coredump": 3, "littlefs": 0x83,
           "factory": 0, "ota_0": 0x10, "ota_1": 0x11}
    for i, (name, (t, s, o, size)) in enumerate(parts.items()):
        e = struct.pack("<2sBBII16sI", si.TABLE_ENTRY_MAGIC, si.APP_TYPE[t], sub[s],
                        o, size, name.encode(), 0)
        b[si.TABLE_OFFSET + i * 32:si.TABLE_OFFSET + (i + 1) * 32] = e
    od = si.otadata_selecting_ota0(PARTS) if otadata is None else otadata
    b[OTADATA:OTADATA + len(od)] = od
    b[app_offset:app_offset + len(app)] = app
    return bytes(b)


def rom_crc32_le(crc, data):
    """Independent re-implementation of the ESP ROM esp_rom_crc32_le()."""
    crc = ~crc & 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return ~crc & 0xFFFFFFFF


class Otadata(unittest.TestCase):
    def test_selects_ota0_with_valid_crc(self):
        od = si.otadata_selecting_ota0(PARTS)
        self.assertEqual(len(od), PARTS["otadata"][3])
        seq, state, crc = struct.unpack_from("<I", od, 0)[0], *struct.unpack_from("<II", od, 24)
        self.assertEqual(seq, 1)                       # (seq - 1) % 2 == 0 -> ota_0
        self.assertEqual(state, si.OTA_IMG_UNDEFINED)  # boots without pending-verify
        self.assertEqual(crc, rom_crc32_le(0xFFFFFFFF, struct.pack("<I", seq)))
        self.assertEqual(od[si.SECTOR:], b"\xff" * si.SECTOR)  # second copy blank


class AppChecks(unittest.TestCase):
    def test_valid_app_both_variants(self):
        for v in si.VARIANTS:
            r = si.check_app(make_app(variant=v), VERSION, PARTS, v)
            self.assertEqual((r["project"], r["slot"]), ("summit", "ota_0"))

    def test_rejects_wrong_variant(self):
        with self.assertRaisesRegex(si.ImageError, "rev1 variant"):
            si.check_app(make_app(variant="rev3"), VERSION, PARTS, "rev1")
        with self.assertRaisesRegex(si.ImageError, "rev3 variant"):
            si.check_app(make_app(variant="rev1"), VERSION, PARTS, "rev3")

    def test_rejects_wrong_chip(self):
        with self.assertRaisesRegex(si.ImageError, "chip id"):
            si.check_app(make_app(chip=9), VERSION, PARTS, "rev3")

    def test_rejects_wrong_project(self):
        with self.assertRaisesRegex(si.ImageError, "project"):
            si.check_app(make_app(project="lyricflash"), VERSION, PARTS, "rev3")

    def test_rejects_version_mismatch(self):
        with self.assertRaisesRegex(si.ImageError, "version"):
            si.check_app(make_app(version="9.9.9"), VERSION, PARTS, "rev3")

    def test_rejects_less_than_30_percent_headroom(self):
        with self.assertRaisesRegex(si.ImageError, "headroom"):
            si.check_app(make_app(size=int(0x800000 * 0.71)), VERSION, PARTS, "rev3")

    def test_rejects_non_image(self):
        with self.assertRaises(si.ImageError):
            si.check_app(b"\0" * 0x200, VERSION, PARTS, "rev3")


class FullChecks(unittest.TestCase):
    def test_valid_full_app_in_ota0(self):
        app = make_app()
        r = si.check_full(make_full(app), app, PARTS, "rev3")
        self.assertEqual(r, {"bytes": OTA0 + len(app), "app_partition": "ota_0",
                             "app_offset": hex(OTA0)})

    def test_rejects_app_in_factory(self):
        app = make_app()
        with self.assertRaisesRegex(si.ImageError, "factory"):
            si.check_full(make_full(app, app_offset=FACTORY), app, PARTS, "rev3")

    def test_rejects_blank_otadata(self):
        app = make_app()
        blank = b"\xff" * PARTS["otadata"][3]
        with self.assertRaisesRegex(si.ImageError, "otadata"):
            si.check_full(make_full(app, otadata=blank), app, PARTS, "rev3")

    def test_rejects_bootloader_of_other_variant(self):
        app = make_app(variant="rev1")
        with self.assertRaisesRegex(si.ImageError, "bootloader"):
            si.check_full(make_full(app, variant="rev3"), app, PARTS, "rev1")

    def test_rejects_changed_partition_table(self):
        app = make_app()
        moved = dict(PARTS, coredump=("data", "coredump", 0x10D000, 0x10000))
        with self.assertRaisesRegex(si.ImageError, "partition table"):
            si.check_full(make_full(app, parts=moved), app, PARTS, "rev3")

    def test_rejects_app_mismatch(self):
        app = make_app()
        full = bytearray(make_full(app))
        full[OTA0 + 0x100] ^= 0xFF
        with self.assertRaisesRegex(si.ImageError, "differs"):
            si.check_full(bytes(full), app, PARTS, "rev3")


class UsbParts(unittest.TestCase):
    def test_keep_settings_must_not_touch_nvs(self):
        si.check_usb_parts([(OTADATA, 0x2000), (OTA0, 0x40000)], PARTS, allow_nvs=False)
        with self.assertRaisesRegex(si.ImageError, "nvs"):
            si.check_usb_parts([(0x0, 0x200000)], PARTS, allow_nvs=False)


def fake_build(d, variant="rev3"):
    build = Path(d, "build")
    (build / "bootloader").mkdir(parents=True)
    (build / "partition_table").mkdir()
    app = make_app(variant=variant)
    (build / "summit.bin").write_bytes(app)
    (build / "flasher_args.json").write_text(json.dumps({
        "bootloader": {"offset": "0x2000", "file": "bootloader/bootloader.bin"},
        "partition-table": {"offset": "0x8000", "file": "partition_table/partition-table.bin"},
        "app": {"offset": hex(FACTORY), "file": "summit.bin"},   # idf.py default
    }))
    return build, app


class MergeArgs(unittest.TestCase):
    def test_app_goes_to_ota0_with_otadata(self):
        with tempfile.TemporaryDirectory() as d:
            build, _ = fake_build(d)
            args = mfi.merge_args(build)
            pairs = dict(zip(args[::2], args[1::2]))
            self.assertEqual(pairs[hex(OTA0)], str(build / "summit.bin"))
            self.assertEqual(pairs[hex(OTADATA)], str(build / "otadata-ota0.bin"))
            self.assertNotIn(hex(FACTORY), pairs)
            self.assertEqual((build / "otadata-ota0.bin").read_bytes(),
                             si.otadata_selecting_ota0(PARTS))


class Packaging(unittest.TestCase):
    def run_package(self, variant):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        build, app = fake_build(d.name, variant)
        mfi.merge_args(build)
        (build / "summit-full.bin").write_bytes(make_full(app, variant=variant))
        out = Path(d.name, "dist")
        return pkg.package(build, out, variant, "abc123"), out

    def test_package_both_variants(self):
        for v in si.VARIANTS:
            info, out = self.run_package(v)
            self.assertEqual(info["hardware_id"], f"waveshare-p4-wifi6-lcd5-{v}")
            self.assertEqual(info["chip_rev_family"], v)
            self.assertFalse(info["hardware_tested"])
            z = zipfile.ZipFile(out / f"summit-{VERSION}-{v}-firmware.zip")
            self.assertEqual(len(z.namelist()), 7)

    def test_keep_settings_manifest_writes_otadata_and_ota0_only(self):
        _, out = self.run_package("rev3")
        m = json.loads((out / "manifest-rev3-preserve.json").read_text())
        offsets = {p["offset"]: p["path"] for p in m["builds"][0]["parts"]}
        self.assertEqual(offsets, {OTADATA: f"summit-{VERSION}-rev3-otadata-ota0.bin",
                                   OTA0: f"summit-{VERSION}-rev3-ota.bin"})
        full = json.loads((out / "manifest-rev3.json").read_text())
        self.assertEqual(full["builds"][0]["parts"], [{"path": f"summit-{VERSION}-rev3-full.bin",
                                                       "offset": 0}])

    def test_verify_detects_tampering(self):
        _, out = self.run_package("rev3")
        (out / f"summit-{VERSION}-rev3-ota.bin").write_bytes(make_app(size=0x40010))
        with self.assertRaisesRegex(si.ImageError, "checksum"):
            pkg.verify(out, VERSION, "rev3")

    def test_rejects_wrong_variant_build(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        build, app = fake_build(d.name, "rev1")
        mfi.merge_args(build)
        (build / "summit-full.bin").write_bytes(make_full(app, variant="rev1"))
        with self.assertRaisesRegex(si.ImageError, "rev3 variant"):
            pkg.package(build, Path(d.name, "dist"), "rev3")


if __name__ == "__main__":
    unittest.main(verbosity=1)
