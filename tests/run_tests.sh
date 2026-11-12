#!/usr/bin/env bash
# Host-side tests. No ESP-IDF or hardware required.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_partitions.py
python3 tests/test_firmware_image.py
python3 tests/test_hardware_id_contract.py
python3 tests/test_state_topics_contract.py
python3 tests/test_catalog_tool.py
# C++ framework host tests (same sources as the firmware, no ESP-IDF/LVGL).
fw=firmware/framework
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
sh=firmware/shell
up=firmware/services/update
wb=firmware/web
rc=firmware/panels/recipes
cxx="${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -pthread -I$fw/include -I$sh/include -I$up/include -I$wb/include -I$rc/include -Ithird_party/cjson -Itests/host"
srcs="$fw/src/value.cpp $fw/src/event_bus.cpp $fw/src/state_store.cpp $fw/src/settings.cpp $fw/src/registry.cpp $fw/src/system_settings.cpp $fw/src/json_writer.cpp $fw/src/modules.cpp"
shell_srcs="$sh/src/layout.cpp $sh/src/glance.cpp $sh/src/strip_state.cpp $sh/src/rotation.cpp $sh/src/pager.cpp"
update_srcs="$up/src/version.cpp $up/src/catalog.cpp $up/src/preflight.cpp $up/src/image_check.cpp $up/src/engine.cpp $up/src/health_gate.cpp $up/src/status_topics.cpp"
${CC:-gcc} -std=c99 -g -fsanitize=address,undefined -c third_party/cjson/cJSON.c -o "$out/cJSON.o"
web_srcs="$wb/src/api.cpp $out/cJSON.o"
recipe_srcs="$rc/src/module.cpp $rc/src/recipe.cpp $rc/src/quantity.cpp $rc/src/step_time.cpp"
for t in test_event_bus test_settings test_panels test_settings_schema test_shell_logic test_update test_web_api test_modules test_recipes; do
  $cxx $srcs $shell_srcs $update_srcs $web_srcs $recipe_srcs tests/host/$t.cpp -o "$out/$t"
  "$out/$t"
done
