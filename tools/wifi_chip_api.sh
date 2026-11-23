#!/usr/bin/env bash
# Run from firmware/ inside the ESP-IDF environment (the "Wi-Fi chip firmware" workflow, job host-api).
# Resolves the P4 project's managed components and prints the esp_hosted host-side API for the co-processor version
# and the slave OTA, so the device code is written against the real headers. Never fails the job on a missing match.
set -uo pipefail
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.rev3" reconfigure || { echo "reconfigure failed"; exit 1; }
ls managed_components | grep -i hosted || echo "no hosted component directory found"
d=$(ls -d managed_components/*esp_hosted* 2>/dev/null | head -1)
echo "== component dir: ${d:-none} =="
[ -n "$d" ] || exit 0
echo "== esp_hosted version =="; grep -m3 -E "^version|^name" "$d/idf_component.yml" || true
echo "== OTA and version API (headers) =="
grep -rn -E "esp_hosted_slave_ota|esp_hosted_get_coprocessor|esp_hosted_coprocessor_fwver|slave_ota" "$d" --include=*.h | head -60 || true
echo "== OTA example files =="; ls "$d/examples" 2>/dev/null | head -20 || true
echo "== the header that declares esp_hosted_slave_ota_begin =="
h=$(grep -rl "esp_hosted_slave_ota_begin" "$d" --include=*.h | head -1)
echo "${h:-not found}"
[ -n "$h" ] && sed -n '1,200p' "$h" | grep -n -E "esp_hosted_slave_ota|esp_hosted_get_coprocessor|typedef|struct|#include|@brief|@param|@return" | head -80 || true
echo "== co-processor version and app desc structs =="
grep -n -B8 "esp_hosted_coprocessor_fwver_t;" "$d/host/api/include/esp_hosted_api_types.h" | head -30 || true
grep -rn -B12 "} esp_hosted_app_desc_t" "$d/host" --include=*.h | head -30 || true
