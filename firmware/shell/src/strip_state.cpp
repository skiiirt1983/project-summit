#include "summit/shell/strip_state.h"

namespace summit::shell {

// Wrap-safe "a is before b" for millisecond timestamps.
static bool before(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) < 0; }

void StripVisibility::set_reason(HideReason r, bool active)
{
    if (active) {
        reasons_ |= r;
    } else {
        reasons_ &= static_cast<uint8_t>(~r);
    }
    if (reasons_ == 0) revealed_ = false;
}

bool StripVisibility::reveal(uint32_t now_ms)
{
    if (reasons_ == 0) return false;
    revealed_ = true;
    reveal_until_ = now_ms + kRevealMs;
    return true;
}

bool StripVisibility::visible(uint32_t now_ms) const
{
    if (reasons_ == 0) return true;
    return revealed_ && before(now_ms, reveal_until_);
}

void ToastQueue::push(Toast t, uint32_t now_ms)
{
    t.queued_ms = now_ms;
    for (Toast &q : queue_) {
        if (q.text == t.text) {  // merge: keep one, escalate if the new one is critical
            q.critical = q.critical || t.critical;
            return;
        }
    }
    if (t.critical) {
        // Ahead of every waiting normal toast. A normal toast on screen is interrupted
        // and shown again (in full) after the critical ones.
        if (showing_ && !queue_.front().critical) showing_ = false;
        auto it = queue_.begin() + (showing_ ? 1 : 0);
        while (it != queue_.end() && it->critical) ++it;
        queue_.insert(it, std::move(t));
    } else {
        queue_.push_back(std::move(t));
    }
    while (queue_.size() > kMaxQueued) {
        // Drop the oldest waiting normal toast (never the one on screen or a critical one).
        auto victim = queue_.end();
        for (auto it = queue_.begin() + (showing_ ? 1 : 0); it != queue_.end(); ++it) {
            if (!it->critical) {
                victim = it;
                break;
            }
        }
        if (victim == queue_.end()) break;
        queue_.erase(victim);
    }
}

const Toast *ToastQueue::current(uint32_t now_ms, bool strip_visible)
{
    if (showing_ && !before(now_ms, until_)) {
        queue_.pop_front();
        showing_ = false;
    }
    if (!showing_) {
        // Expire normal toasts that waited too long.
        for (auto it = queue_.begin(); it != queue_.end();) {
            if (!it->critical && now_ms - it->queued_ms >= kExpireMs) {
                it = queue_.erase(it);
            } else {
                ++it;
            }
        }
        if (!queue_.empty() && (strip_visible || queue_.front().critical)) {
            showing_ = true;
            until_ = now_ms + kShowMs;
        }
    }
    if (!showing_) return nullptr;
    if (!strip_visible && !queue_.front().critical) return nullptr;
    return &queue_.front();
}

}  // namespace summit::shell
