#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "summit/registry.h"
#include "summit/shell/glance.h"
#include "summit/shell/layout.h"
#include "summit/shell/pager.h"
#include "summit/shell/rotation.h"
#include "summit/shell/strip_state.h"

using namespace summit;
using namespace summit::shell;

TEST(tokens_are_6_percent_of_short_side_and_orientation_independent)
{
    Tokens l = tokens_for(1280, 720), p = tokens_for(720, 1280);
    CHECK(l.unit == 44);
    CHECK(p.unit == l.unit && p.text_m == l.text_m && p.space_s == l.space_s);
    CHECK(l.space_s == 11 && l.space_m == 22 && l.radius == 11);
    CHECK(l.text_s < l.text_m && l.text_m < l.text_l && l.text_l < l.text_xl);
}

TEST(layout_both_resolutions_content_never_under_strip)
{
    for (auto wh : {std::pair<int16_t, int16_t>{1280, 720}, {720, 1280}}) {
        Layout l = layout_for(wh.first, wh.second);
        CHECK(l.orientation == (wh.first > wh.second ? kLandscape : kPortrait));
        CHECK(l.strip.y == 0 && l.strip.h == l.tokens.unit && l.strip.w == wh.first);
        CHECK(l.content.y == l.strip.h);
        CHECK(l.content.y + l.content.h == wh.second);
        CHECK(l.content.w == wh.first);
        CHECK(l.strip_toast.h == 2 * l.tokens.unit);
        Layout full = layout_for(wh.first, wh.second, false);
        CHECK(full.content.y == 0 && full.content.h == wh.second);
    }
}

static std::vector<GlanceChipDef> demo_panel_chips()
{
    std::vector<GlanceChipDef> c(3);
    c[0].id = "next_event"; c[0].topic = "calendar/next_text"; c[0].order = 20; c[0].drop_rank = 1; c[0].panel_id = "calendar";
    c[1].id = "weather";    c[1].topic = "weather/now_text";   c[1].order = 30; c[1].drop_rank = 2; c[1].panel_id = "weather";
    c[2].id = "printer";    c[2].topic = "printer/progress";   c[2].order = 40; c[2].drop_rank = 3; c[2].panel_id = "printer";
    return c;
}

TEST(chips_collected_in_spec_order)
{
    std::vector<GlanceChipDef> all;
    std::string err;
    CHECK(collect_chips(demo_panel_chips(), all, &err));
    std::vector<std::string> ids;
    for (auto &c : all) ids.push_back(c.id);
    CHECK((ids == std::vector<std::string>{"time", "next_event", "weather", "printer",
                                          "wifi", "update", "rotation_lock", "battery"}));
    auto dup = demo_panel_chips();
    dup[0].id = "battery";
    CHECK(!collect_chips(dup, all, &err) && err == "duplicate glance chip id: battery");
}

TEST(chip_with_no_data_is_hidden)
{
    CHECK(!chip_content(Value{}).visible);
    CHECK(!chip_content(std::string()).visible);
    CHECK(!chip_content(false).visible);
    CHECK(chip_content(true).visible && chip_content(true).text.empty());
    ChipContent c = chip_content(std::string("4\xC2\xB0" "C"));
    CHECK(c.visible && c.text == "4\xC2\xB0" "C");
    CHECK(!chip_content(int64_t{3}).visible);
}

TEST(narrow_width_drops_printer_then_weather_then_event)
{
    std::vector<GlanceChipDef> all;
    collect_chips(demo_panel_chips(), all, nullptr);
    std::vector<ChipWidth> w;
    int key = 0;
    for (auto &c : all) w.push_back({c.id, c.drop_rank, key++, 100});
    // 8 chips x 100 + 7 gaps x 10 = 870
    CHECK(fit_chips(w, 870, 10).size() == 8);
    auto ids = fit_chips(w, 869, 10);
    CHECK(std::find(ids.begin(), ids.end(), "printer") == ids.end() && ids.size() == 7);
    ids = fit_chips(w, 759, 10);  // 7 chips need 760
    CHECK(std::find(ids.begin(), ids.end(), "weather") == ids.end() && ids.size() == 6);
    ids = fit_chips(w, 600, 10);
    CHECK((ids == std::vector<std::string>{"time", "wifi", "update", "rotation_lock", "battery"}));
    ids = fit_chips(w, 10, 10);  // time and battery (and system icons) never drop
    CHECK(std::find(ids.begin(), ids.end(), "time") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "battery") != ids.end());
}

TEST(strip_hidden_modes_and_tap_reveal_for_3s)
{
    StripVisibility v;
    CHECK(v.visible(0) && !v.reveal(0));
    v.set_reason(kHideAlarm, true);
    v.set_reason(kHideFullscreen, true);
    CHECK(!v.visible(100));
    CHECK(v.reveal(1000));
    CHECK(v.visible(1000) && v.visible(3999) && !v.visible(4000));
    v.set_reason(kHideAlarm, false);
    CHECK(!v.visible(5000));  // still full-screen
    v.set_reason(kHideFullscreen, false);
    CHECK(v.visible(5000));
    v.set_reason(kHideAmbient, true);
    CHECK(!v.visible(5001));  // an old reveal doesn't carry over
    v.set_reason(kHideAmbient, false);
    StripVisibility w;  // timestamps wrapping past 2^32
    w.set_reason(kHideUpdate, true);
    w.reveal(0xFFFFFF00u);
    CHECK(w.visible(0x00000100u) && !w.visible(0x00000C00u));
}

