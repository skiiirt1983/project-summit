#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "summit/registry.h"
#include "summit/boot/splash_timer.h"
#include "summit/shell/glance.h"
#include "summit/shell/layout.h"
#include "summit/shell/pager.h"
#include "summit/shell/rotation.h"
#include "summit/shell/motion.h"
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
    CHECK(q.current(0, true) == nullptr);
    q.push({"Print done", "", false, 0, ""}, 0);
    q.push({"Update available", "", false, 0, ""}, 0);
    q.push({"Print done", "", false, 0, ""}, 50);  // duplicate merged
    CHECK(q.pending() == 2);
    const Toast *t = q.current(100, true);
    CHECK(t && t->text == "Print done");
    CHECK(q.current(100 + ToastQueue::kShowMs - 1, true)->text == "Print done");
    q.push({"Print done", "", false, 0, ""}, 200);  // duplicate of the one showing: merged
    t = q.current(100 + ToastQueue::kShowMs, true);
    CHECK(t && t->text == "Update available");
    CHECK(q.current(100 + 2 * ToastQueue::kShowMs, true) == nullptr);
    for (int i = 0; i < 20; ++i) q.push({"n" + std::to_string(i), "", false, 0, ""}, 0);
    CHECK(q.pending() == ToastQueue::kMaxQueued);
}

TEST(toasts_wait_while_hidden_and_expire_after_10_min)
{
    ToastQueue q;
    q.push({"Print done", "", false, 0, ""}, 1000);
    CHECK(q.current(2000, false) == nullptr);               // hidden: waits
    const Toast *t = q.current(3000, true);
    CHECK(t && t->text == "Print done");                    // shown once visible
    ToastQueue e;
    e.push({"Old news", "", false, 0, ""}, 1000);
    CHECK(e.current(1000 + ToastQueue::kExpireMs - 1, false) == nullptr);
    CHECK(e.current(1000 + ToastQueue::kExpireMs, true) == nullptr);  // expired unseen
    CHECK(e.pending() == 0);
}

TEST(critical_toasts_show_immediately_even_when_hidden)
{
    ToastQueue q;
    q.push({"Print done", "", false, 0, ""}, 0);
    CHECK(q.current(10, true)->text == "Print done");
    q.push({"Battery low: 10%", "", true, 0, ""}, 20);          // interrupts the normal toast
    const Toast *t = q.current(30, false);                   // strip hidden: still shown
    CHECK(t && t->critical && t->text == "Battery low: 10%");
    t = q.current(30 + ToastQueue::kShowMs, true);
    CHECK(t && t->text == "Print done");                     // interrupted toast shown again
    q.push({"Alarm 07:00", "", true, 0, ""}, 40);
    q.push({"Alarm 07:00", "", false, 0, ""}, 50);              // merged into the critical one
    CHECK(q.pending() == 2);
    ToastQueue e;
    e.push({"Alarm", "", true, 0, ""}, 0);
    CHECK(e.current(ToastQueue::kExpireMs + 5, false) != nullptr);  // critical never expires
}

