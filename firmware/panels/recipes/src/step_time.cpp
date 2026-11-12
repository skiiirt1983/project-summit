#include "summit/recipes/step_time.h"

#include <cctype>

namespace summit::recipes {

namespace {

bool digit(char c) { return c >= '0' && c <= '9'; }
bool alpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }

void skip_spaces(const std::string &t, size_t &i)
{
    while (i < t.size() && t[i] == ' ') ++i;
}

// "25", "2.5", "1/2", "1 1/2". Advances i past the number on success.
bool number(const std::string &t, size_t &i, double &out)
{
    size_t p = i;
    if (p >= t.size() || !digit(t[p])) return false;
    double v = 0;
    while (p < t.size() && digit(t[p])) v = v * 10 + (t[p++] - '0');
    if (p + 1 < t.size() && t[p] == '.' && digit(t[p + 1])) {
        ++p;
        double scale = 0.1;
        while (p < t.size() && digit(t[p])) {
            v += (t[p++] - '0') * scale;
            scale *= 0.1;
        }
    } else if (p + 1 < t.size() && t[p] == '/' && digit(t[p + 1])) {  // "1/2"
        size_t q = p + 1;
        double den = 0;
        while (q < t.size() && digit(t[q])) den = den * 10 + (t[q++] - '0');
        if (den > 0) {
            v /= den;
            p = q;
        }
    } else {  // "1 1/2"
        size_t q = p;
        skip_spaces(t, q);
        if (q > p && q < t.size() && digit(t[q])) {
            double num = 0, den = 0;
            size_t r = q;
            while (r < t.size() && digit(t[r])) num = num * 10 + (t[r++] - '0');
            if (r + 1 < t.size() && t[r] == '/' && digit(t[r + 1])) {
                ++r;
                while (r < t.size() && digit(t[r])) den = den * 10 + (t[r++] - '0');
                if (den > 0) {
                    v += num / den;
                    p = r;
                }
            }
        }
    }
    out = v;
    i = p;
    return true;
}

// Seconds per unit for the word at i (0 if not a time unit). Advances i past it.
int unit(const std::string &t, size_t &i)
{
    size_t p = i;
    std::string w;
    while (p < t.size() && alpha(t[p])) w += static_cast<char>(std::tolower(static_cast<unsigned char>(t[p++])));
    int s = 0;
    for (const char *u : {"hour", "hours", "hr", "hrs", "h"})
        if (w == u) s = 3600;
    for (const char *u : {"minute", "minutes", "min", "mins"})
        if (w == u) s = 60;
    for (const char *u : {"second", "seconds", "sec", "secs"})
        if (w == u) s = 1;
    if (s) i = p;
    return s;
}

// "-", en dash, em dash or " to " between the two ends of a range.
bool range_separator(const std::string &t, size_t &i)
{
    size_t p = i;
    skip_spaces(t, p);
    if (p < t.size() && t[p] == '-') {
        ++p;
    } else if (t.compare(p, 3, "\xE2\x80\x93") == 0 || t.compare(p, 3, "\xE2\x80\x94") == 0) {
        p += 3;
    } else if (p > i && t.compare(p, 3, "to ") == 0) {
        p += 2;
    } else {
        return false;
    }
    skip_spaces(t, p);
    if (p >= t.size() || !digit(t[p])) return false;
    i = p;
    return true;
}

// number [range] unit at i. Advances i past the unit.
bool amount(const std::string &t, size_t &i, int &seconds, int &unit_s)
{
    size_t p = i;
    double v;
    if (!number(t, p, v)) return false;
    size_t q = p;
    double upper;
    if (range_separator(t, q) && number(t, q, upper)) p = q;  // lower bound is used
    if (p < t.size() && (t[p] == ' ' || t[p] == '-')) ++p;    // "30 minutes", "30-minute"
    skip_spaces(t, p);
    int u = unit(t, p);
    if (!u) return false;
    const double total = v * u;
    if (total < 1 || total > 48 * 3600) return false;
    seconds = static_cast<int>(total + 0.5);
    unit_s = u;
    i = p;
    return true;
}

}  // namespace

std::vector<TimeSpan> find_times(const std::string &text)
{
    std::vector<TimeSpan> out;
    size_t i = 0;
    while (i < text.size()) {
        const bool boundary = i == 0 || !(std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '.' ||
                                          text[i - 1] == '/');
        if (!digit(text[i]) || !boundary) {
            ++i;
            continue;
        }
        size_t p = i;
        int seconds = 0, u = 0;
        if (!amount(text, p, seconds, u)) {
            while (i < text.size() && (digit(text[i]) || text[i] == '.' || text[i] == '/')) ++i;
            continue;
        }
        // Compound: "1 hour 30 minutes", "1 hour and 30 minutes", "1 hr, 15 min".
        if (u > 1) {
            size_t q = p;
            if (q < text.size() && text[q] == ',') ++q;
            skip_spaces(text, q);
            if (text.compare(q, 4, "and ") == 0) q += 4;
            int more = 0, u2 = 0;
            if (q > p && amount(text, q, more, u2) && u2 < u) {
                seconds += more;
                p = q;
            }
        }
        out.push_back({i, p - i, seconds});
        i = p;
    }
    return out;
}

int detect_timer_s(const std::string &text)
{
    std::vector<TimeSpan> t = find_times(text);
    return t.empty() ? 0 : t.front().seconds;
}

std::string format_duration(int seconds)
{
    if (seconds <= 0) return "0 s";
    const int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    std::string out;
    if (h) out += std::to_string(h) + " h";
    if (m) out += (out.empty() ? "" : " ") + std::to_string(m) + " min";
    if (s && !h) out += (out.empty() ? "" : " ") + std::to_string(s) + " s";
    return out;
}

}  // namespace summit::recipes
