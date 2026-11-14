// Low-power sleep (D-012): when the device may sleep, when it must wake for a timer
// or alarm, and what is kept across deep sleep. Pure logic, host-tested
// (tests/host/test_sleep.cpp). The deep-sleep call itself is behind SleepPort.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/timers.h"

namespace summit {

constexpr int kDefaultIdleSleepMin = 5;
constexpr uint64_t kFaceDownSleepMs = 30 * 1000;
// Wake this long before a timer or alarm is due, to cover wake-to-screen time.
// Measure on the board and adjust (HARDWARE_TODO.md).
constexpr uint64_t kDefaultWakeMarginMs = 5 * 1000;

enum class SleepBlocker {
    None,            // may sleep now
    Active,          // touched recently, not face-down long enough
    KeepAwake,       // a recipe is open with the screen-on rule (ui/keep_awake)
    Updating,        // an update is running
    Charging,        // on USB power and "stay on when charging" is on
    FaceDownUsb,     // face-down on USB power: screen off and muted, but no deep sleep (Wi-Fi and data stay fresh)
    Ringing,         // a timer or alarm is ringing
    DueSoon,         // a timer or alarm is due within the wake margin
    Disabled,        // idle timeout set to 0 (never) and not face-down
};
const char *sleep_blocker_name(SleepBlocker b);

struct SleepInput {
    uint64_t now_ms = 0;
    uint64_t last_activity_ms = 0;   // last touch (or wake)
    bool face_down = false;
    uint64_t face_down_since_ms = 0; // valid while face_down
    int idle_sleep_min = kDefaultIdleSleepMin;  // setting; 0 = never sleep when idle
    bool on_usb_power = false;
    bool stay_on_when_charging = true;          // setting
    bool keep_awake = false;
    bool updating = false;
    bool ringing = false;
    int64_t next_timer_ms = -1;      // time until the earliest running timer ends; -1 = none
    int64_t next_alarm_s = -1;       // seconds until the next alarm; -1 = none
    uint64_t wake_margin_ms = kDefaultWakeMarginMs;
};

enum class WakeFor { Nothing, Timer, Alarm };

struct SleepDecision {
    SleepBlocker blocker = SleepBlocker::Active;
    bool sleep() const { return blocker == SleepBlocker::None; }
    // Face-down for kFaceDownSleepMs (D-012 / face-down DND): chimes muted, alarms and timers still ring.
    bool dnd = false;
    // Screen off: DND, except while something rings. Independent of sleeping (on USB it never sleeps face-down).
    bool screen_off = false;
    int64_t wake_in_ms = -1;         // RTC timer wake; -1 = no timed wake (motion / button only)
    WakeFor wake_for = WakeFor::Nothing;
};

SleepDecision decide_sleep(const SleepInput &in);

// ---- What survives deep sleep (RTC memory) ----
struct SleepSnapshot {
    std::vector<SavedTimer> timers;  // running timers, remaining time at the moment of sleep
    int64_t next_alarm_s = -1;       // at the moment of sleep
    WakeFor wake_for = WakeFor::Nothing;
};

constexpr size_t kSnapshotMaxBytes = 1024;
// Fixed layout with a magic number and CRC-32; an unreadable blob is ignored.
std::vector<uint8_t> encode_snapshot(const SleepSnapshot &s);
bool decode_snapshot(const uint8_t *data, size_t size, SleepSnapshot &out);

// ---- Wake ----
enum class WakeCause { PowerOn, Timer, Motion, Button };
const char *wake_cause_name(WakeCause c);  // "power_on", "timer", "motion", "button"

// Fast path after wake (D-012): what to bring up first.
struct WakePlan {
    bool ring_first = false;   // timer/alarm wake: screen + audio and ring before Wi-Fi
    bool clock_first = false;  // pick-up / button: show the clock at once, Wi-Fi in the background
};
WakePlan wake_plan(WakeCause cause);

// The platform's deep sleep. The device implementation is untested until Phase 1.
class SleepPort {
public:
    virtual ~SleepPort() = default;
    // Stores the blob where it survives deep sleep, arms the wake sources and sleeps.
    // On the device this does not return; the next boot reports the wake through
    // wake_cause() / saved() / slept_ms().
    virtual void enter(const std::vector<uint8_t> &snapshot, int64_t wake_in_ms) = 0;
};

}  // namespace summit
