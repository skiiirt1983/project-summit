#include "summit/timers.h"

#include <algorithm>
#include <cstdio>

namespace summit {

int KitchenTimer::remaining_s(uint64_t now_ms) const
{
    if (ringing || now_ms >= end_ms) return 0;
    return static_cast<int>((end_ms - now_ms + 999) / 1000);  // round up: "0:01" until it fires
}

std::string format_countdown(int seconds)
{
    if (seconds < 0) seconds = 0;
    char buf[24];
    const int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    if (h) std::snprintf(buf, sizeof buf, "%d:%02d:%02d", h, m, s);
    else std::snprintf(buf, sizeof buf, "%d:%02d", m, s);
    return buf;
}

bool parse_timer_request(const std::string &text, int &seconds, std::string &name)
{
    const size_t bar = text.find('|');
    const std::string num = text.substr(0, bar);
    if (num.empty() || num.size() > 6) return false;
    int v = 0;
    for (char c : num) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    if (v < 1 || v > Timers::kMaxSeconds) return false;
    seconds = v;
    name = bar == std::string::npos ? "" : text.substr(bar + 1);
    if (name.size() > 80) name.resize(80);
    for (char &c : name)
        if (c == '\n' || c == '\r' || c == '|') c = ' ';  // keep list_text one line per timer
    return true;  // an empty name becomes "Timer 1", "Timer 2" ... in Timers::start
}

std::string Timers::numbered_name() const
{
    for (int n = 1;; ++n) {  // the lowest number not in use: Timer 1, Timer 2, ...
        const std::string candidate = "Timer " + std::to_string(n);
        bool used = false;
        for (const KitchenTimer &t : timers_) used = used || t.name == candidate;
        if (!used) return candidate;
    }
}

int Timers::start(const std::string &name, int seconds, uint64_t now_ms)
{
    if (seconds < 1 || seconds > kMaxSeconds || timers_.size() >= kMax) return 0;
    KitchenTimer t;
    t.id = next_id_++;
    t.name = name.empty() || name == "Timer" ? numbered_name() : name;
    t.total_s = seconds;
    t.end_ms = now_ms + static_cast<uint64_t>(seconds) * 1000;
    timers_.push_back(t);
    return t.id;
}

bool Timers::cancel(int id)
{
    auto it = std::find_if(timers_.begin(), timers_.end(), [id](const KitchenTimer &t) { return t.id == id; });
    if (it == timers_.end()) return false;
    timers_.erase(it);
    return true;
}

void Timers::dismiss_ringing()
{
    timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [](const KitchenTimer &t) { return t.ringing; }),
                  timers_.end());
}

std::vector<KitchenTimer> Timers::tick(uint64_t now_ms)
{
    std::vector<KitchenTimer> fired;
    for (KitchenTimer &t : timers_) {
        if (!t.ringing && now_ms >= t.end_ms) {
            t.ringing = true;
            fired.push_back(t);
        }
    }
    // Nobody dismissed it: stop ringing after a minute.
    timers_.erase(std::remove_if(timers_.begin(), timers_.end(),
                                 [now_ms](const KitchenTimer &t) { return t.ringing && now_ms >= t.end_ms + kRingMs; }),
                  timers_.end());
    return fired;
}

std::vector<SavedTimer> Timers::save(uint64_t now_ms) const
{
    std::vector<SavedTimer> out;
    for (const KitchenTimer &t : timers_) {
        if (t.ringing) continue;
        out.push_back({t.name, t.total_s, t.end_ms > now_ms ? t.end_ms - now_ms : 0});
    }
    return out;
}

void Timers::restore(const std::vector<SavedTimer> &saved, uint64_t slept_ms, uint64_t now_ms)
{
    timers_.clear();
    for (const SavedTimer &s : saved) {
        if (timers_.size() >= kMax || s.total_s < 1 || s.total_s > kMaxSeconds) continue;
        KitchenTimer t;
        t.id = next_id_++;
        t.name = s.name.empty() || s.name == "Timer" ? numbered_name() : s.name;
        t.total_s = s.total_s;
        t.end_ms = now_ms + (s.remaining_ms > slept_ms ? s.remaining_ms - slept_ms : 0);
        timers_.push_back(t);
    }
}

