#include "summit/state_store.h"

namespace summit {

bool StateStore::set(const std::string &key, Value value)
{
    if (!valid_topic(key)) return false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        Entry &e = entries_[key];
        if (e.revision != 0 && e.value == value) return false;
        e.value = value;
        e.revision++;
    }
    bus_.publish(key, std::move(value));
    return true;
}

Value StateStore::get(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(key);
    return it == entries_.end() ? Value{} : it->second.value;
}

bool StateStore::has(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(mu_);
    return entries_.count(key) != 0;
}

uint32_t StateStore::revision(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(key);
    return it == entries_.end() ? 0 : it->second.revision;
}

}  // namespace summit
