// ESP32-P4 hardware JPEG decoder as an LVGL image decoder (compiled in CI; untested on hardware: HARDWARE_TODO.md).
// Pictures are LVGL file images "S:/recipes/<id>/pages/001.jpg" (LVGL's POSIX driver maps S: to /sdcard). Baseline
// JPEGs are decoded by the engine into RGB565 in PSRAM and kept in LVGL's image cache; anything the engine cannot do
// (progressive, huge, no memory) fails with a reason in the log and the panel shows its "images unavailable" message.
#pragma once
#include <string>

namespace summit::jpeg_hw {

// Call once after lv_init() and the display are up. Creates the engine, registers the LVGL decoder and decodes a tiny
// built-in picture to check the colour order (it flips the engine's red/blue order by itself if the first try is
// wrong). Returns false and logs why if the engine is unusable; images then stay unavailable.
bool start();
bool usable();

// Lines for the diag/metrics text: jpeg_state, jpeg_decodes, jpeg_fails, jpeg_last_ms, jpeg_last_size, jpeg_last_error.
std::string diag_lines();

}  // namespace summit::jpeg_hw
