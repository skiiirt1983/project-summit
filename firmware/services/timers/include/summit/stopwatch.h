// Stopwatch with laps (Paul, 2026-11-24): independent of the timers, counts up, a lap keeps it running and logs the
// lap time and the total time, and the laps stay until the stopwatch is reset. Pure logic, host-tested in
// tests/host/test_clock_tools.cpp; the service, topics and screen come in later steps.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace summit {

struct Lap {
    int number = 0;        // 1, 2, 3 ...
    uint64_t lap_ms = 0;   // time since the previous lap (or the start)
    uint64_t total_ms = 0; // stopwatch time at this lap
};

class Stopwatch {
public:
    static constexpr size_t kMaxLaps = 99;

    void start(uint64_t now_ms);  // also resumes after stop
    void stop(uint64_t now_ms);   // pauses; elapsed time stays
    void reset();                 // clears the time and every lap; stops
    // Records a lap while running. False when stopped or the 99 laps are used.
    bool lap(uint64_t now_ms);

    bool running() const { return running_; }
    uint64_t elapsed_ms(uint64_t now_ms) const;
    const std::vector<Lap> &laps() const { return laps_; }  // oldest first
    // Fastest and slowest lap numbers (0 until there are two laps), for highlighting.
    int fastest_lap() const;
    int slowest_lap() const;
    // One line per lap, newest first: "<number>|<lap_ms>|<total_ms>".
    std::string laps_text() const;

private:
    bool running_ = false;
    uint64_t accumulated_ms_ = 0;  // time from earlier runs
    uint64_t started_ms_ = 0;      // when the current run began
    std::vector<Lap> laps_;
};

// "0:07.25" (minutes:seconds.hundredths), "1:02:03.45" from one hour on.
std::string format_stopwatch(uint64_t ms);

}  // namespace summit
