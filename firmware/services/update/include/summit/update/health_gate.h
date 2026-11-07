// Post-boot health gate for a newly installed (pending-verify) image.
// Pattern from LyricFlash main/ota_guard.c + ota_health_logic.c; Summit adds touch,
// I2C and settings-load checks (ARCHITECTURE.md "Carried over from LyricFlash").
// Pure logic: the device task samples every 2.5 s and acts on the decision.
#pragma once
#include <cstdint>

namespace summit::update {

struct HealthSample {
    uint32_t ui_tick_age_ms = UINT32_MAX;  // since the UI loop last ran
    uint32_t frames_rendered = 0;          // since boot
    uint32_t free_heap = 0;
    bool touch_ok = false;     // touch controller answers (no press needed)
    bool i2c_ok = false;       // expected devices answer on the shared bus
    bool settings_ok = false;  // settings loaded (any result except an invalid schema)
};

bool sample_healthy(const HealthSample &s);

enum class GateDecision { Wait, Confirm, Rollback };

class HealthGate {
public:
    static constexpr uint32_t kSampleMs = 2500;
    static constexpr uint32_t kDeadlineMs = 40000;
    static constexpr uint32_t kRequiredSamples = 3;  // consecutive
    static constexpr uint32_t kMaxTickAgeMs = 1500;
    static constexpr uint32_t kMinFreeHeap = 16 * 1024;

    // now_ms: time since boot.
    GateDecision feed(const HealthSample &s, uint32_t now_ms);
    // Fail-fast: any init failure on a pending image rolls back at once.
    GateDecision init_failed() { return decided(GateDecision::Rollback); }
    uint32_t consecutive() const { return consecutive_; }

private:
    GateDecision decided(GateDecision d)
    {
        done_ = d;
        return d;
    }
    uint32_t consecutive_ = 0;
    GateDecision done_ = GateDecision::Wait;
};

}  // namespace summit::update
