#include "summit/shell/layout.h"

namespace summit::shell {

Tokens tokens_for(int16_t width, int16_t height)
{
    int s = width < height ? width : height;
    int unit = (s * 6 + 99) / 100;  // 6% of the short side, rounded up
    unit = (unit + 3) / 4 * 4;      // multiple of 4 so quarters stay whole
    Tokens t;
    t.unit = static_cast<int16_t>(unit);
    t.space_s = static_cast<int16_t>(unit / 4);
    t.space_m = static_cast<int16_t>(unit / 2);
    t.space_l = static_cast<int16_t>(unit);
    t.radius = static_cast<int16_t>(unit / 4);
    t.text_s = static_cast<int16_t>(unit * 9 / 25);    // ~16 on 44
    t.text_m = static_cast<int16_t>(unit * 11 / 25);   // ~20
    t.text_l = static_cast<int16_t>(unit * 18 / 25);   // ~32
    t.text_xl = static_cast<int16_t>(unit * 11 / 10);  // ~48
    return t;
}

Layout layout_for(int16_t width, int16_t height, bool strip_reserved)
{
    Layout l;
    l.orientation = width >= height ? kLandscape : kPortrait;
    l.tokens = tokens_for(width, height);
    const int16_t u = l.tokens.unit;
    l.strip = {0, 0, width, u};
    l.strip_toast = {0, 0, width, static_cast<int16_t>(2 * u)};
    if (strip_reserved) {
        l.content = {0, u, width, static_cast<int16_t>(height - u)};
    } else {
        l.content = {0, 0, width, height};
    }
    return l;
}

}  // namespace summit::shell
