#!/usr/bin/env bash
# Host-side tests. No ESP-IDF or hardware required.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_partitions.py
python3 tests/test_firmware_image.py
python3 tests/test_hardware_id_contract.py
python3 tests/test_state_topics_contract.py
# C++ framework host tests (same sources as the firmware, no ESP-IDF/LVGL).
fw=firmware/framework
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
sh=firmware/shell
up=firmware/services/update
cxx="${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -pthread -I$fw/include -I$sh/include -I$up/include -Itests/host"
srcs="$fw/src/value.cpp $fw/src/event_bus.cpp $fw/src/state_store.cpp $fw/src/settings.cpp $fw/src/registry.cpp $fw/src/system_settings.cpp"
shell_srcs="$sh/src/layout.cpp $sh/src/glance.cpp $sh/src/strip_state.cpp $sh/src/rotation.cpp $sh/src/pager.cpp"
update_srcs="$up/src/version.cpp $up/src/catalog.cpp $up/src/preflight.cpp $up/src/image_check.cpp $up/src/engine.cpp $up/src/health_gate.cpp"
for t in test_event_bus test_settings test_panels test_settings_schema test_shell_logic test_update; do
  $cxx $srcs $shell_srcs $update_srcs tests/host/$t.cpp -o "$out/$t"
  "$out/$t"
done
