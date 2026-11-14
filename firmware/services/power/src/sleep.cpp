#include "summit/sleep.h"

#include <cstring>

namespace summit {

const char *sleep_blocker_name(SleepBlocker b)
{
    switch (b) {
        case SleepBlocker::None: return "";
        case SleepBlocker::Active: return "active";
        case SleepBlocker::KeepAwake: return "keep_awake";
        case SleepBlocker::Updating: return "updating";
        case SleepBlocker::Charging: return "charging";
        case SleepBlocker::Ringing: return "ringing";
        case SleepBlocker::DueSoon: return "due_soon";
        case SleepBlocker::Disabled: return "disabled";
    }
    return "";
}

SleepDecision decide_sleep(const SleepInput &in)
{
    SleepDecision d;
    // Hard blockers first: these hold even when face-down.
    if (in.updating) return d.blocker = SleepBlocker::Updating, d;
    if (in.ringing) return d.blocker = SleepBlocker::Ringing, d;
    if (in.keep_awake) return d.blocker = SleepBlocker::KeepAwake, d;
    if (in.on_usb_power && in.stay_on_when_charging) return d.blocker = SleepBlocker::Charging, d;

    const bool face_down_long =
        in.face_down && in.now_ms >= in.face_down_since_ms && in.now_ms - in.face_down_since_ms >= kFaceDownSleepMs;
    const uint64_t idle_ms = in.now_ms >= in.last_activity_ms ? in.now_ms - in.last_activity_ms : 0;
    const bool idle_long =
        in.idle_sleep_min > 0 && idle_ms >= static_cast<uint64_t>(in.idle_sleep_min) * 60 * 1000;
    if (!face_down_long && !idle_long)
        return d.blocker = in.idle_sleep_min > 0 || in.face_down ? SleepBlocker::Active : SleepBlocker::Disabled, d;

    // Earliest due item decides the timed wake.
    int64_t due_ms = -1;
    WakeFor what = WakeFor::Nothing;
    if (in.next_timer_ms >= 0) {
        due_ms = in.next_timer_ms;
        what = WakeFor::Timer;
    }
    if (in.next_alarm_s >= 0 && (due_ms < 0 || in.next_alarm_s * 1000 < due_ms)) {
        due_ms = in.next_alarm_s * 1000;
        what = WakeFor::Alarm;
    }
    if (due_ms >= 0) {
        const int64_t margin = static_cast<int64_t>(in.wake_margin_ms);
        if (due_ms <= margin) return d.blocker = SleepBlocker::DueSoon, d;  // not worth sleeping
        d.wake_in_ms = due_ms - margin;
        d.wake_for = what;
    }
    d.blocker = SleepBlocker::None;
    return d;
}

// ---- snapshot ----
namespace {

constexpr uint32_t kMagic = 0x31504C53;  // "SLP1"
constexpr size_t kNameBytes = 80;

uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

void put(std::vector<uint8_t> &b, uint64_t v, int bytes)
{
    for (int i = 0; i < bytes; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

struct Reader {
    const uint8_t *p;
    size_t n, at = 0;
    bool ok = true;
    uint64_t get(int bytes)
    {
        if (at + bytes > n) return ok = false, 0;
        uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) v |= static_cast<uint64_t>(p[at + i]) << (8 * i);
        at += bytes;
        return v;
    }
};

}  // namespace

std::vector<uint8_t> encode_snapshot(const SleepSnapshot &s)
{
    std::vector<uint8_t> b;
    put(b, kMagic, 4);
    const size_t count = s.timers.size() > Timers::kMax ? Timers::kMax : s.timers.size();
    put(b, count, 1);
    put(b, static_cast<uint8_t>(s.wake_for), 1);
    put(b, static_cast<uint64_t>(s.next_alarm_s), 8);
    for (size_t i = 0; i < count; ++i) {
        const SavedTimer &t = s.timers[i];
        put(b, static_cast<uint32_t>(t.total_s), 4);
        put(b, t.remaining_ms, 8);
        const size_t len = t.name.size() > kNameBytes ? kNameBytes : t.name.size();
        put(b, len, 1);
        b.insert(b.end(), t.name.begin(), t.name.begin() + len);
    }
    put(b, crc32(b.data(), b.size()), 4);
    return b;
}

bool decode_snapshot(const uint8_t *data, size_t size, SleepSnapshot &out)
{
    if (!data || size < 18 || size > kSnapshotMaxBytes) return false;
    Reader r{data, size - 4};
    if (r.get(4) != kMagic) return false;
    Reader tail{data + size - 4, 4};
    if (tail.get(4) != crc32(data, size - 4)) return false;
    SleepSnapshot s;
    const size_t count = r.get(1);
    const uint64_t wake_for = r.get(1);
    s.next_alarm_s = static_cast<int64_t>(r.get(8));
    if (count > Timers::kMax || wake_for > static_cast<uint64_t>(WakeFor::Alarm)) return false;
    s.wake_for = static_cast<WakeFor>(wake_for);
    for (size_t i = 0; i < count; ++i) {
        SavedTimer t;
        t.total_s = static_cast<int>(r.get(4));
        t.remaining_ms = r.get(8);
        const size_t len = r.get(1);
        if (!r.ok || len > kNameBytes || r.at + len > r.n) return false;
        t.name.assign(reinterpret_cast<const char *>(data + r.at), len);
        r.at += len;
        if (t.total_s < 1 || t.total_s > Timers::kMaxSeconds ||
            t.remaining_ms > static_cast<uint64_t>(Timers::kMaxSeconds) * 1000)
            return false;
        s.timers.push_back(t);
    }
    if (!r.ok || r.at != r.n) return false;
    out = s;
    return true;
}

const char *wake_cause_name(WakeCause c)
{
    switch (c) {
        case WakeCause::PowerOn: return "power_on";
        case WakeCause::Timer: return "timer";
        case WakeCause::Motion: return "motion";
        case WakeCause::Button: return "button";
    }
    return "power_on";
}

WakePlan wake_plan(WakeCause cause)
{
    WakePlan p;
    p.ring_first = cause == WakeCause::Timer;
    p.clock_first = cause == WakeCause::Motion || cause == WakeCause::Button;
    return p;
}

}  // namespace summit
