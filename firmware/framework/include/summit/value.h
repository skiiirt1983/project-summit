// Value type shared by the event bus, state store and settings.
#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace summit {

// None, bool, int64, double, string. Structured data (e.g. weather/now) is a
// JSON string produced by the owning service.
using Value = std::variant<std::monostate, bool, int64_t, double, std::string>;

enum class ValueType : uint8_t { None = 0, Bool = 1, Int = 2, Double = 3, String = 4 };

inline ValueType type_of(const Value &v) { return static_cast<ValueType>(v.index()); }

// Topics, state keys and setting keys: lowercase segments [a-z0-9_] joined by '/',
// e.g. "weather/now", "panels/news/feeds". Max 64 chars.
bool valid_topic(const std::string &topic);

}  // namespace summit
