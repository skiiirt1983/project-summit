#include "summit/stopwatch.h"

#include <cstdio>

namespace summit {

void Stopwatch::start(uint64_t now_ms)
{
    if (running_) return;
    running_ = true;
    started_ms_ = now_ms;
}

void Stopwatch::stop(uint64_t now_ms)
{
    if (!running_) return;
    accumulated_ms_ += now_ms > started_ms_ ? now_ms - started_ms_ : 0;
    running_ = false;
}

void Stopwatch::reset()
{
    running_ = false;
    accumulated_ms_ = 0;
    started_ms_ = 0;
    laps_.clear();
}

uint64_t Stopwatch::elapsed_ms(uint64_t now_ms) const
{
    if (!running_) return accumulated_ms_;
    return accumulated_ms_ + (now_ms > started_ms_ ? now_ms - started_ms_ : 0);
}

bool Stopwatch::lap(uint64_t now_ms)
{
    if (!running_ || laps_.size() >= kMaxLaps) return false;
    const uint64_t total = elapsed_ms(now_ms);
    const uint64_t before = laps_.empty() ? 0 : laps_.back().total_ms;
    Lap l;
    l.number = static_cast<int>(laps_.size()) + 1;
    l.total_ms = total;
    l.lap_ms = total > before ? total - before : 0;
    laps_.push_back(l);
    return true;
}

int Stopwatch::fastest_lap() const
{
    if (laps_.size() < 2) return 0;
    const Lap *best = &laps_[0];
    for (const Lap &l : laps_)
        if (l.lap_ms < best->lap_ms) best = &l;  // the first of equals wins
    return best->number;
}

int Stopwatch::slowest_lap() const
{
    if (laps_.size() < 2) return 0;
    const Lap *worst = &laps_[0];
    for (const Lap &l : laps_)
        if (l.lap_ms > worst->lap_ms) worst = &l;
    return worst->number;
}

std::string Stopwatch::laps_text() const
{
    std::string out;
    for (size_t i = laps_.size(); i-- > 0;) {
        if (!out.empty()) out += '\n';
        out += std::to_string(laps_[i].number) + "|" + std::to_string(laps_[i].lap_ms) + "|" + std::to_string(laps_[i].total_ms);
    }
    return out;
}

std::string format_stopwatch(uint64_t ms)
{
    const uint64_t total_s = ms / 1000;
    const unsigned hundredths = static_cast<unsigned>((ms % 1000) / 10);
    const unsigned h = static_cast<unsigned>(total_s / 3600), m = static_cast<unsigned>((total_s % 3600) / 60),
                   s = static_cast<unsigned>(total_s % 60);
    char buf[40];
    if (h) std::snprintf(buf, sizeof buf, "%u:%02u:%02u.%02u", h, m, s, hundredths);
    else std::snprintf(buf, sizeof buf, "%u:%02u.%02u", m, s, hundredths);
    return buf;
}

}  // namespace summit
