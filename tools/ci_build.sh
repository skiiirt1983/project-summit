#!/usr/bin/env bash
# Build one chip-revision variant and merge the full USB image.
# Run from firmware/ inside the ESP-IDF environment: bash ../tools/ci_build.sh rev3|rev1
set -euo pipefail
variant="$1"
case "$variant" in rev3|rev1) ;; *) echo "unknown variant: $variant" >&2; exit 1;; esac
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.${variant}" build
# Crash addresses seen on the board (tools/crash_addrs.txt, one or more 0x... per line): print the function and the
# surrounding code from this build's ELF into the job log, so a "CRASH DETAILS: pc=0x..." line can be read without
# the ELF file. Never fails the build.
if [ -f ../tools/crash_addrs.txt ]; then
  echo "== CRASH SYMBOLS ($variant) =="
  for a in $(grep -o '0x[0-9a-fA-F]\+' ../tools/crash_addrs.txt | sort -u); do
    riscv32-esp-elf-addr2line -pfiaC -e build/summit.elf "$a" || true
  done
  for a in $(grep -o '0x[0-9a-fA-F]\+' ../tools/crash_addrs.txt | sort -u | head -2); do
    lo=$(printf '0x%x' $(( a - 48 ))); hi=$(printf '0x%x' $(( a + 24 )))
    echo "-- disassembly around $a"
    riscv32-esp-elf-objdump -d -C --start-address=$lo --stop-address=$hi build/summit.elf || true
  done
  echo "== END CRASH SYMBOLS =="
fi
# App goes to ota_0 with otadata selecting ota_0; factory stays empty (recovery app only).
args=$(python3 ../tools/make_flash_images.py build)
python3 -m esptool --chip esp32p4 merge_bin -o build/summit-full.bin -f raw $args
