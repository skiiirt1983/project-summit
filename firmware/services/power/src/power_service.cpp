#include "summit/power_service.h"

namespace summit {

using namespace power_topics;

namespace {

bool as_bool(const Value &v)
{
    const bool *b = std::get_if<bool>(&v);
    return b && *b;
}

int64_t as_int(const Value &v, int64_t fallback)
{
    const int64_t *i = std::get_if<int64_t>(&v);
    return i ? *i : fallback;
}

}  // namespace

PowerService::PowerService(EventBus &bus, StateStore &state, Settings &settings, TimerService &timers, SleepPort &port)
    : bus_(bus), state_(state), settings_(settings), timers_(timers), port_(port)
{
    auto activity = [this](const std::string &, const Value &) { last_activity_ms_ = now_ms_; };
    subs_.push_back(bus_.subscribe(kActivity, activity));
    subs_.push_back(bus_.subscribe("input/double_tap", activity));
    subs_.push_back(bus_.subscribe(kFaceDown, [this](const std::string &, const Value &v) {
        const bool down = as_bool(v);
        if (down && !face_down_) face_down_since_ms_ = now_ms_;
        if (!down && face_down_) last_activity_ms_ = now_ms_;  // turned back over
        face_down_ = down;
    }));
    state_.set(kWakeCause, std::string(wake_cause_name(WakeCause::PowerOn)));
    state_.set(kSleepBlocker, std::string(sleep_blocker_name(SleepBlocker::Active)));
}

PowerService::~PowerService()
{
    for (auto id : subs_) bus_.unsubscribe(id);
}

void PowerService::on_wake(WakeCause cause, const uint8_t *blob, size_t size, uint64_t slept_ms, uint64_t now_ms)
{
    now_ms_ = last_activity_ms_ = last_check_ms_ = now_ms;
    asleep_ = false;
    started_ = true;
    face_down_ = as_bool(state_.get(kFaceDown));
    face_down_since_ms_ = now_ms;
    SleepSnapshot snap;
    // A missing or damaged blob (first power-on, battery pulled) restores nothing.
    if (cause != WakeCause::PowerOn && decode_snapshot(blob, size, snap)) timers_.restore(snap.timers, slept_ms, now_ms);
    state_.set(kWakeCause, std::string(wake_cause_name(cause)));
    state_.set(kSleepBlocker, std::string(sleep_blocker_name(SleepBlocker::Active)));
}

SleepInput PowerService::input(uint64_t now_ms) const
{
    SleepInput in;
    in.now_ms = now_ms;
    in.last_activity_ms = last_activity_ms_;
    in.face_down = face_down_;
    in.face_down_since_ms = face_down_since_ms_;
    in.idle_sleep_min = static_cast<int>(as_int(settings_.get(kIdleSleepMin), kDefaultIdleSleepMin));
    if (in.idle_sleep_min < 0) in.idle_sleep_min = 0;
    const Value stay = settings_.get(kStayOnCharging);
    in.stay_on_when_charging = std::get_if<bool>(&stay) ? std::get<bool>(stay) : true;
    in.on_usb_power = as_bool(state_.get("power/usb_power"));
    // The screen-on rule has a safety limit: a forgotten open recipe must not drain the battery.
    const uint64_t idle = now_ms >= last_activity_ms_ ? now_ms - last_activity_ms_ : 0;
    in.keep_awake = as_bool(state_.get("ui/keep_awake")) && idle < kKeepAwakeLimitMs;
    in.updating = as_bool(state_.get("update/in_progress"));
    in.ringing = as_bool(state_.get("timers/ringing")) || as_bool(state_.get("alarm/ringing")) || timers_.timers().ringing();
    in.next_timer_ms = timers_.timers().next_due_ms(now_ms);
    in.next_alarm_s = as_int(state_.get("alarm/next_in_s"), -1);
    in.wake_margin_ms = wake_margin_ms_;
    return in;
}

void PowerService::tick(uint64_t now_ms)
{
    now_ms_ = now_ms;
    if (!started_) {  // no on_wake(): plain start
        started_ = true;
        last_activity_ms_ = last_check_ms_ = now_ms;
    }
    if (asleep_ || now_ms - last_check_ms_ < 1000) return;  // once a second is plenty
    last_check_ms_ = now_ms;
    const SleepInput in = input(now_ms);
    const SleepDecision d = decide_sleep(in);
    state_.set(kSleepBlocker, std::string(sleep_blocker_name(d.blocker)));
    if (!d.sleep()) return;
    SleepSnapshot snap;
    snap.timers = timers_.timers().save(now_ms);
    snap.next_alarm_s = in.next_alarm_s;
    snap.wake_for = d.wake_for;
    asleep_ = true;
    port_.enter(encode_snapshot(snap), d.wake_in_ms);  // does not return on the device
}

}  // namespace summit
