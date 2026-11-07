#!/usr/bin/env bash
# Build one chip-revision variant and merge the full USB image.
# Run from firmware/ inside the ESP-IDF environment: bash ../tools/ci_build.sh rev3|rev1
set -euo pipefail
variant="$1"
case "$variant" in rev3|rev1) ;; *) echo "unknown variant: $variant" >&2; exit 1;; esac
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.${variant}" build
# App goes to ota_0 with otadata selecting ota_0; factory stays empty (recovery app only).
args=$(python3 ../tools/make_flash_images.py build)
python3 -m esptool --chip esp32p4 merge_bin -o build/summit-full.bin -f raw $args
