#include <string>

#include "check.h"
#include "fake_storage.h"
#include "summit/clock_tools.h"
#include "summit/system_settings.h"

using namespace summit;

namespace {
constexpr int64_t kMon = 1795392000;  // 2026-11-23 00:00:00, a Monday
int64_t at(int day, int h, int m, int s = 0) { return kMon + static_cast<int64_t>(day) * 86400 + h * 3600 + m * 60 + s; }

struct Rig {
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{system_settings_schema(), storage, &bus};
    ClockToolsService *svc = nullptr;
    uint64_t now = 1000;
    std::vector<std::string> toasts, critical;
    Rig()
    {
        settings.load();
        bus.subscribe("notify/toast", [this](const std::string &, const Value &v) { if (auto *s = std::get_if<std::string>(&v)) toasts.push_back(*s); });
        bus.subscribe("notify/toast_critical", [this](const std::string &, const Value &v) { if (auto *s = std::get_if<std::string>(&v)) critical.push_back(*s); });
        svc = new ClockToolsService(bus, state, settings);
        svc->tick(now);  // the loop's first pass
    }
    ~Rig() { delete svc; }
    void step(uint64_t ms = 100)
    {
        now += ms;
        bus.dispatch();
        svc->tick(now);
        bus.dispatch();
    }
    void clock(int64_t unix_s) { state.set("time/unix", unix_s); }
    std::string str(const char *topic) const
    {
        const Value v = state.get(topic);
        const std::string *s = std::get_if<std::string>(&v);
        return s ? *s : "<unset>";
    }
    bool flag(const char *topic) const
    {
        const Value v = state.get(topic);
        const bool *b = std::get_if<bool>(&v);
        return b && *b;
    }
    void say(const char *topic, const std::string &text)
    {
        bus.publish(topic, text);
        step(0);
    }
};
}  // namespace

TEST(stopwatch_commands_and_topics)
{
    Rig r;
    CHECK(r.str("stopwatch/state") == "0|0|0" && r.str("stopwatch/laps").empty() && r.str("stopwatch/lap_marks") == "0|0");
    r.say("stopwatch/command", "start");
    CHECK(r.str("stopwatch/state") == "1|0|1000");           // running: the screen counts from this anchor
    r.step(5000);
    r.say("stopwatch/command", "lap");
    CHECK(r.str("stopwatch/laps") == "1|5000|5000");
    r.step(3000);
    r.say("stopwatch/command", "lap");
    CHECK(r.str("stopwatch/laps") == "2|3000|8000\n1|5000|5000" && r.str("stopwatch/lap_marks") == "2|1");
    r.step(2000);
    r.say("stopwatch/command", "stop");
    CHECK(r.str("stopwatch/state") == "0|10000|" + std::to_string(r.now));
    r.say("stopwatch/command", "toggle");                  // toggle starts it again
    CHECK(r.str("stopwatch/state").substr(0, 8) == "1|10000|");
    r.say("stopwatch/command", "toggle");
    CHECK(r.str("stopwatch/state").substr(0, 2) == "0|");
    r.say("stopwatch/command", "klaxon");                  // unknown commands do nothing
    r.bus.publish("stopwatch/command", int64_t{5});
    r.step();
    CHECK(r.svc->stopwatch().laps().size() == 2);          // the laps stay until reset
    r.say("stopwatch/command", "reset");
    CHECK(r.str("stopwatch/state").substr(0, 4) == "0|0|" && r.str("stopwatch/laps").empty() && r.svc->stopwatch().laps().empty());
}

TEST(alarms_ring_on_the_synced_clock_and_dismiss)
{
    Rig r;
    r.say("alarm/add", "7|30|0|Wake up");
    CHECK(r.str("alarm/list") == "1|07:30|0|1|0|Wake up" && r.str("alarm/next_text") == "07:30");  // shown at once ...
    r.step(60000);
    CHECK(!r.flag("alarm/ringing"));                                                                // ... but it cannot ring before the clock has synced
    r.clock(at(0, 7, 0));
    r.step();
    CHECK(r.str("alarm/next_text") == "07:30" && !r.flag("alarm/ringing"));
    r.clock(at(0, 7, 30));
    r.step();
    CHECK(r.flag("alarm/ringing") && r.str("alarm/ringing_text") == "07:30 Wake up");
    CHECK(r.critical.size() == 1 && r.critical[0] == "Alarm: Wake up");
    CHECK(r.str("alarm/list") == "1|07:30|0|1|1|Wake up");
    r.say("alarm/dismiss", "1");
    CHECK(!r.flag("alarm/ringing") && r.str("alarm/ringing_text").empty() && r.str("alarm/list") == "1|07:30|0|0|0|Wake up");  // once: now off
    CHECK(r.str("alarm/next_text").empty());
}

