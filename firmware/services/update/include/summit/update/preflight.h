// Pre-flight before any update (ARCHITECTURE.md "Summit additions" 3). Pure logic.
#pragma once
#include <cstdint>
#include <string>

namespace summit::update {

struct PreflightInput {
    bool on_usb_power = false;
    int battery_pct = 0;
    int64_t next_alarm_in_s = -1;  // -1 = no alarm set
    bool wifi_connected = false;
    bool time_synced = false;      // needed for TLS certificate checks
    int64_t image_size = 0;
    int64_t slot_size = 0;
    bool timer_running = false;    // a recipe timer is running or ringing (D-011)
    bool from_network = true;      // catalog download (false = web upload over the LAN)
};

enum PreflightFail : uint32_t {
    kNeedPower = 1,    // not on USB power and battery below 40%
    kAlarmSoon = 2,    // an alarm within 15 minutes
    kNoWifi = 4,
    kNoTime = 8,       // clock not synced (catalog download only)
    kDoesNotFit = 16,  // image larger than the spare slot (or empty)
    kTimerRunning = 32,  // a recipe timer would be lost by the restart
};

constexpr int kMinBatteryPct = 40;
constexpr int64_t kAlarmGuardS = 15 * 60;

// Going back to the previous version restarts the device too: "" if allowed, else why not.
std::string previous_version_blocker(const PreflightInput &in);

// 0 = all clear; otherwise PreflightFail bits.
uint32_t preflight(const PreflightInput &in);
// Human-readable first failing reason (for the Updates screen and web UI).
std::string preflight_message(uint32_t failures);

}  // namespace summit::update
