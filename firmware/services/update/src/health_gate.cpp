#include "summit/update/health_gate.h"

namespace summit::update {

bool i2c_healthy(const std::vector<I2cDeviceStatus> &devices, std::vector<std::string> *warnings)
{
    bool ok = true;
    for (const I2cDeviceStatus &d : devices) {
        if (d.present) continue;
        if (d.built_in) {
            ok = false;
        } else if (warnings) {
            warnings->push_back(d.name + " not found (add-on, does not affect updates)");
        }
    }
    return ok;
}

std::vector<I2cDeviceStatus> board_i2c_devices()
{
    std::vector<I2cDeviceStatus> d(4);
    d[0].name = "GT911 touch";
    d[1].name = "ES8311 codec";
    d[2].name = "ES7210 echo canceller";
    d[3].name = "LIS2DW12 accelerometer";
    d[3].built_in = false;  // add-on wired to the 40-pin header
    return d;
}

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
