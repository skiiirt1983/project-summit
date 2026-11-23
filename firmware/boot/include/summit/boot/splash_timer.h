// When the boot screen may end: not before a minimum time, and not before start-up is ready.
// Pure logic (no LVGL), host-tested in tests/host/test_shell_logic.cpp.
#pragma once
#include <cstdint>

namespace summit::boot {

constexpr uint32_t kSplashMinMs = 2000;  // one full loop of the animation

// The boot screen covers the start-up work (clock sync, first state updates, start-up toasts) so none of it is
// seen. It stays at least kStartupSettleMs; if a network is being joined it also waits for the clock, up to
// kStartupMaxMs. It never waits for Wi-Fi as such: with no saved network the setup access point is on and the
// screen ends after the settle time.
constexpr uint32_t kStartupSettleMs = 5000;
constexpr uint32_t kStartupMaxMs = 15000;

inline bool startup_done(bool wifi_connected, bool time_synced, bool ap_active, uint32_t elapsed_ms)
{
    if (elapsed_ms < kStartupSettleMs) return false;
    if (ap_active || elapsed_ms >= kStartupMaxMs) return true;
    return wifi_connected && time_synced;
}

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
