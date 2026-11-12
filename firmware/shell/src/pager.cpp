#include "summit/shell/pager.h"

#include <algorithm>

namespace summit::shell {

std::vector<std::string> page_order(const std::vector<std::string> &registered, const std::string &setting,
                                    const std::vector<std::string> &hidden)
{
    std::vector<std::string> out;
    auto add = [&](const std::string &id) {
        if (std::find(hidden.begin(), hidden.end(), id) != hidden.end()) return;
        if (std::find(registered.begin(), registered.end(), id) != registered.end() &&
            std::find(out.begin(), out.end(), id) == out.end())
            out.push_back(id);
    };
    add(kHomePanel);
    size_t start = 0;
    while (start <= setting.size()) {
        size_t end = setting.find(',', start);
        if (end == std::string::npos) end = setting.size();
        std::string id = setting.substr(start, end - start);
        id.erase(0, id.find_first_not_of(' '));
        id.erase(id.find_last_not_of(' ') + 1);
        if (!id.empty()) add(id);
        start = end + 1;
    }
    for (const auto &id : registered) add(id);
    return out;
}

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
