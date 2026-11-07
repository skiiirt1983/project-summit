#!/usr/bin/env python3
"""Write BUILD_DIR/otadata-ota0.bin and print esptool merge_bin arguments.

Usage: make_flash_images.py BUILD_DIR

The full USB image is merged from: bootloader, partition table, otadata that
selects ota_0, and the app at ota_0. idf.py's own merge-bin would put the app in
factory, which is reserved for the recovery app.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import summit_image as si  # noqa: E402


def merge_args(build):
    build = Path(build)
    parts = si.read_partitions()
    flasher = json.loads((build / "flasher_args.json").read_text())
    boot = flasher["bootloader"]
    table = flasher["partition-table"]
    if int(boot["offset"], 0) != si.BOOTLOADER_OFFSET:
        raise si.ImageError(f"bootloader offset {boot['offset']} != {si.BOOTLOADER_OFFSET:#x}")
    if int(table["offset"], 0) != si.TABLE_OFFSET:
        raise si.ImageError(f"partition table offset {table['offset']} != {si.TABLE_OFFSET:#x}")
    otadata = build / "otadata-ota0.bin"
    otadata.write_bytes(si.otadata_selecting_ota0(parts))
    return [
        hex(si.BOOTLOADER_OFFSET), str(build / boot["file"]),
        hex(si.TABLE_OFFSET), str(build / table["file"]),
        hex(parts["otadata"][2]), str(otadata),
        hex(parts[si.MAIN_APP_SLOT][2]), str(build / f"{si.PROJECT}.bin"),
    ]


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_flash_images.py BUILD_DIR")
    print(" ".join(merge_args(sys.argv[1])))
