#include "summit/json_writer.h"

#include <cmath>
#include <cstdio>

namespace summit {

std::string json_escape(const std::string &s)
{
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20 || c == 0x7F) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);  // UTF-8 passes through
            }
        }
    }
    return out + "\"";
}

void JsonWriter::comma()
{
    if (need_comma_) out_ += ',';
    need_comma_ = true;
}

JsonWriter &JsonWriter::begin_object()
{
    comma();
    out_ += '{';
    need_comma_ = false;
    return *this;
}

JsonWriter &JsonWriter::end_object()
{
    out_ += '}';
    need_comma_ = true;
    return *this;
}

JsonWriter &JsonWriter::begin_array()
{
    comma();
    out_ += '[';
    need_comma_ = false;
    return *this;
}

JsonWriter &JsonWriter::end_array()
{
    out_ += ']';
    need_comma_ = true;
    return *this;
}

JsonWriter &JsonWriter::key(const std::string &k)
{
    comma();
    out_ += json_escape(k) + ':';
    need_comma_ = false;
    return *this;
}

JsonWriter &JsonWriter::value(const std::string &s)
{
    comma();
    out_ += json_escape(s);
    return *this;
}

JsonWriter &JsonWriter::value(int64_t i)
{
    comma();
    out_ += std::to_string(i);
    return *this;
}

JsonWriter &JsonWriter::value(double d)
{
    comma();
    if (!std::isfinite(d)) {
        out_ += "null";
        return *this;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    std::string s = buf;
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";  // stays a double on import
    out_ += s;
    return *this;
}

JsonWriter &JsonWriter::value(bool b)
{
    comma();
    out_ += b ? "true" : "false";
    return *this;
}

JsonWriter &JsonWriter::null()
{
    comma();
    out_ += "null";
    return *this;
}

JsonWriter &JsonWriter::value(const Value &v)
{
    switch (type_of(v)) {
    case ValueType::None: return null();
    case ValueType::Bool: return value(std::get<bool>(v));
    case ValueType::Int: return value(std::get<int64_t>(v));
    case ValueType::Double: return value(std::get<double>(v));
    case ValueType::String: return value(std::get<std::string>(v));
    }
    return null();
}

JsonWriter &JsonWriter::raw(const std::string &json)
{
    comma();
    out_ += json;
    return *this;
}

}  // namespace summit
