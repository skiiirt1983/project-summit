// Motion tokens: one place for every duration, curve and gesture rule in the UI, so all
// screens move the same way. Pure logic (no LVGL), host-tested (tests/host/test_shell_logic.cpp).
// The LVGL side is summit/shell/ui_motion.h.
#pragma once
#include <cstdint>

namespace summit::shell::motion {

// Durations (ms). Things entering are slower than things leaving; small things are faster than big ones.
constexpr uint32_t kPressMs = 80;      // finger down: a control reacts at once
constexpr uint32_t kReleaseMs = 200;   // finger up: it relaxes back
constexpr uint32_t kPageMs = 320;      // page swipe settle from rest (a flick is shorter, see settle_ms)
constexpr uint32_t kSheetInMs = 300;   // quick settings down
constexpr uint32_t kSheetOutMs = 220;
constexpr uint32_t kScreenInMs = 320;  // Updates / Modules pushed in from the right
constexpr uint32_t kScreenOutMs = 240;
constexpr uint32_t kMinSettleMs = 140; // never faster than this: the eye must see where the page went
constexpr uint32_t kDotsHoldMs = 1200; // page dots stay this long after the last page change
constexpr uint32_t kDotsFadeMs = 300;

// Curves as cubic-bezier control points (x1, y1, x2, y2), 0..1024 as LVGL wants them.
struct Bezier {
    int16_t x1, y1, x2, y2;
};
constexpr Bezier kEnter = {51, 717, 102, 1024};   // (0.05, 0.7, 0.1, 1): arrives fast, lands softly
constexpr Bezier kExit = {307, 0, 819, 154};      // (0.3, 0, 0.8, 0.15): leaves with growing speed
constexpr Bezier kStandard = {205, 0, 0, 1024};   // (0.2, 0, 0, 1): moves that start and end on screen

// Dragging past the first / last page: the page follows the finger less and less, never more
// than about half of `extent`. Sign follows `overscroll`.
int rubber_band(int overscroll, int extent);

// Time to finish a swipe from where the finger let go: `distance_px` still to travel at the
// finger's `velocity_px_per_ms`, so a flick keeps its speed instead of restarting. Between
// kMinSettleMs and `base_ms`.
uint32_t settle_ms(int distance_px, float velocity_px_per_ms, uint32_t base_ms = kPageMs);

// Page dots opacity (0..255) `since_ms` after the last page change or drag.
uint8_t dots_opacity(uint32_t since_ms);

}  // namespace summit::shell::motion
