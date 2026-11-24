#include <string>
#include <vector>

#include "check.h"
#include "summit/timers.h"

using namespace summit;

TEST(countdown_and_request_parsing)
{
    CHECK(format_countdown(45) == "0:45" && format_countdown(754) == "12:34" && format_countdown(3900) == "1:05:00" &&
          format_countdown(0) == "0:00");
    int s;
    std::string name;
    CHECK(parse_timer_request("1800|Pancakes: Step 2 of 3", s, name) && s == 1800 && name == "Pancakes: Step 2 of 3");
    CHECK(parse_timer_request("60", s, name) && s == 60 && name.empty());  // numbered by the timer list
    CHECK(parse_timer_request("60|a|b\nc", s, name) && name == "a b c");
    for (const char *bad : {"", "|x", "abc|x", "0|x", "-5|x", "9999999|x", "1e3|x"}) CHECK(!parse_timer_request(bad, s, name));
}

TEST(several_timers_at_once)
{
    Timers t;
    CHECK(t.chip_text(0).empty() && !t.ringing());
    int a = t.start("Rest batter", 1800, 0);
    int b = t.start("Eggs", 300, 1000);
    CHECK(a && b && a != b);
    CHECK(t.chip_text(1000) == "5:00 +1");  // soonest first, one more running
    CHECK(t.list_text(61000) == std::to_string(b) + "|240|0|Eggs\n" + std::to_string(a) + "|1739|0|Rest batter");
    CHECK(t.tick(300999).empty());
    auto fired = t.tick(301000);
    CHECK(fired.size() == 1 && fired[0].name == "Eggs" && t.ringing() && t.chip_text(301000) == "Timer done");
    CHECK(t.tick(302000).empty());  // fires once
    t.dismiss_ringing();
    CHECK(!t.ringing() && t.all().size() == 1 && t.chip_text(302000) == "24:58");
    CHECK(t.cancel(a) && !t.cancel(a) && t.chip_text(302000).empty());

    // Nobody dismisses: stops ringing by itself after a minute.
    int c = t.start("Tea", 10, 400000);
    CHECK(c && t.tick(410000).size() == 1 && t.ringing());
    t.tick(410000 + Timers::kRingMs - 1);
    CHECK(t.ringing());
    t.tick(410000 + Timers::kRingMs);
    CHECK(!t.ringing() && t.all().empty());

    CHECK(!t.start("x", 0, 0) && !t.start("x", Timers::kMaxSeconds + 1, 0));
    for (size_t i = 0; i < Timers::kMax; ++i) CHECK(t.start("n", 60, 0));
    CHECK(!t.start("one too many", 60, 0));
}

TEST(service_topics)
{
    EventBus bus;
    StateStore state(bus);
    TimerService svc(bus, state);
    std::vector<std::string> toasts, critical;
    bus.subscribe("notify/toast", [&](const std::string &, const Value &v) { toasts.push_back(std::get<std::string>(v)); });
    bus.subscribe("notify/toast_critical", [&](const std::string &, const Value &v) { critical.push_back(std::get<std::string>(v)); });
    auto str = [&](const char *k) { return std::get<std::string>(state.get(k)); };
    CHECK(str("timers/chip_text").empty() && std::get<bool>(state.get("timers/ringing")) == false);

    svc.tick(1000);
    bus.publish("timers/start", std::string("120|Pancakes: Step 3 of 3"));
    bus.publish("timers/start", std::string("garbage"));
    bus.dispatch();
    bus.dispatch();
    CHECK(svc.timers().all().size() == 1 && toasts.size() == 1 && toasts[0] == "Timer started: 2:00");
    CHECK(str("timers/chip_text") == "2:00");
    svc.tick(61000);
    CHECK(str("timers/chip_text") == "1:00" && str("timers/list").find("|60|0|Pancakes: Step 3 of 3") != std::string::npos);
    svc.tick(121000);
    bus.dispatch();
    CHECK(critical.size() == 1 && critical[0] == "Timer done: Pancakes: Step 3 of 3");
    CHECK(std::get<bool>(state.get("timers/ringing")) && str("timers/chip_text") == "Timer done");
    bus.publish("timers/dismiss", true);
    bus.dispatch();
    CHECK(!std::get<bool>(state.get("timers/ringing")) && str("timers/chip_text").empty());

    bus.publish("timers/start", std::string("600|Soup"));
    bus.dispatch();
    const int id = svc.timers().all()[0].id;
    bus.publish("timers/cancel", int64_t{id});
    bus.dispatch();
    CHECK(svc.timers().all().empty());
}

int main()
{
    RUN(countdown_and_request_parsing);
    RUN(several_timers_at_once);
    RUN(service_topics);
    return report("timers");
}
