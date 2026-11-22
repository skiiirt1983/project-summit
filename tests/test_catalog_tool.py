"""tools/catalog.py: entries from build-info.json and the device's validation rules."""
import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402

SHA = "ab" * 32
URL = "https://skiiirt1983.github.io/slopeform-home/firmware/0.1.0/summit-0.1.0-rev3-ota.bin"


def build_info(tmp, version="0.1.0", hw="waveshare-p4-wifi6-lcd5-rev3"):
    p = Path(tmp) / "build-info.json"
    p.write_text(json.dumps({"project": "summit", "version": version, "hardware_id": hw,
                             "ota": {"bytes": 1228800, "sha256": SHA}}))
    return p


class Catalog(unittest.TestCase):
    def good(self):
        with tempfile.TemporaryDirectory() as d:
            e = catalog.entry(build_info(d), URL, "stable", "notes", "2026-11-07")
        return {"format": 1, "project": "summit", "builds": [e]}

    def test_entry_from_build_info_is_valid(self):
        c = self.good()
        self.assertEqual(catalog.validate(c), [])
        b = c["builds"][0]
        self.assertEqual((b["version"], b["size"], b["sha256"], b["partition_layout"]), ("0.1.0", 1228800, SHA, 1))

    def test_header(self):
        c = self.good()
        c["format"] = 2
        self.assertIn("format must be 1", catalog.validate(c))
        c = self.good()
        c["project"] = "lyricflash"
        self.assertTrue(catalog.validate(c))

    def test_each_bad_field(self):
        bad = {
            "version": ["0.1", "01.0.0", "1.0.0+meta", "1.0.0-", "v1.0.0"],
            "channel": ["nightly", None],
            "hardware_id": ["waveshare-p4-wifi6-lcd5", "waveshare-p4-wifi6-lcd5-rev2"],
            "partition_layout": [2, "1"],
            "url": ["http://skiiirt1983.github.io/x.bin", "https://evil.example.com/x.bin",
                    "https://skiiirt1983.github.io:444/x.bin", "https://u@skiiirt1983.github.io/x.bin",
                    "https://skiiirt1983.github.io/"],
            "size": [0, 0x800001, "100", True],
            "sha256": ["AB" * 32, "ab" * 31],
        }
        for field, values in bad.items():
            for value in values:
                c = self.good()
                c["builds"][0][field] = value
                self.assertTrue(catalog.validate(c), f"{field}={value!r} should be rejected")

    def test_prerelease_ok_and_duplicates_rejected(self):
        c = self.good()
        beta = copy.deepcopy(c["builds"][0])
        beta.update(version="0.2.0-beta.1", channel="beta")
        rev1 = copy.deepcopy(c["builds"][0])
        rev1["hardware_id"] = "waveshare-p4-wifi6-lcd5-rev1"
        c["builds"] += [beta, rev1]
        self.assertEqual(catalog.validate(c), [])
        c["builds"].append(copy.deepcopy(beta))
        self.assertTrue(any("duplicate" in e for e in catalog.validate(c)))

    def test_entry_refuses_bad_input(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                catalog.entry(build_info(d), "https://evil.example.com/x.bin", "stable")


class SiteCatalog(unittest.TestCase):
    def publish(self, site, version, variant):
        d = Path(site) / "firmware" / version
        d.mkdir(parents=True, exist_ok=True)
        (d / f"build-info-{variant}.json").write_text(json.dumps({
            "project": "summit", "version": version, "hardware_id": f"waveshare-p4-wifi6-lcd5-{variant}",
            "ota": {"bytes": 1228800, "sha256": SHA}}))

    def test_catalog_from_published_versions(self):
        with tempfile.TemporaryDirectory() as site:
            for v in ("0.1.0-alpha.2", "0.1.0-alpha.10", "0.1.0", "0.0.9"):
                for variant in ("rev1", "rev3"):
                    self.publish(site, v, variant)
            cat = catalog.build_site_catalog(site, "https://skiiirt1983.github.io/project-summit/", "alpha")
            self.assertEqual(catalog.validate(cat), [])
            self.assertEqual([b["version"] for b in cat["builds"][::2]], ["0.1.0", "0.1.0-alpha.10", "0.1.0-alpha.2", "0.0.9"])
            self.assertEqual(cat["builds"][0]["url"],
                             "https://skiiirt1983.github.io/project-summit/firmware/0.1.0/summit-0.1.0-rev3-ota.bin")
            self.assertEqual(json.loads((Path(site) / "catalog.json").read_text()), cat)
            self.assertTrue(all(b["channel"] == "alpha" for b in cat["builds"]))

    def test_a_bad_build_stops_the_catalog(self):
        with tempfile.TemporaryDirectory() as site:
            self.publish(site, "not-a-version", "rev3")
            with self.assertRaises(ValueError):
                catalog.build_site_catalog(site, "https://skiiirt1983.github.io/project-summit", "alpha")
            self.assertFalse((Path(site) / "catalog.json").exists())



if __name__ == "__main__":
    unittest.main(verbosity=1)