int64_t Timers::next_due_ms(uint64_t now_ms) const
{
    int64_t best = -1;
    for (const KitchenTimer &t : timers_) {
        if (t.ringing) continue;
        const int64_t left = t.end_ms > now_ms ? static_cast<int64_t>(t.end_ms - now_ms) : 0;
        if (best < 0 || left < best) best = left;
    }
    return best;
}

bool Timers::ringing() const
{
    return std::any_of(timers_.begin(), timers_.end(), [](const KitchenTimer &t) { return t.ringing; });
}

std::string Timers::chip_text(uint64_t now_ms) const
{
    if (timers_.empty()) return "";
    if (ringing()) return "Timer done";
    int soonest = INT32_MAX;
    for (const KitchenTimer &t : timers_) soonest = std::min(soonest, t.remaining_s(now_ms));
    std::string s = format_countdown(soonest);
    if (timers_.size() > 1) s += " +" + std::to_string(timers_.size() - 1);
    return s;
}

std::string Timers::list_text(uint64_t now_ms) const
{
    std::vector<const KitchenTimer *> order;
    for (const KitchenTimer &t : timers_) order.push_back(&t);
    std::stable_sort(order.begin(), order.end(), [now_ms](const KitchenTimer *a, const KitchenTimer *b) {
        return a->remaining_s(now_ms) < b->remaining_s(now_ms);
    });
    std::string out;
    for (const KitchenTimer *t : order) {
        if (!out.empty()) out += '\n';
        out += std::to_string(t->id) + "|" + std::to_string(t->remaining_s(now_ms)) + "|" + (t->ringing ? "1" : "0") + "|" +
               t->name;
    }
    return out;
}

TimerService::TimerService(EventBus &bus, StateStore &state) : bus_(bus), state_(state)
{
    subs_.push_back(bus_.subscribe("timers/start", [this](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);
        int seconds;
        std::string name;
        if (!s || !parse_timer_request(*s, seconds, name)) return;
        if (timers_.start(name, seconds, now_ms_)) {
            bus_.publish("notify/toast", "Timer started: " + format_countdown(seconds));
        } else {
            bus_.publish("notify/toast", std::string("Too many timers running"));
        }
        publish(now_ms_);
    }));
    subs_.push_back(bus_.subscribe("timers/cancel", [this](const std::string &, const Value &v) {
        if (const int64_t *id = std::get_if<int64_t>(&v)) timers_.cancel(static_cast<int>(*id));
        publish(now_ms_);
    }));
    subs_.push_back(bus_.subscribe("timers/dismiss", [this](const std::string &, const Value &) {
        timers_.dismiss_ringing();
        publish(now_ms_);
    }));
    publish(0);
}

TimerService::~TimerService()
{
    for (auto id : subs_) bus_.unsubscribe(id);
}

void TimerService::tick(uint64_t now_ms)
{
    now_ms_ = now_ms;
    for (const KitchenTimer &t : timers_.tick(now_ms)) {
        // Critical: shown at once, even in full-screen modes; the audio service rings on timers/ringing.
        bus_.publish("notify/toast_critical", "Timer done: " + t.name);
    }
    publish(now_ms);
}

void TimerService::restore(const std::vector<SavedTimer> &saved, uint64_t slept_ms, uint64_t now_ms)
{
    now_ms_ = now_ms;
    timers_.restore(saved, slept_ms, now_ms);
    publish(now_ms);
}

void TimerService::publish(uint64_t now_ms)
{
    // The state store only notifies on change, so this is cheap between seconds.
    state_.set("timers/chip_text", timers_.chip_text(now_ms));
    state_.set("timers/list", timers_.list_text(now_ms));
    state_.set("timers/ringing", timers_.ringing());
}

}  // namespace summit
