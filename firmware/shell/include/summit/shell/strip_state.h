// Glance strip behaviour: hide reasons + tap-to-reveal, and the toast queue. Pure logic.
#pragma once
#include <cstdint>
#include <deque>
#include <string>

namespace summit::shell {

// Full-screen modes that hide the strip (ARCHITECTURE.md "Glance strip").
enum HideReason : uint8_t {
    kHideFullscreen = 1,   // LyricFlash full-screen visuals
    kHideAlarm = 2,        // ringing alarm
    kHideAmbient = 4,      // night / ambient clock
    kHideUpdate = 8,       // update progress
};

class StripVisibility {
public:
    static constexpr uint32_t kRevealMs = 3000;

    void set_reason(HideReason r, bool active);
    bool hidden_by_mode() const { return reasons_ != 0; }
    // Tap while hidden: show for 3 s. Returns true if it revealed the strip.
    bool reveal(uint32_t now_ms);
    bool visible(uint32_t now_ms) const;

private:
    uint8_t reasons_ = 0;
    uint32_t reveal_until_ = 0;
    bool revealed_ = false;
};

struct Toast {
    std::string text;
    std::string icon;
};

// Notifications briefly expand the strip, one at a time, oldest first.
class ToastQueue {
public:
    static constexpr uint32_t kShowMs = 4000;
    static constexpr size_t kMaxQueued = 8;

    // A toast identical to the last queued/showing one is ignored.
    void push(Toast t);
    // Advances time; returns the toast to show now, or nullptr when collapsed.
    const Toast *current(uint32_t now_ms);
    size_t pending() const { return queue_.size(); }

private:
    std::deque<Toast> queue_;
    bool showing_ = false;
    uint32_t until_ = 0;
};

}  // namespace summit::shell
