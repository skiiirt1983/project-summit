// Hardware ID = board + ESP32-P4 chip-revision family.
// The update engine installs only catalog entries whose hardware_id matches.
// Must match tools/summit_image.py (guarded by tests/test_hardware_id_contract.py).
#pragma once
#include "sdkconfig.h"

#define SUMMIT_BOARD_ID "waveshare-p4-wifi6-lcd5"

#if CONFIG_ESP32P4_SELECTS_REV_LESS_V3
#define SUMMIT_CHIP_REV_FAMILY "rev1"
#else
#define SUMMIT_CHIP_REV_FAMILY "rev3"
#endif

#define SUMMIT_HARDWARE_ID SUMMIT_BOARD_ID "-" SUMMIT_CHIP_REV_FAMILY
