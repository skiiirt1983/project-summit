// Flash and PSRAM diagnostics for the "Image validation failed" hunt (alpha.38, 2026-11-24). Compiled in CI; never run
// on the board until Paul flashes it (docs/HARDWARE_TODO.md).
//
// Background: every image up to 3.95 MB installed fine; alpha.38 (4.41 MB) is the first that reaches past 16 MB in the
// ota_1 slot (0xC20000 + 4,063,232 = 0x1000000) and fails verification every time, with the download itself passing
// its SHA-256. The chip is a 32 MB GigaDevice (id c84019): access above 16 MB uses 4-byte addresses. Not proven to be
// the cause (a PSRAM buffer above 4 MB is the other coincidence), so this measures both.
#pragma once
#include <string>

#include "summit/event_bus.h"

namespace summit::update::flashcheck {

// Subscribes to the bus event diag/flash_test: runs the self-test in its own short task (the flash erase stalls the
// display for a moment). The self-test only touches the unused "assets" partition (0x1420000..0x1FFFFFF) and 5 MB of
// PSRAM; it never touches an app slot, settings or the bootloader.
void start(EventBus &bus);

// "flash_selftest=...\nflash_last_verify=...\n" for the diag/metrics text (flash_last_verify says where a failed image
// check found the flash different from the image: see esp/flash_check.h).
std::string diag_lines();

}  // namespace summit::update::flashcheck
