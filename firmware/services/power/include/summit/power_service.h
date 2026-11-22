// Feeds the sleep rules (sleep.h) from the bus, state store and settings, and puts
// the device to sleep through a SleepPort. Pure; host-tested with a fake port.
#pragma once
#include <cstdint>
#include <vector>

#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/sleep.h"
#include "summit/state_store.h"
#include "summit/timers.h"

namespace summit {

namespace power_topics {
constexpr const char *kActivity = "input/activity";         // event: a touch (published by the shell)
constexpr const char *kFaceDown = "sensor/face_down";       // bool, IMU service
constexpr const char *kSleepBlocker = "power/sleep_blocker";  // string: why the device is awake; "" = about to sleep
constexpr const char *kDnd = "power/dnd";                  // bool: face-down, chimes muted (alarms still ring)
constexpr const char *kScreenOff = "power/screen_off";      // bool: screen should be off (face-down, nothing ringing)
constexpr const char *kWakeCause = "power/wake_cause";      // string: power_on | timer | motion | button
constexpr const char *kIdleSleepMin = "power/idle_sleep_min";      // setting (system_settings.cpp)
constexpr const char *kStayOnCharging = "power/stay_on_charging";  // setting
}  // namespace power_topics

// A recipe may hold the screen on for this long without a touch (ARCHITECTURE.md: ui/keep_awake).
constexpr uint64_t kKeepAwakeLimitMs = 30 * 60 * 1000;

class PowerService {
public:
    PowerService(EventBus &bus, StateStore &state, Settings &settings, TimerService &timers, SleepPort &port);
    ~PowerService();
    PowerService(const PowerService &) = delete;
    PowerService &operator=(const PowerService &) = delete;

    // Once at start-up, before the first tick. `blob` is what the port kept across
    // deep sleep (may be null); `slept_ms` is how long the device was asleep.
    void on_wake(WakeCause cause, const uint8_t *blob, size_t size, uint64_t slept_ms, uint64_t now_ms);
    // From the UI loop, after bus.dispatch() and the timer service's tick.
    void tick(uint64_t now_ms);

    SleepInput input(uint64_t now_ms) const;  // what the rules see right now
    bool asleep() const { return asleep_; }   // true after enter() until on_wake() (simulator / tests)
    void set_wake_margin_ms(uint64_t ms) { wake_margin_ms_ = ms; }

private:
    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    TimerService &timers_;
    SleepPort &port_;
    std::vector<EventBus::SubscriptionId> subs_;
    uint64_t now_ms_ = 0, last_activity_ms_ = 0, face_down_since_ms_ = 0, last_check_ms_ = 0;
    uint64_t wake_margin_ms_ = kDefaultWakeMarginMs;
    bool face_down_ = false, asleep_ = false, started_ = false;
};

}  // namespace summit
