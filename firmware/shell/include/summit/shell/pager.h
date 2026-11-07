// Pager: one panel per page, in registry order; swipe left/right. Pure logic.
#pragma once
#include <string>
#include <vector>

namespace summit::shell {

class Pager {
public:
    explicit Pager(std::vector<std::string> panel_ids) : ids_(std::move(panel_ids)) {}

    size_t count() const { return ids_.size(); }
    size_t index() const { return index_; }
    const std::string &current() const { return ids_.at(index_); }
    const std::vector<std::string> &ids() const { return ids_; }

    // Returns false if out of range or already there. No wrap-around.
    bool go_to(size_t index);
    bool go_to(const std::string &panel_id);
    bool next() { return go_to(index_ + 1); }
    bool prev() { return index_ > 0 && go_to(index_ - 1); }

    // Swipe decision from a finished horizontal drag: +1 = next page, -1 = previous,
    // 0 = snap back. Commits past a quarter of the width, or on a fast flick.
    static int swipe_direction(int drag_dx, int width, float velocity_px_per_ms);

private:
    std::vector<std::string> ids_;
    size_t index_ = 0;
};

}  // namespace summit::shell
