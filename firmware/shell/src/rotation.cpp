#include "summit/shell/rotation.h"

namespace summit::shell {

void RotationController::report(Orientation o, uint32_t now_ms)
{
    if (o != pending_) {
        pending_ = o;
        pending_since_ = now_ms;
    }
}

void RotationController::set_lock(const std::string &mode)
{
    lock_ = (mode == "landscape" || mode == "portrait") ? mode : "off";
}

bool RotationController::update(uint32_t now_ms)
{
    Orientation target;
    if (lock_ == "landscape") {
        target = kLandscape;
    } else if (lock_ == "portrait") {
        target = kPortrait;
    } else {
        if (pending_ == applied_ || now_ms - pending_since_ < kDebounceMs) return false;
        target = pending_;
    }
    if (target == applied_) return false;
    applied_ = target;
    return true;
}

std::string RotationController::toggled_lock() const
{
    if (lock_ != "off") return "off";
    return applied_ == kLandscape ? "landscape" : "portrait";
}

}  // namespace summit::shell
