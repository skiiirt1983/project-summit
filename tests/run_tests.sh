#!/usr/bin/env bash
# Host-side tests. No ESP-IDF or hardware required.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_partitions.py
python3 tests/test_firmware_image.py
python3 tests/test_hardware_id_contract.py
python3 tests/test_state_topics_contract.py
python3 tests/test_catalog_tool.py
node tests/web/test_recipe_import.mjs   # recipe text parser used by the web import
# C++ framework host tests (same sources as the firmware, no ESP-IDF/LVGL).
fw=firmware/framework
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
sh=firmware/shell
up=firmware/services/update
wb=firmware/web
rc=firmware/panels/recipes
tm=firmware/services/timers
cxx="${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -pthread -I$fw/include -I$sh/include -I$up/include -I$wb/include -I$rc/include -I$tm/include -Ifirmware/services/audio/include -Ifirmware/services/jpeg/include -Ifirmware/services/power/include -Ifirmware/services/net/include -Ifirmware/boot/include -Ithird_party/cjson -Itests/host"
srcs="$fw/src/value.cpp $fw/src/event_bus.cpp $fw/src/state_store.cpp $fw/src/settings.cpp $fw/src/registry.cpp $fw/src/system_settings.cpp $fw/src/json_writer.cpp $fw/src/modules.cpp $fw/src/storage.cpp"
shell_srcs="$sh/src/layout.cpp $sh/src/glance.cpp $sh/src/strip_state.cpp $sh/src/rotation.cpp $sh/src/pager.cpp $sh/src/motion.cpp"
update_srcs="$up/src/version.cpp $up/src/catalog.cpp $up/src/preflight.cpp $up/src/image_check.cpp $up/src/engine.cpp $up/src/health_gate.cpp $up/src/status_topics.cpp"
${CC:-gcc} -std=c99 -g -fsanitize=address,undefined -c third_party/cjson/cJSON.c -o "$out/cJSON.o"
web_srcs="$wb/src/api.cpp $wb/src/log_buffer.cpp $wb/src/log_mirror.cpp $out/cJSON.o"
recipe_srcs="$rc/src/module.cpp $rc/src/recipe.cpp $rc/src/quantity.cpp $rc/src/step_time.cpp $rc/src/store.cpp $rc/src/cook.cpp $rc/src/web_api.cpp $rc/src/jsonld.cpp"
# Every library source is compiled once (in parallel) and linked into each test: compiling the same ~60 sources for each
# of the 20 tests took longer than the CI job's time limit.
srcs_all="$srcs $shell_srcs $update_srcs $wb/src/api.cpp $wb/src/log_buffer.cpp $wb/src/log_mirror.cpp $recipe_srcs $tm/src/timers.cpp $tm/src/stopwatch.cpp $tm/src/alarms.cpp $tm/src/clock_tools.cpp firmware/services/audio/src/audio.cpp firmware/services/audio/src/sounds_data.cpp firmware/services/jpeg/src/jpeg_info.cpp firmware/services/power/src/sleep.cpp firmware/services/power/src/power_service.cpp"
mkdir -p "$out/obj"
objs=""
i=0
: > "$out/compile_list.txt"
for s in $srcs_all; do
  i=$((i + 1))
  objs="$objs $out/obj/$i.o"
  echo "$s $out/obj/$i.o" >> "$out/compile_list.txt"
done
xargs -P "$(nproc 2>/dev/null || echo 2)" -L 1 sh -c '$0 -c "$1" -o "$2"' "$cxx" < "$out/compile_list.txt" || { echo "compile failed"; exit 1; }
for t in test_event_bus test_settings test_panels test_settings_schema test_shell_logic test_update test_web_api test_modules test_recipes test_storage test_timers test_recipe_api test_jsonld test_sleep test_net_policy test_log_mirror test_audio test_jpeg_info test_clock_tools test_clock_service; do
  $cxx $objs "$out/cJSON.o" tests/host/$t.cpp -o "$out/$t"
  "$out/$t"
done
