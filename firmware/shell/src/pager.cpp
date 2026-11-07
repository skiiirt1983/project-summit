#include "summit/shell/pager.h"

namespace summit::shell {

bool Pager::go_to(size_t index)
{
    if (index >= ids_.size() || index == index_) return false;
    index_ = index;
    return true;
}

bool Pager::go_to(const std::string &panel_id)
{
    for (size_t i = 0; i < ids_.size(); ++i) {
        if (ids_[i] == panel_id) return go_to(i);
    }
    return false;
}

int Pager::swipe_direction(int drag_dx, int width, float velocity_px_per_ms)
{
    const float kFlick = 0.5f;  // px/ms
    // Dragging left (negative dx) reveals the next page.
    if (drag_dx * 4 <= -width || (drag_dx < 0 && velocity_px_per_ms <= -kFlick)) return +1;
    if (drag_dx * 4 >= width || (drag_dx > 0 && velocity_px_per_ms >= kFlick)) return -1;
    return 0;
}

}  // namespace summit::shell
