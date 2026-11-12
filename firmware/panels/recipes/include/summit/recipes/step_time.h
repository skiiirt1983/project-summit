// Finds cooking times in step text so they can be tapped to start a timer. Pure logic.
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace summit::recipes {

struct TimeSpan {
    size_t start = 0, length = 0;  // bytes in the step text ("25–30 minutes")
    int seconds = 0;               // a range uses its lower bound
};

// Every time mention, in order: "bake 25–30 minutes" -> 1500 s; "1 hour 30 minutes" ->
// 5400 s; "1 1/2 hours", "45 sec", "2-3 hrs", "30-minute rest", "10 to 12 min".
std::vector<TimeSpan> find_times(const std::string &text);
// The first time in the text, or 0.
int detect_timer_s(const std::string &text);
// "25 min", "1 h 30 min", "45 s" for timer names and chips.
std::string format_duration(int seconds);

}  // namespace summit::recipes
