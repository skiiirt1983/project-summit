#include "summit/clock_tools.h"

namespace summit {

namespace {
bool parse_id(const Value &v, int &id)
{
    if (const int64_t *i = std::get_if<int64_t>(&v)) {
        if (*i < 1 || *i > 1000000) return false;
        id = static_cast<int>(*i);
        return true;
    }
    return false;
}
}  // namespace

ClockToolsService::ClockToolsService(EventBus &bus, StateStore &state, Settings &settings)
    : bus_(bus), state_(state), settings_(settings)
{
    const Value saved = settings_.get("alarm/saved");
    if (const std::string *s = std::get_if<std::string>(&saved)) {
        alarms_.restore(*s, 0);  // armed properly once the clock has synced
        saved_ = alarms_.serialize();
    }

    subs_.push_back(bus_.subscribe("stopwatch/command", [this](const std::string &, const Value &v) {
        const std::string *c = std::get_if<std::string>(&v);
        if (!c) return;
        if (*c == "start") stopwatch_.start(now_ms_);
        else if (*c == "stop") stopwatch_.stop(now_ms_);
        else if (*c == "toggle") stopwatch_.running() ? stopwatch_.stop(now_ms_) : stopwatch_.start(now_ms_);
        else if (*c == "lap") stopwatch_.lap(now_ms_);
        else if (*c == "reset") stopwatch_.reset();
        else return;
        publish_stopwatch();
    }));
    subs_.push_back(bus_.subscribe("alarm/add", [this](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);
        int h, m, d;
        std::string label;
        if (!s || !parse_alarm_request(*s, h, m, d, label)) return;
        if (!alarms_.add(h, m, d, label, local_now() < 0 ? 0 : local_now())) bus_.publish("notify/toast", std::string("Too many alarms"));
        publish_alarms();
    }));
    subs_.push_back(bus_.subscribe("alarm/update", [this](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);
        int id, h, m, d;
        std::string label;
        if (!s || !parse_alarm_update(*s, id, h, m, d, label)) return;
        alarms_.update(id, h, m, d, label, local_now() < 0 ? 0 : local_now());
        publish_alarms();
    }));
    subs_.push_back(bus_.subscribe("alarm/enable", [this](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);  // "<id>|<0/1>"
        if (!s) return;
        const size_t bar = s->find('|');
        if (bar == std::string::npos || bar == 0 || bar + 2 != s->size() || ((*s)[bar + 1] != '0' && (*s)[bar + 1] != '1')) return;
        int id = 0;
        for (size_t i = 0; i < bar; ++i) {
            if ((*s)[i] < '0' || (*s)[i] > '9' || i > 6) return;
            id = id * 10 + ((*s)[i] - '0');
        }
        alarms_.set_enabled(id, (*s)[bar + 1] == '1', local_now() < 0 ? 0 : local_now());
        publish_alarms();
    }));
    subs_.push_back(bus_.subscribe("alarm/delete", [this](const std::string &, const Value &v) {
        int id;
        if (parse_id(v, id)) alarms_.remove(id);
        publish_alarms();
    }));
    subs_.push_back(bus_.subscribe("alarm/dismiss", [this](const std::string &, const Value &) {
        alarms_.dismiss();
        publish_alarms();
    }));
    subs_.push_back(bus_.subscribe("alarm/snooze", [this](const std::string &, const Value &) {
        const int64_t now = local_now();
        if (now >= 0) alarms_.snooze(now);
        publish_alarms();
    }));
    publish_stopwatch();
    publish_alarms();
}

ClockToolsService::~ClockToolsService()
{
    for (auto id : subs_) bus_.unsubscribe(id);
}

int64_t ClockToolsService::local_now() const
{
    const Value v = state_.get("time/unix");
    if (const int64_t *t = std::get_if<int64_t>(&v)) return *t;
    return -1;
}

void ClockToolsService::tick(uint64_t now_ms)
{
    now_ms_ = now_ms;
    const int64_t local = local_now();
    if (local >= 0) {
        // The clock was just set (the first time/unix after start-up): arm every alarm against the real time without ringing
        // any. Later jumps are Alarms::tick's business (a late alarm rings if it is within 2 minutes, a clock that goes back
        // re-arms), so an alarm that comes due while the device slept still rings on waking.
        if (last_local_s_ < 0) alarms_.clock_changed(local);
        last_local_s_ = local;
        for (int id : alarms_.tick(local)) {
            const Alarm *a = alarms_.find(id);
            if (!a) continue;
            // Critical: shown at once; the audio service rings on alarm/ringing.
            bus_.publish("notify/toast_critical", "Alarm: " + (a->label.empty() ? format_alarm_time(a->hour, a->minute) : a->label));
        }
    }
    publish_alarms();
}

void ClockToolsService::publish_stopwatch()
{
    // The screen counts on its own from this anchor: elapsed = elapsed_at_anchor (+ now - anchor while running).
    state_.set("stopwatch/state", std::string(stopwatch_.running() ? "1" : "0") + "|" + std::to_string(stopwatch_.elapsed_ms(now_ms_)) + "|" +
                                      std::to_string(now_ms_));
    state_.set("stopwatch/laps", stopwatch_.laps_text());
    state_.set("stopwatch/lap_marks", std::to_string(stopwatch_.fastest_lap()) + "|" + std::to_string(stopwatch_.slowest_lap()));
}

void ClockToolsService::publish_alarms()
{
    state_.set("alarm/list", alarms_.list_text());
    state_.set("alarm/next_text", alarms_.next_text());
    state_.set("alarm/ringing", alarms_.ringing());
    std::string ringing_text;
    for (const Alarm &a : alarms_.all())
        if (a.ringing) ringing_text = format_alarm_time(a.hour, a.minute) + (a.label.empty() ? "" : " " + a.label);
    state_.set("alarm/ringing_text", ringing_text);
    save_alarms();
}

void ClockToolsService::save_alarms()
{
    const std::string now_text = alarms_.serialize();
    if (now_text == saved_) return;
    saved_ = now_text;
    settings_.set("alarm/saved", now_text);
}

}  // namespace summit