TEST(toasts_show_one_at_a_time_then_collapse)
{
    ToastQueue q;
    CHECK(q.current(0) == nullptr);
    q.push({"Print done", ""});
    q.push({"Print done", ""});  // duplicate ignored
    q.push({"Update available", ""});
    const Toast *t = q.current(100);
    CHECK(t && t->text == "Print done");
    CHECK(q.current(100 + ToastQueue::kShowMs - 1)->text == "Print done");
    t = q.current(100 + ToastQueue::kShowMs);
    CHECK(t && t->text == "Update available");
    CHECK(q.current(100 + 2 * ToastQueue::kShowMs) == nullptr);
    for (int i = 0; i < 20; ++i) q.push({"n" + std::to_string(i), ""});
    CHECK(q.pending() == ToastQueue::kMaxQueued);
}

TEST(rotation_debounces_400ms)
{
    RotationController r(kLandscape);
    r.report(kPortrait, 1000);
    CHECK(!r.update(1399) && r.applied() == kLandscape);
    CHECK(r.update(1400) && r.applied() == kPortrait);
    r.report(kLandscape, 2000);
    r.report(kPortrait, 2200);  // bounced back within the window
    CHECK(!r.update(3000) && r.applied() == kPortrait);
}

TEST(rotation_lock_pins_and_long_press_toggles)
{
    RotationController r(kPortrait);
    CHECK(r.toggled_lock() == "portrait");
    r.set_lock(r.toggled_lock());
    r.report(kLandscape, 0);
    CHECK(!r.update(1000) && r.applied() == kPortrait);
    CHECK(r.toggled_lock() == "off");
    r.set_lock("off");
    CHECK(r.update(1000) && r.applied() == kLandscape);  // sensor was already stable
    r.set_lock("portrait");
    CHECK(r.update(1001) && r.applied() == kPortrait);  // explicit lock applies now
    r.set_lock("bogus");
    CHECK(r.lock() == "off");
}

TEST(pager_navigation_and_swipe)
{
    Pager p({"home_clock", "weather", "news"});
    CHECK(p.current() == "home_clock" && !p.prev());
    CHECK(p.next() && p.current() == "weather");
    CHECK(p.go_to(std::string("news")) && !p.next());
    CHECK(!p.go_to(std::string("nope")) && !p.go_to(size_t{9}));
    CHECK(Pager::swipe_direction(-320, 1280, 0.0f) == +1);
    CHECK(Pager::swipe_direction(-319, 1280, 0.0f) == 0);
    CHECK(Pager::swipe_direction(-40, 1280, -0.8f) == +1);  // flick
    CHECK(Pager::swipe_direction(200, 720, 0.0f) == -1);
    CHECK(Pager::swipe_direction(30, 720, 0.1f) == 0);
}

struct ChipPanel : Panel {
    PanelManifest m;
    explicit ChipPanel(std::string id, std::string chip)
    {
        m.id = id; m.name = id; m.version = "1"; m.sizes = kSizeFull;
        GlanceChipDef c;
        c.id = chip; c.topic = id + "/glance";
        m.glances = {c};
    }
    const PanelManifest &manifest() const override { return m; }
    bool create(PanelContext &) override { return true; }
    void show(PanelContext &, const PanelView &) override {}
    void tick(PanelContext &, uint32_t) override {}
    void on_event(PanelContext &, const std::string &, const Value &) override {}
    void hide(PanelContext &) override {}
    void destroy(PanelContext &) override {}
};

TEST(panels_register_chips_through_manifest)
{
    PanelRegistry reg;
    CHECK(reg.add(std::make_unique<ChipPanel>("weather", "weather")));
    std::string err;
    CHECK(!reg.add(std::make_unique<ChipPanel>("weather2", "weather"), &err));
    CHECK(err == "duplicate glance chip id: weather");
    auto chips = reg.glance_chips();
    CHECK(chips.size() == 1 && chips[0].panel_id == "weather" && chips[0].topic == "weather/glance");
    auto bad = std::make_unique<ChipPanel>("x", "Bad Id");
    CHECK(!reg.add(std::move(bad), &err));
}

int main()
{
    RUN(tokens_are_6_percent_of_short_side_and_orientation_independent);
    RUN(layout_both_resolutions_content_never_under_strip);
    RUN(chips_collected_in_spec_order);
    RUN(chip_with_no_data_is_hidden);
    RUN(narrow_width_drops_printer_then_weather_then_event);
    RUN(strip_hidden_modes_and_tap_reveal_for_3s);
    RUN(toasts_show_one_at_a_time_then_collapse);
    RUN(rotation_debounces_400ms);
    RUN(rotation_lock_pins_and_long_press_toggles);
    RUN(pager_navigation_and_swipe);
    RUN(panels_register_chips_through_manifest);
    return report("shell logic");
}
