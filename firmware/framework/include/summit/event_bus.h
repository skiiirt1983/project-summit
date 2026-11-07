// Event bus: any task publishes, one consumer task (the UI loop) dispatches.
// publish() only queues; handlers run inside dispatch() on the consumer task,
// so panels never run on a service task.
#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "summit/value.h"

namespace summit {

class EventBus {
public:
    using Handler = std::function<void(const std::string &topic, const Value &value)>;
    using SubscriptionId = uint32_t;  // 0 = invalid

    struct Stats {
        uint32_t published = 0;
        uint32_t delivered = 0;  // handler calls
        uint32_t dropped = 0;    // oldest events dropped because the queue was full
        size_t queued = 0;
        size_t high_water = 0;
    };

    explicit EventBus(size_t capacity = 64);

    // Exact-topic subscription. Returns 0 for an invalid topic or empty handler.
    SubscriptionId subscribe(const std::string &topic, Handler handler);
    void unsubscribe(SubscriptionId id);

    // Thread-safe, never blocks on handlers. Returns false for an invalid topic.
    // When full, the oldest queued event is dropped (counted in stats).
    bool publish(const std::string &topic, Value value);

    // Consumer task only. Delivers at most `max` events, and never more than were
    // queued when the call started, so a handler that publishes cannot loop forever.
    size_t dispatch(size_t max = SIZE_MAX);

    Stats stats() const;

private:
    struct Event {
        std::string topic;
        Value value;
    };
    struct Sub {
        SubscriptionId id;
        std::string topic;
        Handler handler;
    };

    bool alive(SubscriptionId id) const;

    mutable std::mutex mu_;
    const size_t capacity_;
    std::deque<Event> queue_;
    std::vector<Sub> subs_;
    SubscriptionId next_id_ = 1;
    Stats stats_;
};

}  // namespace summit
