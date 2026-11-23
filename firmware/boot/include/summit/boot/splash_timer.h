// When the boot screen may end: not before a minimum time, and not before start-up is ready.
// Pure logic (no LVGL), host-tested in tests/host/test_shell_logic.cpp.
#pragma once
#include <cstdint>

namespace summit::boot {

constexpr uint32_t kSplashMinMs = 2000;  // one full loop of the animation

constexpr uint32_t kStartupMaxMs = 20000;  // never keep the boot screen longer than this waiting for the network

enum class StartupWait { Wifi, Clock, Done };

// What the boot screen is waiting for. Wi-Fi first, then the clock (SNTP); done when both are there, when the setup
// access point is on (no network is saved, so there is nothing to wait for), or after kStartupMaxMs.
inline StartupWait startup_wait(bool wifi_connected, bool time_synced, bool ap_active, uint32_t elapsed_ms)
{
    if (elapsed_ms >= kStartupMaxMs || ap_active) return StartupWait::Done;
    if (!wifi_connected) return StartupWait::Wifi;
    if (!time_synced) return StartupWait::Clock;
    return StartupWait::Done;
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
