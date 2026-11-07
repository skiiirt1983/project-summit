// State store: the one shared truth. Services write, panels read.
// A change publishes the key as a topic on the event bus; unchanged writes don't.
#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#include "summit/event_bus.h"
#include "summit/value.h"

namespace summit {

class StateStore {
public:
    explicit StateStore(EventBus &bus) : bus_(bus) {}

    // Thread-safe. Returns true if the value changed (and was published).
    // Returns false for an invalid key or an unchanged value.
    bool set(const std::string &key, Value value);

    // Missing key -> std::monostate.
    Value get(const std::string &key) const;
    bool has(const std::string &key) const;

    // Increments on every change; 0 = never set.
    uint32_t revision(const std::string &key) const;

private:
    struct Entry {
        Value value;
        uint32_t revision = 0;
    };
    EventBus &bus_;
    mutable std::mutex mu_;
    std::map<std::string, Entry> entries_;
};

}  // namespace summit
