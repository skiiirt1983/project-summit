#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "check.h"
#include "summit/event_bus.h"
#include "summit/state_store.h"

using namespace summit;

TEST(topic_syntax)
{
    CHECK(valid_topic("weather/now"));
    CHECK(valid_topic("panels/home_clock/style"));
    CHECK(!valid_topic(""));
    CHECK(!valid_topic("Weather/now"));
    CHECK(!valid_topic("weather//now"));
    CHECK(!valid_topic("/weather"));
    CHECK(!valid_topic("weather/"));
    CHECK(!valid_topic("weather now"));
    CHECK(!valid_topic(std::string(65, 'a')));
}

TEST(publish_only_queues_dispatch_delivers)
{
    EventBus bus;
    std::vector<std::string> got;
    bus.subscribe("a/b", [&](const std::string &t, const Value &v) {
        got.push_back(t + "=" + std::to_string(std::get<int64_t>(v)));
    });
    CHECK(bus.publish("a/b", int64_t{1}));
    CHECK(bus.publish("a/c", int64_t{2}));  // no subscriber
    CHECK(got.empty());
    CHECK(bus.dispatch() == 2);
    CHECK(got.size() == 1 && got[0] == "a/b=1");
    CHECK(!bus.publish("Bad", Value{}));
    CHECK(bus.subscribe("Bad", [](const std::string &, const Value &) {}) == 0);
}

TEST(unsubscribe_stops_delivery_even_mid_dispatch)
{
    EventBus bus;
    int first = 0, second = 0;
    EventBus::SubscriptionId id2 = 0;
    bus.subscribe("x", [&](const std::string &, const Value &) {
        ++first;
        bus.unsubscribe(id2);
    });
    id2 = bus.subscribe("x", [&](const std::string &, const Value &) { ++second; });
    bus.publish("x", true);
    bus.dispatch();
    CHECK(first == 1 && second == 0);
}

TEST(handler_publishing_cannot_loop_forever)
{
    EventBus bus;
    int calls = 0;
    bus.subscribe("ping", [&](const std::string &, const Value &) {
        ++calls;
        bus.publish("ping", true);
    });
    bus.publish("ping", true);
    CHECK(bus.dispatch() == 1);
    CHECK(calls == 1);
    CHECK(bus.stats().queued == 1);
}

TEST(full_queue_drops_oldest)
{
    EventBus bus(3);
    std::vector<int64_t> got;
    bus.subscribe("n", [&](const std::string &, const Value &v) { got.push_back(std::get<int64_t>(v)); });
    for (int64_t i = 1; i <= 5; ++i) bus.publish("n", i);
    bus.dispatch();
    CHECK((got == std::vector<int64_t>{3, 4, 5}));
    EventBus::Stats s = bus.stats();
    CHECK(s.dropped == 2 && s.published == 5 && s.delivered == 3 && s.high_water == 3);
}

TEST(dispatch_respects_max)
{
    EventBus bus;
    int n = 0;
    bus.subscribe("t", [&](const std::string &, const Value &) { ++n; });
    for (int i = 0; i < 5; ++i) bus.publish("t", true);
    CHECK(bus.dispatch(2) == 2 && n == 2);
    CHECK(bus.dispatch() == 3 && n == 5);
}

TEST(publish_from_many_threads)
{
    EventBus bus(10000);
    std::atomic<int> got{0};
    bus.subscribe("t", [&](const std::string &, const Value &) { ++got; });
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&] { for (int i = 0; i < 1000; ++i) bus.publish("t", int64_t{i}); });
    size_t delivered = 0;
    while (delivered < 4000) delivered += bus.dispatch();
    for (auto &th : threads) th.join();
    CHECK(got == 4000);
    CHECK(bus.stats().dropped == 0);
}

TEST(state_store_publishes_changes_only)
{
    EventBus bus;
    StateStore store(bus);
    int events = 0;
    bus.subscribe("weather/now", [&](const std::string &, const Value &) { ++events; });
    CHECK(!store.has("weather/now"));
    CHECK(type_of(store.get("weather/now")) == ValueType::None);
    CHECK(store.set("weather/now", std::string("{\"t\":4}")));
    CHECK(!store.set("weather/now", std::string("{\"t\":4}")));  // unchanged
    CHECK(store.set("weather/now", std::string("{\"t\":5}")));
    CHECK(!store.set("Bad Key", true));
    bus.dispatch();
    CHECK(events == 2);
    CHECK(store.revision("weather/now") == 2);
    CHECK(std::get<std::string>(store.get("weather/now")) == "{\"t\":5}");
}

TEST(state_store_first_set_of_none_still_publishes)
{
    EventBus bus;
    StateStore store(bus);
    CHECK(store.set("a", Value{}));
    CHECK(store.revision("a") == 1);
}

int main()
{
    RUN(topic_syntax);
    RUN(publish_only_queues_dispatch_delivers);
    RUN(unsubscribe_stops_delivery_even_mid_dispatch);
    RUN(handler_publishing_cannot_loop_forever);
    RUN(full_queue_drops_oldest);
    RUN(dispatch_respects_max);
    RUN(publish_from_many_threads);
    RUN(state_store_publishes_changes_only);
    RUN(state_store_first_set_of_none_still_publishes);
    return report("event_bus + state_store");
}
