// Kitchen timers (D-008): several named timers at once, started from a recipe step.
// `Timers` is the pure logic; `TimerService` connects it to the event bus and state
// store (topics in ARCHITECTURE.md "State topics"). Host-tested.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/event_bus.h"
#include "summit/state_store.h"

namespace summit {

struct KitchenTimer {
    int id = 0;
    std::string name;
    int total_s = 0;
    uint64_t end_ms = 0;
    bool ringing = false;  // finished, waiting to be dismissed
    int remaining_s(uint64_t now_ms) const;
};

class Timers {
public:
    static constexpr size_t kMax = 8;
    static constexpr int kMaxSeconds = 48 * 3600;
    static constexpr uint64_t kRingMs = 60 * 1000;  // a ringing timer stops by itself after a minute

    // 0 if refused (bad duration or too many timers).
    int start(const std::string &name, int seconds, uint64_t now_ms);
    bool cancel(int id);     // running or ringing
    void dismiss_ringing();  // stops every ringing timer
    // Advances time. Returns the timers that finished since the last call.
    std::vector<KitchenTimer> tick(uint64_t now_ms);

    const std::vector<KitchenTimer> &all() const { return timers_; }
    bool ringing() const;
    // Glance chip: "12:34", "1:05:00", "12:34 +2" (two more running), "Timer done"; "" if none.
    std::string chip_text(uint64_t now_ms) const;
    // One line per timer, soonest first: "<id>|<remaining_s>|<0/1 ringing>|<name>".
    std::string list_text(uint64_t now_ms) const;

private:
    std::vector<KitchenTimer> timers_;
    int next_id_ = 1;
};

std::string format_countdown(int seconds);  // "0:45", "12:34", "1:05:00"

// "<seconds>|<name>" (the timers/start payload).
bool parse_timer_request(const std::string &text, int &seconds, std::string &name);

class TimerService {
public:
    TimerService(EventBus &bus, StateStore &state);
    ~TimerService();
    TimerService(const TimerService &) = delete;
    TimerService &operator=(const TimerService &) = delete;

    // Call a few times a second from the UI loop (after bus.dispatch()).
    void tick(uint64_t now_ms);
    const Timers &timers() const { return timers_; }

private:
    void publish(uint64_t now_ms);
    EventBus &bus_;
    StateStore &state_;
    Timers timers_;
    uint64_t now_ms_ = 0;
    std::vector<EventBus::SubscriptionId> subs_;
};

}  // namespace summit
