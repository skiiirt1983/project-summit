// Alarm clock (Paul, 2026-11-24): alarms at a time of day, with repeat days and snooze, ringing until dismissed (or
// snoozed). Pure logic on local wall-clock seconds (seconds since 1970-01-01 00:00 in the device's local time, so the
// time zone is the caller's business); host-tested in tests/host/test_clock_tools.cpp. The service, topics, tray item
// and screen come in later steps.
//
// Choices (change them here if Paul wants otherwise): up to 8 alarms; an alarm with no repeat days fires once at the
// next occurrence of its time and then switches itself off when dismissed; setting an alarm for the minute that is
// already running means tomorrow; an alarm that is more than 2 minutes late (the device was asleep or the clock jumped)
// is skipped, not fired; the clock going back re-arms everything (the service calls clock_changed() on a time sync); a ringing alarm that nobody touches stops after 5 minutes; snooze is 9 minutes by default.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace summit {

// Repeat days as a bit mask: bit 0 = Monday ... bit 6 = Sunday; 0 = once.
constexpr int kDaysAll = 0x7F;
constexpr int kDaysWeekdays = 0x1F;
constexpr int kDaysWeekend = 0x60;

int weekday_of(int64_t local_s);        // 0 = Monday ... 6 = Sunday
int seconds_of_day(int64_t local_s);    // 0..86399

struct Alarm {
    int id = 0;
    int hour = 0;     // 0..23
    int minute = 0;   // 0..59
    int days = 0;     // repeat mask, 0 = once
    bool enabled = true;
    std::string label;
    // Runtime state.
    int64_t next_fire_s = -1;     // when it rings next (-1 = not armed)
    bool ringing = false;
    int64_t ring_started_s = -1;
    int64_t snooze_until_s = -1;  // >= 0: snoozed, rings again then
};

class Alarms {
public:
    static constexpr size_t kMax = 8;
    static constexpr int kRingSeconds = 5 * 60;
    static constexpr int kLateSeconds = 120;
    static constexpr int kSnoozeMinutes = 9;

    // 0 if refused (bad time or too many alarms). Armed from `now_local_s`.
    int add(int hour, int minute, int days, const std::string &label, int64_t now_local_s, bool enabled = true);
    bool update(int id, int hour, int minute, int days, const std::string &label, int64_t now_local_s);
    bool set_enabled(int id, bool enabled, int64_t now_local_s);
    bool remove(int id);

    // Call a few times a second with the local time. Returns the ids that started ringing in this call. An alarm that
    // came due while the device slept rings if the first tick after waking is within 2 minutes, else it is skipped.
    std::vector<int> tick(int64_t now_local_s);
    // The clock was set or the time zone changed (time sync, settings): re-arms every alarm without firing any.
    void clock_changed(int64_t now_local_s);
    // Stops every ringing alarm; one without repeat days switches itself off. True if one was ringing.
    bool dismiss();
    // Silences every ringing alarm; it rings again after `minutes`. True if one was ringing.
    bool snooze(int64_t now_local_s, int minutes = kSnoozeMinutes);

    bool ringing() const;
    const std::vector<Alarm> &all() const { return alarms_; }
    const Alarm *find(int id) const;
    // The soonest enabled alarm (or snooze): its id (0 if none) and the seconds until it rings.
    int next(int64_t now_local_s, int64_t *seconds_until = nullptr) const;
    // Tray text: the next alarm's time "07:30" ("" if none).
    std::string next_text() const;
    // One line per alarm, by time of day: "<id>|<HH:MM>|<days>|<0/1 enabled>|<0/1 ringing>|<label>".
    std::string list_text() const;

private:
    static int64_t compute_next(const Alarm &a, int64_t from_local_s);
    void rearm_all(int64_t now_local_s);
    Alarm *find_mut(int id);

    std::vector<Alarm> alarms_;
    int next_id_ = 1;
    int64_t last_tick_s_ = -1;
};

std::string format_alarm_time(int hour, int minute);  // "07:30"
// "<hour>|<minute>|<days>|<label>" (the alarm/set payload for a new alarm); false if out of range.
bool parse_alarm_request(const std::string &text, int &hour, int &minute, int &days, std::string &label);

}  // namespace summit
