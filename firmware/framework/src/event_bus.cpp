#include "summit/event_bus.h"

#include <algorithm>
#include <utility>

namespace summit {

EventBus::EventBus(size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

EventBus::SubscriptionId EventBus::subscribe(const std::string &topic, Handler handler)
{
    if (!valid_topic(topic) || !handler) return 0;
    std::lock_guard<std::mutex> lock(mu_);
    SubscriptionId id = next_id_++;
    if (next_id_ == 0) next_id_ = 1;
    subs_.push_back({id, topic, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(mu_);
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(),
                               [id](const Sub &s) { return s.id == id; }),
                subs_.end());
}

bool EventBus::publish(const std::string &topic, Value value)
{
    if (!valid_topic(topic)) return false;
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.size() >= capacity_) {
        queue_.pop_front();
        stats_.dropped++;
    }
    queue_.push_back({topic, std::move(value)});
    stats_.published++;
    stats_.high_water = std::max(stats_.high_water, queue_.size());
    return true;
}

bool EventBus::alive(SubscriptionId id) const
{
    std::lock_guard<std::mutex> lock(mu_);
    return std::any_of(subs_.begin(), subs_.end(), [id](const Sub &s) { return s.id == id; });
}

size_t EventBus::dispatch(size_t max)
{
    size_t budget;
    {
        std::lock_guard<std::mutex> lock(mu_);
        budget = std::min(max, queue_.size());
    }
    size_t done = 0;
    for (; done < budget; ++done) {
        Event ev;
        std::vector<std::pair<SubscriptionId, Handler>> targets;
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (queue_.empty()) break;
            ev = std::move(queue_.front());
            queue_.pop_front();
            for (const Sub &s : subs_) {
                if (s.topic == ev.topic) targets.emplace_back(s.id, s.handler);
            }
        }
        for (auto &t : targets) {
            if (!alive(t.first)) continue;  // unsubscribed by an earlier handler
            t.second(ev.topic, ev.value);
            std::lock_guard<std::mutex> lock(mu_);
            stats_.delivered++;
        }
    }
    return done;
}

EventBus::Stats EventBus::stats() const
{
    std::lock_guard<std::mutex> lock(mu_);
    Stats s = stats_;
    s.queued = queue_.size();
    return s;
}

}  // namespace summit
