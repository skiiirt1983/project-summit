// Display, touch and backlight of the Waveshare ESP32-P4-WIFI6-Touch-LCD-5 (HX8394 over MIPI-DSI,
// GT911 touch). Hardware code: compiled in CI, never run on the board until Phase 1 (HARDWARE_TODO.md).
#pragma once
#include <cstdint>

#include "lvgl.h"
#include "summit/panel_api.h"

namespace summit::bsp {

struct Handles {
    lv_display_t *display = nullptr;  // native 720x1280; rotated in software for landscape
    lv_indev_t *touch = nullptr;      // null if the GT911 was not found (the screen still works)
};

// Brings up I2C, the DSI panel, the backlight (off), touch and LVGL (tick + display + pointer).
// Returns false and logs the reason if the panel could not start. The backlight stays off until
// set_brightness() so a half-drawn first frame is never shown.
bool start(Handles &out);

// Panel strips drawn since boot (health gate: "the display is being driven").
uint32_t frames_rendered();

// Display pixel clock in MHz (shown in the diagnostics).
uint32_t pixel_clock_mhz();

// True if a device answers at this 7-bit I2C address (health gate; safe to call from any task).
bool i2c_present(uint8_t addr);

// Debug switches (settings debug/display_flush and debug/backlight), changeable at run time.
// mode: "normal" (DMA2D), "cpu_copy" (CPU copy into the frame buffer), "skip" (nothing is drawn).
void set_debug_flush(const char *mode);
void set_debug_backlight_full(bool full);

// 0..100 percent (LEDC PWM on the backlight pin).
void set_brightness(int percent);

// Landscape = the native panel rotated 90 degrees in software. The shell calls this through its
// ApplyOrientation callback and re-lays out from the display's new size.
void set_orientation(lv_display_t *display, Orientation o);

}  // namespace summit::bsp
