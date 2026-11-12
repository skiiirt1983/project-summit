// Pure logic behind the Recipes panel: library search/filter and the state of a
// recipe being cooked (servings, current step, ticked ingredients, book page).
#pragma once
#include <string>
#include <vector>

#include "summit/recipes/quantity.h"
#include "summit/recipes/recipe.h"
#include "summit/recipes/step_time.h"

namespace summit::recipes {

// Every tag used in the library, sorted, without duplicates.
std::vector<std::string> all_tags(const std::vector<IndexEntry> &entries);
// Entries whose title contains `search` (case-insensitive, ASCII) and that carry
// `tag` ("" = any). Order is kept (recently cooked first).
std::vector<IndexEntry> filter_library(const std::vector<IndexEntry> &entries, const std::string &search,
                                       const std::string &tag);

class CookSession {
public:
    CookSession() = default;
    CookSession(Recipe recipe, int pages);

    const Recipe &recipe() const { return recipe_; }
    bool card() const { return has_card(recipe_); }
    bool book() const { return pages_ > 0; }

    // -- servings (1..99); scaling only when the recipe states its servings
    bool can_scale() const { return recipe_.servings > 0; }
    int servings() const { return servings_; }
    bool change_servings(int delta);
    double factor() const { return can_scale() ? scale_factor(recipe_.servings, servings_) : 1.0; }
    std::string ingredient_text(size_t i) const;

    // -- ingredients ticked off
    bool ticked(size_t i) const { return i < ticked_.size() && ticked_[i]; }
    void toggle(size_t i);

    // -- steps (card mode)
    size_t step() const { return step_; }
    size_t step_count() const { return recipe_.steps.size(); }
    bool next_step();
    bool prev_step();
    const std::string &step_text() const;
    // Times to offer as timers for the current step: those found in its text, or the
    // step's own timer_s when the text has none.
    std::vector<TimeSpan> step_times() const;
    std::string step_label() const;  // "Step 2 of 5"

    // -- pages (book mode), 1-based
    int page() const { return page_; }
    int pages() const { return pages_; }
    bool next_page();
    bool prev_page();

private:
    Recipe recipe_;
    int pages_ = 0;
    int servings_ = 0;
    size_t step_ = 0;
    int page_ = 1;
    std::vector<bool> ticked_;
};

}  // namespace summit::recipes
