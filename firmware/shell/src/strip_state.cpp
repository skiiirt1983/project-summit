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

void ToastQueue::push(Toast t)
{
    if (!queue_.empty() && queue_.back().text == t.text && queue_.back().icon == t.icon) return;
    if (queue_.size() >= kMaxQueued) {
        // Keep the one on screen; drop the oldest waiting toast.
        queue_.erase(queue_.begin() + (showing_ ? 1 : 0));
    }
    queue_.push_back(std::move(t));
}

const Toast *ToastQueue::current(uint32_t now_ms)
{
    if (showing_ && !before(now_ms, until_)) {
        queue_.pop_front();
        showing_ = false;
    }
    if (!showing_ && !queue_.empty()) {
        showing_ = true;
        until_ = now_ms + kShowMs;
    }
    return showing_ ? &queue_.front() : nullptr;
}

}  // namespace summit::shell
