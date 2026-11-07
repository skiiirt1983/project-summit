#!/usr/bin/env bash
# Host-side tests. No ESP-IDF or hardware required.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_partitions.py
python3 tests/test_firmware_image.py
python3 tests/test_hardware_id_contract.py
# C++ framework host tests (same sources as the firmware, no ESP-IDF/LVGL).
fw=firmware/framework
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
cxx="${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -pthread -I$fw/include -Itests/host"
srcs="$fw/src/value.cpp $fw/src/event_bus.cpp $fw/src/state_store.cpp $fw/src/settings.cpp $fw/src/registry.cpp $fw/src/system_settings.cpp"
for t in test_event_bus test_settings test_panels test_settings_schema; do
  $cxx $srcs tests/host/$t.cpp -o "$out/$t"
  "$out/$t"
done
