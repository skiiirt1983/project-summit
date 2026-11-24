#include "summit/alarms.h"

#include <algorithm>
#include <cstdio>

namespace summit {

namespace {
constexpr int64_t kDay = 86400;
int64_t floor_div(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
std::string clean_label(std::string s)
{
    if (s.size() > 40) s.resize(40);
    for (char &c : s)
        if (c == '\n' || c == '\r' || c == '|') c = ' ';
    return s;
}
}  // namespace

int weekday_of(int64_t local_s)
{
    const int64_t days = floor_div(local_s, kDay);
    return static_cast<int>(((days + 3) % 7 + 7) % 7);  // 1970-01-01 was a Thursday (3)
}

int seconds_of_day(int64_t local_s) { return static_cast<int>(local_s - floor_div(local_s, kDay) * kDay); }

std::string format_alarm_time(int hour, int minute)
{
    hour = hour < 0 ? 0 : hour > 23 ? 23 : hour;      // clamped so the text is always "HH:MM"
    minute = minute < 0 ? 0 : minute > 59 ? 59 : minute;
    char b[8];
    b[0] = static_cast<char>('0' + hour / 10);
    b[1] = static_cast<char>('0' + hour % 10);
    b[2] = ':';
    b[3] = static_cast<char>('0' + minute / 10);
    b[4] = static_cast<char>('0' + minute % 10);
    b[5] = '\0';
    return b;
}

bool parse_alarm_request(const std::string &text, int &hour, int &minute, int &days, std::string &label)
{
    size_t pos = 0;
    int vals[3];
    for (int i = 0; i < 3; ++i) {
        const size_t bar = text.find('|', pos);
        if (bar == std::string::npos) return false;
        const std::string num = text.substr(pos, bar - pos);
        if (num.empty() || num.size() > 3) return false;
        int v = 0;
        for (char c : num) {
            if (c < '0' || c > '9') return false;
            v = v * 10 + (c - '0');
        }
        vals[i] = v;
        pos = bar + 1;
    }
    if (vals[0] > 23 || vals[1] > 59 || vals[2] > kDaysAll) return false;
    hour = vals[0];
    minute = vals[1];
    days = vals[2];
    label = clean_label(text.substr(pos));
    return true;
}

int64_t Alarms::compute_next(const Alarm &a, int64_t from)
{
    const int64_t today = floor_div(from, kDay);
    for (int d = 0; d < 8; ++d) {
        const int64_t t = (today + d) * kDay + a.hour * 3600 + a.minute * 60;
        if (t <= from) continue;
        if (a.days != 0 && !(a.days & (1 << weekday_of(t)))) continue;
        return t;
    }
    return -1;
}

Alarm *Alarms::find_mut(int id)
{
    for (Alarm &a : alarms_)
        if (a.id == id) return &a;
    return nullptr;
}

const Alarm *Alarms::find(int id) const
{
    for (const Alarm &a : alarms_)
        if (a.id == id) return &a;
    return nullptr;
}

int Alarms::add(int hour, int minute, int days, const std::string &label, int64_t now, bool enabled)
{
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || days < 0 || days > kDaysAll || alarms_.size() >= kMax) return 0;
    Alarm a;
    a.id = next_id_++;
    a.hour = hour;
    a.minute = minute;
    a.days = days;
    a.enabled = enabled;
    a.label = clean_label(label);
    a.next_fire_s = enabled ? compute_next(a, now) : -1;
    alarms_.push_back(a);
    return a.id;
}

bool Alarms::update(int id, int hour, int minute, int days, const std::string &label, int64_t now)
{
    Alarm *a = find_mut(id);
    if (!a || hour < 0 || hour > 23 || minute < 0 || minute > 59 || days < 0 || days > kDaysAll) return false;
    a->hour = hour;
    a->minute = minute;
    a->days = days;
    a->label = clean_label(label);
    a->ringing = false;
    a->snooze_until_s = -1;
    a->next_fire_s = a->enabled ? compute_next(*a, now) : -1;
    return true;
}

bool Alarms::set_enabled(int id, bool enabled, int64_t now)
{
    Alarm *a = find_mut(id);
    if (!a) return false;
    a->enabled = enabled;
    a->ringing = false;
    a->snooze_until_s = -1;
    a->next_fire_s = enabled ? compute_next(*a, now) : -1;
    return true;
}

bool Alarms::remove(int id)
{
    auto it = std::find_if(alarms_.begin(), alarms_.end(), [id](const Alarm &a) { return a.id == id; });
    if (it == alarms_.end()) return false;
    alarms_.erase(it);
    return true;
}

void Alarms::rearm_all(int64_t now)
{
    for (Alarm &a : alarms_) {
        if (a.ringing || a.snooze_until_s >= 0) continue;
        a.next_fire_s = a.enabled ? compute_next(a, now) : -1;
    }
}

void Alarms::clock_changed(int64_t now)
{
    rearm_all(now);
    last_tick_s_ = now;
}

std::vector<int> Alarms::tick(int64_t now)
{
    std::vector<int> fired;
    if (last_tick_s_ >= 0 && now < last_tick_s_) rearm_all(now);  // the clock went back
    last_tick_s_ = now;
    for (Alarm &a : alarms_) {
        if (!a.enabled) continue;
        if (a.ringing) {
            if (now - a.ring_started_s >= kRingSeconds) {  // nobody came: stop (a once-alarm is spent)
                a.ringing = false;
                if (a.days == 0) a.enabled = false;
            }
            continue;
        }
        if (a.snooze_until_s >= 0) {
            if (now >= a.snooze_until_s) {
                a.snooze_until_s = -1;
                a.ringing = true;
                a.ring_started_s = now;
                fired.push_back(a.id);
            }
            continue;
        }
        if (a.next_fire_s < 0 || now < a.next_fire_s) continue;
        const bool late = now - a.next_fire_s > kLateSeconds;
        if (a.days == 0) a.next_fire_s = -1;  // once: not armed again
        else a.next_fire_s = compute_next(a, now);
        if (late) {  // missed while asleep: skip it quietly (a once-alarm is spent)
            if (a.days == 0) a.enabled = false;
            continue;
        }
        a.ringing = true;
        a.ring_started_s = now;
        fired.push_back(a.id);
    }
    return fired;
}

bool Alarms::dismiss()
{
    bool any = false;
    for (Alarm &a : alarms_) {
        if (!a.ringing) continue;
        any = true;
        a.ringing = false;
        if (a.days == 0) a.enabled = false;
    }
    return any;
}

bool Alarms::snooze(int64_t now, int minutes)
{
    if (minutes < 1) minutes = kSnoozeMinutes;
    bool any = false;
    for (Alarm &a : alarms_) {
        if (!a.ringing) continue;
        any = true;
        a.ringing = false;
        a.snooze_until_s = now + static_cast<int64_t>(minutes) * 60;
    }
    return any;
}

bool Alarms::ringing() const
{
    return std::any_of(alarms_.begin(), alarms_.end(), [](const Alarm &a) { return a.ringing; });
}

int Alarms::next(int64_t now, int64_t *seconds_until) const
{
    int best_id = 0;
    int64_t best = -1;
    for (const Alarm &a : alarms_) {
        if (!a.enabled || a.ringing) continue;
        const int64_t t = a.snooze_until_s >= 0 ? a.snooze_until_s : a.next_fire_s;
        if (t < 0) continue;
        if (best < 0 || t < best) {
            best = t;
            best_id = a.id;
        }
    }
    if (seconds_until) *seconds_until = best_id ? std::max<int64_t>(0, best - now) : -1;
    return best_id;
}

std::string Alarms::next_text() const
{
    int64_t best = -1;
    const Alarm *pick = nullptr;
    for (const Alarm &a : alarms_) {
        if (!a.enabled || a.ringing) continue;
        const int64_t t = a.snooze_until_s >= 0 ? a.snooze_until_s : a.next_fire_s;
        if (t < 0) continue;
        if (best < 0 || t < best) {
            best = t;
            pick = &a;
        }
    }
    if (!pick) return "";
    // A snoozed alarm shows when it rings again, not its set time.
    if (pick->snooze_until_s >= 0) {
        const int sod = seconds_of_day(pick->snooze_until_s);
        return format_alarm_time(sod / 3600, (sod % 3600) / 60);
    }
    return format_alarm_time(pick->hour, pick->minute);
}

std::string Alarms::list_text() const
{
    std::vector<const Alarm *> order;
    for (const Alarm &a : alarms_) order.push_back(&a);
    std::stable_sort(order.begin(), order.end(), [](const Alarm *x, const Alarm *y) {
        return x->hour * 60 + x->minute < y->hour * 60 + y->minute;
    });
    std::string out;
    for (const Alarm *a : order) {
        if (!out.empty()) out += '\n';
        out += std::to_string(a->id) + "|" + format_alarm_time(a->hour, a->minute) + "|" + std::to_string(a->days) + "|" +
               (a->enabled ? "1" : "0") + "|" + (a->ringing ? "1" : "0") + "|" + a->label;
    }
    return out;
}

}  // namespace summit
