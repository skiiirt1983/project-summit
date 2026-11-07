// Orientation: 400 ms debounce of sensor reports, rotation lock. Pure logic.
#pragma once
#include <cstdint>
#include <string>

#include "summit/panel_api.h"

namespace summit::shell {

class RotationController {
public:
    static constexpr uint32_t kDebounceMs = 400;  // ARCHITECTURE.md "Core loop"
    static constexpr uint32_t kMorphMs = 250;

    explicit RotationController(Orientation initial = kLandscape) : applied_(initial), pending_(initial) {}

    // Sensor report (e.g. LIS2DW12 4D change).
    void report(Orientation o, uint32_t now_ms);
    // Lock from the ui/rotation_lock setting: "off", "landscape" or "portrait".
    void set_lock(const std::string &mode);
    // Returns true when the applied orientation changed (the shell then re-lays out).
    bool update(uint32_t now_ms);

    Orientation applied() const { return applied_; }
    const std::string &lock() const { return lock_; }
    // Long-press: unlocked -> lock to the current orientation; locked -> "off".
    std::string toggled_lock() const;

private:
    Orientation applied_;
    Orientation pending_;
    uint32_t pending_since_ = 0;
    std::string lock_ = "off";
};

}  // namespace summit::shell
