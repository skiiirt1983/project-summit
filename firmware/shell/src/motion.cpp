#include "summit/shell/motion.h"

#include <cmath>
#include <cstdlib>

namespace summit::shell::motion {

int rubber_band(int overscroll, int extent)
{
    if (extent <= 0 || overscroll == 0) return 0;
    // x * d * c / (d + c * x), c = 0.55: starts at about half the finger's speed and flattens.
    const double x = std::abs(overscroll), d = extent, c = 0.55;
    const int out = static_cast<int>(x * d * c / (d + c * x));
    return overscroll < 0 ? -out : out;
}

uint32_t settle_ms(int distance_px, float velocity_px_per_ms, uint32_t base_ms)
{
    const int dist = std::abs(distance_px);
    if (dist == 0) return kMinSettleMs < base_ms ? kMinSettleMs : base_ms;
    const float v = std::fabs(velocity_px_per_ms);
    // A decelerating curve covers the distance at about twice its average speed at the start,
    // so 2 * distance / velocity keeps the first frames at the finger's speed.
    uint32_t ms = base_ms;
    if (v > 0.05f) {
        const float t = 2.0f * static_cast<float>(dist) / v;
        if (t < static_cast<float>(base_ms)) ms = static_cast<uint32_t>(t);
    }
    if (ms < kMinSettleMs) ms = kMinSettleMs;
    return ms > base_ms ? base_ms : ms;
}

uint8_t dots_opacity(uint32_t since_ms)
{
    if (since_ms <= kDotsHoldMs) return 255;
    const uint32_t f = since_ms - kDotsHoldMs;
    if (f >= kDotsFadeMs) return 0;
    return static_cast<uint8_t>(255 - 255 * f / kDotsFadeMs);
}

}  // namespace summit::shell::motion
