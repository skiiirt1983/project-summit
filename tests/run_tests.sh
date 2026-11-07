#!/usr/bin/env bash
# Host-side tests. No ESP-IDF or hardware required.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_partitions.py
python3 tests/test_firmware_image.py
