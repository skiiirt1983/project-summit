#include "summit/shell/glance.h"

#include <algorithm>
#include <set>

namespace summit::shell {

std::vector<GlanceChipDef> system_chips()
{
    // Icons are LVGL symbol strings (Font Awesome private-use code points).
    std::vector<GlanceChipDef> c(5);
    c[0].id = "time";          c[0].topic = "time/now_text";         c[0].side = ChipSide::Left;  c[0].order = 0;
    c[1].id = "wifi";          c[1].topic = "net/wifi_connected";    c[1].side = ChipSide::Right; c[1].order = 10;
    c[1].icon = "\xEF\x87\xAB";  // LV_SYMBOL_WIFI
    c[2].id = "update";        c[2].topic = "update/available";      c[2].side = ChipSide::Right; c[2].order = 20;
    c[2].icon = "\xEF\x80\x99";  // LV_SYMBOL_DOWNLOAD
    c[3].id = "rotation_lock"; c[3].topic = "shell/rotation_locked"; c[3].side = ChipSide::Right; c[3].order = 30;
    c[3].icon = "\xEF\x81\xB9";  // LV_SYMBOL_LOOP
    c[4].id = "battery";       c[4].topic = "power/battery_text";    c[4].side = ChipSide::Right; c[4].order = 40;
    c[4].icon = "\xEF\x89\x80";  // LV_SYMBOL_BATTERY_FULL
    c[4].icon_topic = "power/battery_icon";
    return c;
}

bool collect_chips(const std::vector<GlanceChipDef> &panel_chips, std::vector<GlanceChipDef> &out,
                   std::string *error)
{
    std::vector<GlanceChipDef> all = system_chips();
    all.insert(all.end(), panel_chips.begin(), panel_chips.end());
    std::set<std::string> ids;
    for (const auto &c : all) {
        if (!ids.insert(c.id).second) {
            if (error) *error = "duplicate glance chip id: " + c.id;
            return false;
        }
    }
    std::stable_sort(all.begin(), all.end(), [](const GlanceChipDef &a, const GlanceChipDef &b) {
        if (a.side != b.side) return a.side == ChipSide::Left;
        return a.order < b.order;
    });
    out = std::move(all);
    return true;
}

ChipContent chip_content(const Value &v)
{
    ChipContent c;
    if (const std::string *s = std::get_if<std::string>(&v)) {
        c.visible = !s->empty();
        c.text = *s;
    } else if (const bool *b = std::get_if<bool>(&v)) {
        c.visible = *b;
    }
    return c;
}

std::vector<std::string> fit_chips(const std::vector<ChipWidth> &chips, int available, int gap)
{
    std::vector<ChipWidth> kept = chips;
    auto total = [&] {
        int t = 0;
        for (const auto &c : kept) t += c.width;
        if (!kept.empty()) t += gap * static_cast<int>(kept.size() - 1);
        return t;
    };
    while (total() > available) {
        auto victim = kept.end();
        for (auto it = kept.begin(); it != kept.end(); ++it) {
            if (it->drop_rank == 0) continue;
            if (victim == kept.end() || it->drop_rank > victim->drop_rank ||
                (it->drop_rank == victim->drop_rank && it->order_key > victim->order_key))
                victim = it;
        }
        if (victim == kept.end()) break;  // only never-drop chips left
        kept.erase(victim);
    }
    std::vector<std::string> ids;
    for (const auto &c : kept) ids.push_back(c.id);
    return ids;
}

}  // namespace summit::shell
