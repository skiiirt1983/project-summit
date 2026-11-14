// ESP32-P4 deep sleep behind SleepPort (D-012). Device only.
// UNTESTED ON HARDWARE: compiled in CI, never run. See docs/HARDWARE_TODO.md.
#pragma once
#include <cstdint>
#include <vector>

#include "summit/sleep.h"

namespace summit {

// What the last deep sleep left behind; read once at boot.
struct BootWake {
    WakeCause cause = WakeCause::PowerOn;
    std::vector<uint8_t> blob;  // snapshot from RTC memory (empty after a cold start)
    uint64_t slept_ms = 0;
};
BootWake read_boot_wake();

class DeepSleepPort : public SleepPort {
public:
    // `motion_gpio`: LIS2DW12 INT1, an LP GPIO (active high). -1 = no motion wake.
    explicit DeepSleepPort(int motion_gpio) : motion_gpio_(motion_gpio) {}
    // Keeps the snapshot in RTC memory, arms the RTC timer (wake_in_ms >= 0) and the
    // motion pin, then enters deep sleep. Does not return.
    // Still to do on hardware: turn off the display, backlight, audio and the C6 first,
    // and arm the power button once its pin is known.
    void enter(const std::vector<uint8_t> &snapshot, int64_t wake_in_ms) override;

private:
    int motion_gpio_;
};

}  // namespace summit
