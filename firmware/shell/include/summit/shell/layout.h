// Size tokens and screen layout. Pure logic (no LVGL), host-tested.
// Everything on screen is sized from tokens, never fixed pixels (hard rule 3).
#pragma once
#include <cstdint>

#include "summit/panel_api.h"

namespace summit::shell {

using Tokens = SizeTokens;

// Tokens derive from the short side so they are identical in both orientations.
Tokens tokens_for(int16_t width, int16_t height);

struct Rect {
    int16_t x, y, w, h;
};

struct Layout {
    Orientation orientation;
    Tokens tokens;
    Rect strip;        // glance strip (collapsed height = one token)
    Rect strip_toast;  // strip expanded for a toast (two tokens)
    Rect content;      // where the current panel draws: never under a visible strip
};

// strip_reserved=false (strip hidden for full-screen modes): content uses the
// whole screen and the strip, if revealed, overlays it.
Layout layout_for(int16_t width, int16_t height, bool strip_reserved = true);

}  // namespace summit::shell
