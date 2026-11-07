#include "summit/update/health_gate.h"

namespace summit::update {

bool sample_healthy(const HealthSample &s)
{
    return s.ui_tick_age_ms <= HealthGate::kMaxTickAgeMs && s.frames_rendered > 0 &&
           s.free_heap >= HealthGate::kMinFreeHeap && s.touch_ok && s.i2c_ok && s.settings_ok;
}

GateDecision HealthGate::feed(const HealthSample &s, uint32_t now_ms)
{
    if (done_ != GateDecision::Wait) return done_;
    if (sample_healthy(s)) {
        if (++consecutive_ >= kRequiredSamples) return decided(GateDecision::Confirm);
    } else {
        consecutive_ = 0;  // never pass on disconnected good samples
    }
    if (now_ms >= kDeadlineMs) return decided(GateDecision::Rollback);
    return GateDecision::Wait;
}

}  // namespace summit::update
