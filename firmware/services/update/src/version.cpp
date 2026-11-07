#include "summit/update/version.h"

namespace summit::update {

namespace {

bool parse_number(const std::string &s, uint32_t &out)
{
    if (s.empty() || s.size() > 9 || (s.size() > 1 && s[0] == '0')) return false;
    uint32_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<uint32_t>(c - '0');
    }
    out = v;
    return true;
}

bool numeric(const std::string &s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

std::vector<std::string> split(const std::string &s, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t p = s.find(sep, start);
        out.push_back(s.substr(start, p - start));
        if (p == std::string::npos) return out;
        start = p + 1;
    }
}

}  // namespace

bool parse_version(const std::string &text, Version &out)
{
    if (text.empty() || text.size() > 31) return false;  // fits esp_app_desc_t::version
    std::string core = text, pre;
    size_t dash = text.find('-');
    if (dash != std::string::npos) {
        core = text.substr(0, dash);
        pre = text.substr(dash + 1);
        if (pre.empty()) return false;
    }
    auto parts = split(core, '.');
    if (parts.size() != 3) return false;
    Version v;
    if (!parse_number(parts[0], v.major) || !parse_number(parts[1], v.minor) || !parse_number(parts[2], v.patch))
        return false;
    if (!pre.empty()) {
        for (const auto &id : split(pre, '.')) {
            if (id.empty()) return false;
            for (char c : id) {
                bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
                if (!ok) return false;
            }
            if (numeric(id) && id.size() > 1 && id[0] == '0') return false;
            v.pre.push_back(id);
        }
    }
    out = v;
    return true;
}

int compare_versions(const Version &a, const Version &b)
{
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    if (a.pre.empty() || b.pre.empty()) {
        if (a.pre.empty() && b.pre.empty()) return 0;
        return a.pre.empty() ? 1 : -1;
    }
    for (size_t i = 0; i < a.pre.size() && i < b.pre.size(); ++i) {
        const std::string &x = a.pre[i], &y = b.pre[i];
        const bool nx = numeric(x), ny = numeric(y);
        if (nx && ny) {
            if (x.size() != y.size()) return x.size() < y.size() ? -1 : 1;
            if (x != y) return x < y ? -1 : 1;
        } else if (nx != ny) {
            return nx ? -1 : 1;  // numeric identifiers sort before alphanumeric
        } else if (x != y) {
            return x < y ? -1 : 1;
        }
    }
    if (a.pre.size() == b.pre.size()) return 0;
    return a.pre.size() < b.pre.size() ? -1 : 1;
}

}  // namespace summit::update
