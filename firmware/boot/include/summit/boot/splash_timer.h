// When the boot screen may end: not before a minimum time, and not before start-up is ready.
// Pure logic (no LVGL), host-tested in tests/host/test_shell_logic.cpp.
#pragma once
#include <cstdint>

namespace summit::boot {

constexpr uint32_t kSplashMinMs = 2000;  // one full loop of the animation

class SplashTimer {
public:
    void start(uint32_t now_ms) { start_ms_ = now_ms; started_ = true; }
    void set_ready() { ready_ = true; }
    bool started() const { return started_; }
    bool ready() const { return ready_; }
    // True once start-up is ready AND the minimum time has passed (millisecond counter may wrap).
    bool done(uint32_t now_ms) const { return started_ && ready_ && now_ms - start_ms_ >= kSplashMinMs; }

private:
    uint32_t start_ms_ = 0;
    bool started_ = false, ready_ = false;
};

}  // namespace summit::boot
