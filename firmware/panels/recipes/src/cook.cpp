#include "summit/recipes/cook.h"

#include <algorithm>
#include <cmath>

namespace summit::recipes {

namespace {

std::string lower(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

}  // namespace

std::vector<std::string> all_tags(const std::vector<IndexEntry> &entries)
{
    std::vector<std::string> tags;
    for (const IndexEntry &e : entries)
        for (const std::string &t : e.tags)
            if (std::find(tags.begin(), tags.end(), t) == tags.end()) tags.push_back(t);
    std::sort(tags.begin(), tags.end());
    return tags;
}

std::vector<IndexEntry> filter_library(const std::vector<IndexEntry> &entries, const std::string &search,
                                       const std::string &tag)
{
    std::vector<IndexEntry> out;
    const std::string needle = lower(search);
    for (const IndexEntry &e : entries) {
        if (!tag.empty() && std::find(e.tags.begin(), e.tags.end(), tag) == e.tags.end()) continue;
        if (!needle.empty() && lower(e.title).find(needle) == std::string::npos) continue;
        out.push_back(e);
    }
    return out;
}

CookSession::CookSession(Recipe recipe, int pages) : recipe_(std::move(recipe)), pages_(pages > 0 ? pages : 0)
{
    servings_ = recipe_.servings > 0 ? std::max(1, static_cast<int>(std::lround(recipe_.servings))) : 0;
    ticked_.assign(recipe_.ingredients.size(), false);
}

bool CookSession::change_servings(int delta)
{
    if (!can_scale()) return false;
    const int next = servings_ + delta;
    if (next < 1 || next > 99) return false;
    servings_ = next;
    return true;
}

std::string CookSession::ingredient_text(size_t i) const
{
    return i < recipe_.ingredients.size() ? format_ingredient(recipe_.ingredients[i], factor()) : "";
}

void CookSession::toggle(size_t i)
{
    if (i < ticked_.size()) ticked_[i] = !ticked_[i];
}

bool CookSession::next_step()
{
    if (step_ + 1 >= recipe_.steps.size()) return false;
    ++step_;
    return true;
}

bool CookSession::prev_step()
{
    if (step_ == 0) return false;
    --step_;
    return true;
}

const std::string &CookSession::step_text() const
{
    static const std::string empty;
    return step_ < recipe_.steps.size() ? recipe_.steps[step_].text : empty;
}

std::vector<TimeSpan> CookSession::step_times() const
{
    if (step_ >= recipe_.steps.size()) return {};
    std::vector<TimeSpan> t = find_times(recipe_.steps[step_].text);
    if (t.empty() && recipe_.steps[step_].timer_s > 0) t.push_back({0, 0, recipe_.steps[step_].timer_s});
    return t;
}

std::string CookSession::step_label() const
{
    if (recipe_.steps.empty()) return "";
    return "Step " + std::to_string(step_ + 1) + " of " + std::to_string(recipe_.steps.size());
}

bool CookSession::next_page()
{
    if (page_ >= pages_) return false;
    ++page_;
    return true;
}

bool CookSession::prev_page()
{
    if (page_ <= 1) return false;
    --page_;
    return true;
}

}  // namespace summit::recipes