TEST(snooze_and_repeat)
{
    Rig r;
    r.clock(at(0, 6, 0));
    r.step();
    r.say("alarm/add", "6|45|31|");                     // weekdays, no label
    r.clock(at(0, 6, 45));
    r.step();
    CHECK(r.flag("alarm/ringing") && r.critical.back() == "Alarm: 06:45");  // no label: the time
    r.say("alarm/snooze", "1");
    CHECK(!r.flag("alarm/ringing") && r.str("alarm/next_text") == "06:54");  // 9 minutes
    r.clock(at(0, 6, 54));
    r.step();
    CHECK(r.flag("alarm/ringing"));
    r.say("alarm/dismiss", "1");
    CHECK(r.str("alarm/next_text") == "06:45" && r.str("alarm/list") == "1|06:45|31|1|0|");  // a repeating alarm stays on
}

TEST(editing_alarms)
{
    Rig r;
    r.clock(at(0, 12, 0));
    r.step();
    r.say("alarm/add", "18|0|0|Dinner");
    r.say("alarm/add", "6|30|31|Run");
    CHECK(r.str("alarm/list") == "2|06:30|31|1|0|Run\n1|18:00|0|1|0|Dinner" && r.str("alarm/next_text") == "18:00");
    r.say("alarm/enable", "1|0");
    CHECK(r.str("alarm/next_text") == "06:30" && r.str("alarm/list") == "2|06:30|31|1|0|Run\n1|18:00|0|0|0|Dinner");
    r.say("alarm/enable", "1|1");
    r.say("alarm/update", "1|19|15|0|Late dinner");
    CHECK(r.str("alarm/list") == "2|06:30|31|1|0|Run\n1|19:15|0|1|0|Late dinner");
    r.say("alarm/delete", "");                           // wrong payload type: nothing
    r.bus.publish("alarm/delete", int64_t{2});
    r.step();
    CHECK(r.str("alarm/list") == "1|19:15|0|1|0|Late dinner");
    for (const char *bad : {"", "7|30", "24|0|0|x", "x"}) r.say("alarm/add", bad);
    for (const char *bad : {"", "1|2", "1|x", "|1", "9|1"}) r.say("alarm/enable", bad);
    for (const char *bad : {"", "7|0|0|0|x", "1|99|0|0|x"}) r.say("alarm/update", bad);
    CHECK(r.str("alarm/list") == "1|19:15|0|1|0|Late dinner");   // nothing above changed anything
    for (int i = 0; i < 9; ++i) r.say("alarm/add", "8|" + std::to_string(i) + "|0|");
    CHECK(r.svc->alarms().all().size() == Alarms::kMax && !r.toasts.empty() && r.toasts.back() == "Too many alarms");
}

TEST(alarms_survive_a_restart)
{
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{system_settings_schema(), storage, &bus};
    settings.load();
    {
        ClockToolsService first(bus, state, settings);
        bus.publish("alarm/add", std::string("7|30|31|Work"));
        bus.publish("alarm/add", std::string("22|0|0|Bed"));
        bus.dispatch();
        first.tick(1000);
        bus.publish("alarm/enable", std::string("2|0"));
        bus.dispatch();
        CHECK(std::get<std::string>(settings.get("alarm/saved")) == "7|30|31|1|Work\n22|0|0|0|Bed");
    }
    StateStore state2(bus);
    ClockToolsService second(bus, state2, settings);       // a restart: the setting is loaded
    CHECK(second.alarms().all().size() == 2 && second.alarms().all()[0].label == "Work" && !second.alarms().all()[1].enabled);
    state2.set("time/unix", at(0, 6, 0));
    bus.dispatch();
    second.tick(2000);
    bus.dispatch();
    CHECK(std::get<std::string>(state2.get("alarm/next_text")) == "07:30");  // armed once the clock is set
    state2.set("time/unix", at(0, 7, 30));
    second.tick(3000);
    CHECK(std::get<bool>(state2.get("alarm/ringing")));
}

TEST(an_alarm_due_while_asleep_still_rings_on_waking)
{
    Rig r;
    r.clock(at(0, 6, 0));
    r.step();
    r.say("alarm/add", "7|0|0|");
    r.clock(at(0, 6, 59));
    r.step();
    r.clock(at(0, 7, 0, 40));                            // the device woke 40 s after the alarm time
    r.step();
    CHECK(r.flag("alarm/ringing"));
    Rig late;
    late.clock(at(0, 6, 0));
    late.step();
    late.say("alarm/add", "7|0|31|");
    late.clock(at(0, 6, 59));
    late.step();
    late.clock(at(0, 9, 0));                             // woke two hours late: skipped
    late.step();
    CHECK(!late.flag("alarm/ringing") && late.str("alarm/next_text") == "07:00");
}

int main()
{
    RUN(stopwatch_commands_and_topics);
    RUN(alarms_ring_on_the_synced_clock_and_dismiss);
    RUN(snooze_and_repeat);
    RUN(editing_alarms);
    RUN(alarms_survive_a_restart);
    RUN(an_alarm_due_while_asleep_still_rings_on_waking);
    return report("clock service");
}