TEST(page_order_home_first_then_setting_then_rest)
{
    std::vector<std::string> reg = {"calendar", "home_clock", "weather", "news", "printer"};
    CHECK((page_order(reg, "") == std::vector<std::string>{"home_clock", "calendar", "weather", "news", "printer"}));
    CHECK((page_order(reg, "printer, news,bogus,printer,home_clock") ==
           std::vector<std::string>{"home_clock", "printer", "news", "calendar", "weather"}));
    CHECK((page_order({"news"}, "weather") == std::vector<std::string>{"news"}));
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

TEST(link_toast_keeps_its_action)
{
    ToastQueue q;
    q.push({"New recipe: Pie", "", false, 0, "recipes/open/pie"}, 0);
    const Toast *t = q.current(10, true);
    CHECK(t && t->text == "New recipe: Pie" && t->action == "recipes/open/pie" && !t->critical);
    q.push({"Print done", "", false, 0, ""}, 20);
    CHECK(q.current(20 + ToastQueue::kShowMs, true)->action.empty());  // plain toasts open nothing
}

TEST(motion_rubber_band_settle_and_dots)
{
    using namespace summit::shell::motion;
    // Rubber band: follows the finger at about half speed at first, flattens, keeps the sign, never passes the finger.
    CHECK(rubber_band(0, 1280) == 0 && rubber_band(100, 0) == 0);
    CHECK(rubber_band(100, 1280) > 40 && rubber_band(100, 1280) < 60);
    CHECK(rubber_band(-100, 1280) == -rubber_band(100, 1280));
    int last = 0;
    for (int x = 50; x <= 1280; x += 50) {
        const int r = rubber_band(x, 1280);
        CHECK(r > last && r < x);
        last = r;
    }
    CHECK(rubber_band(1280, 1280) < 1280 / 2);
    // Settle time: the full time from rest, shorter for a flick, never under the minimum or over the base.
    CHECK(settle_ms(1280, 0.0f) == kPageMs);
    CHECK(settle_ms(1000, 0.2f) == kPageMs);           // slow drag: 2 * 1000 / 0.2 is far over the base
    CHECK(settle_ms(400, 4.0f) == 200);                // flick: 2 * 400 / 4
    CHECK(settle_ms(-400, -4.0f) == 200);              // direction does not matter
    CHECK(settle_ms(50, 8.0f) == kMinSettleMs);
    CHECK(settle_ms(0, 0.0f) == kMinSettleMs);
    CHECK(settle_ms(900, 3.0f, 240) == 240);           // a caller's shorter base is the ceiling
    // Dots: fully shown for the hold time, then a linear fade to nothing.
    CHECK(dots_opacity(0) == 255 && dots_opacity(kDotsHoldMs) == 255);
    CHECK(dots_opacity(kDotsHoldMs + kDotsFadeMs / 2) > 100 && dots_opacity(kDotsHoldMs + kDotsFadeMs / 2) < 150);
    CHECK(dots_opacity(kDotsHoldMs + kDotsFadeMs) == 0 && dots_opacity(60000) == 0);
    // Things leave faster than they arrive; a press reacts faster than it relaxes.
    CHECK(kSheetOutMs < kSheetInMs && kScreenOutMs < kScreenInMs && kPressMs < kReleaseMs);
}

TEST(startup_wait_wifi_then_clock_with_limits)
{
    using summit::boot::startup_wait;
    using summit::boot::StartupWait;
    CHECK(startup_wait(false, false, false, 1000) == StartupWait::Wifi);
    CHECK(startup_wait(true, false, false, 1000) == StartupWait::Clock);
    CHECK(startup_wait(true, true, false, 1000) == StartupWait::Done);
    CHECK(startup_wait(false, false, true, 1000) == StartupWait::Done);        // setup AP: nothing to wait for
    CHECK(startup_wait(false, false, false, 19999) == StartupWait::Wifi);
    CHECK(startup_wait(false, false, false, 20000) == StartupWait::Done);      // never stuck on the boot screen
    CHECK(startup_wait(true, false, false, 20000) == StartupWait::Done);
}

TEST(splash_timer_min_time_and_ready)
{
    using summit::boot::SplashTimer;
    using summit::boot::kSplashMinMs;
    SplashTimer t;
    CHECK(!t.done(100000));                       // never started
    t.start(1000);
    CHECK(!t.done(1000 + kSplashMinMs + 5000));   // not ready: keeps showing however long it takes
    t.set_ready();
    CHECK(!t.done(1000 + kSplashMinMs - 1));      // ready early: still held to the minimum
    CHECK(t.done(1000 + kSplashMinMs));
    SplashTimer w;                                // millisecond counter wrapping
    w.start(0xFFFFFF00u);
    w.set_ready();
    CHECK(!w.done(0xFFFFFF00u + 1000) && w.done(0xFFFFFF00u + kSplashMinMs));
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
    RUN(toasts_wait_while_hidden_and_expire_after_10_min);
    RUN(critical_toasts_show_immediately_even_when_hidden);
    RUN(link_toast_keeps_its_action);
    RUN(startup_wait_wifi_then_clock_with_limits);
    RUN(splash_timer_min_time_and_ready);
    RUN(motion_rubber_band_settle_and_dots);
    RUN(page_order_home_first_then_setting_then_rest);
    RUN(rotation_debounces_400ms);
    RUN(rotation_lock_pins_and_long_press_toggles);
    RUN(pager_navigation_and_swipe);
    RUN(panels_register_chips_through_manifest);
    return report("shell logic");
}
