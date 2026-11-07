"""firmware/main/hardware_id.h must agree with tools/summit_image.py."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import summit_image as si  # noqa: E402

h = (ROOT / "firmware/main/hardware_id.h").read_text()
assert re.search(rf'#define SUMMIT_BOARD_ID "{re.escape(si.BOARD_ID)}"', h), "board id differs"
families = re.findall(r'#define SUMMIT_CHIP_REV_FAMILY "(\w+)"', h)
assert sorted(families) == sorted(si.VARIANTS), families
assert re.search(r'#if CONFIG_ESP32P4_SELECTS_REV_LESS_V3\s+#define SUMMIT_CHIP_REV_FAMILY "rev1"', h)
for v in si.VARIANTS:
    d = (ROOT / f"firmware/sdkconfig.defaults.{v}").read_text()
    assert ("CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y" in d) == (v == "rev1"), v
print("PASS: hardware ID and chip-revision variants agree across firmware, sdkconfig and tools")
