// Minimal JSON writer (objects, arrays, strings, numbers, bools, null). Pure, host-tested.
// Parsing untrusted JSON uses cJSON (ESP-IDF's json component on the device).
#pragma once
#include <cstdint>
#include <string>

#include "summit/value.h"

namespace summit {

std::string json_escape(const std::string &s);  // quoted JSON string

class JsonWriter {
public:
    JsonWriter &begin_object();
    JsonWriter &end_object();
    JsonWriter &begin_array();
    JsonWriter &end_array();
    JsonWriter &key(const std::string &k);
    JsonWriter &value(const std::string &s);
    JsonWriter &value(const char *s) { return value(std::string(s)); }
    JsonWriter &value(int64_t i);
    JsonWriter &value(int i) { return value(static_cast<int64_t>(i)); }
    JsonWriter &value(uint32_t i) { return value(static_cast<int64_t>(i)); }
    JsonWriter &value(double d);
    JsonWriter &value(bool b);
    JsonWriter &null();
    JsonWriter &value(const Value &v);       // None -> null
    JsonWriter &raw(const std::string &json);  // already-valid JSON
    const std::string &str() const { return out_; }

private:
    void comma();
    std::string out_;
    bool need_comma_ = false;
};

}  // namespace summit
