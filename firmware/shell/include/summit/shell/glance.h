// Glance chips: which chips exist, which show, and which fit. Pure logic.
#pragma once
#include <string>
#include <vector>

#include "summit/panel_api.h"
#include "summit/value.h"

namespace summit::shell {

// Shell-owned chips: time (left) and the system icons on the right.
// Order: time · [panel chips] | wifi, update, rotation lock, battery.
std::vector<GlanceChipDef> system_chips();

// System chips plus panel chips, sorted for display (left side by order, then
// right side by order). Fails on a duplicate chip id.
bool collect_chips(const std::vector<GlanceChipDef> &panel_chips, std::vector<GlanceChipDef> &out,
                   std::string *error);

// Chip content from its state value: string -> text, true -> icon only.
struct ChipContent {
    bool visible = false;
    std::string text;
};
ChipContent chip_content(const Value &v);

struct ChipWidth {
    std::string id;
    uint8_t drop_rank;
    int order_key;  // display position; ties in rank drop the later chip first
    int width;
};

// Chips (already filtered to those with data) that fit in `available` px with
// `gap` between chips. Drops the highest drop_rank first; rank 0 never drops.
std::vector<std::string> fit_chips(const std::vector<ChipWidth> &chips, int available, int gap);

}  // namespace summit::shell
