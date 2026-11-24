// Stopwatch and alarm service (Paul's design, docs/TIMER_BRIEF.md): connects `Stopwatch` and `Alarms` to the event bus
// and state store (topics in ARCHITECTURE.md "State topics"), next to `TimerService` (the kitchen timers). Host-tested
// in tests/host/test_clock_service.cpp. The clock page, tray items and countdown overlay are screens that use these
// topics (next steps).
//
// Alarms keep working on the real wall-clock time (`time/unix`, UTC until a time zone setting exists). Until the clock
// has synced they are kept but cannot ring; when it syncs (or jumps) they are armed again. The alarms are saved in the
// setting `alarm/saved` after every change, so they survive a restart.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/alarms.h"
#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/state_store.h"
#include "summit/stopwatch.h"

namespace summit {

class ClockToolsService {
public:
    ClockToolsService(EventBus &bus, StateStore &state, Settings &settings);
    ~ClockToolsService();
    ClockToolsService(const ClockToolsService &) = delete;
    ClockToolsService &operator=(const ClockToolsService &) = delete;

    // Call a few times a second from the UI loop (after bus.dispatch()).
    void tick(uint64_t now_ms);
    const Stopwatch &stopwatch() const { return stopwatch_; }
    const Alarms &alarms() const { return alarms_; }

private:
    int64_t local_now() const;  // wall-clock seconds, -1 until the clock has synced
    void publish_stopwatch();
    void publish_alarms();
    void save_alarms();
    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    Stopwatch stopwatch_;
    Alarms alarms_;
    uint64_t now_ms_ = 0;
    int64_t last_local_s_ = -1;
    std::string saved_;
    std::vector<EventBus::SubscriptionId> subs_;
};

}  // namespace summit
