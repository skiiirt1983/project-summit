// Project Summit PC simulator.
//
//   summit_sim --selftest [--out DIR]   headless: scripted checks + PNG screenshots
//   summit_sim --sdl                    interactive window (needs SDL2 at build time)
//
// Headless mode uses a virtual clock and a scripted pointer, so runs are
// deterministic and gestures go through LVGL's real input processing.
// It proves the shell's layout and behaviour on a PC, not on hardware.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "demo_panels.h"
#include "fake_update.h"
#include "http_sim.h"
#include "summit/web/api.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "png.h"
#include "summit/event_bus.h"
#include "summit/registry.h"
#include "summit/settings.h"
#include "summit/shell/shell.h"
#include "summit/state_store.h"
#include "summit/recipes/module.h"
#include "summit/recipes/panel.h"
#include "summit/recipes/store.h"
#include "summit/recipes/web_api.h"
#include "summit/storage.h"
#include "summit/system_settings.h"
#include "summit/timers.h"

#if SUMMIT_SIM_SDL
#include <SDL2/SDL.h>
#endif

using namespace summit;

namespace {

// ---- virtual clock + headless display + scripted pointer --------------------

uint32_t g_ms = 0;
uint32_t tick_cb() { return g_ms; }

struct Headless {
    lv_display_t *disp = nullptr;
    std::vector<uint8_t> fb;
    lv_indev_t *pointer = nullptr;
    lv_point_t point{0, 0};
    bool pressed = false;

    void set_size(int32_t w, int32_t h)
    {
        fb.assign(static_cast<size_t>(w) * h * 4, 0);
        if (!disp) {
            disp = lv_display_create(w, h);
            lv_display_set_flush_cb(disp, [](lv_display_t *d, const lv_area_t *, uint8_t *) {
                lv_display_flush_ready(d);
            });
        } else {
            lv_display_set_resolution(disp, w, h);
        }
        lv_display_set_buffers(disp, fb.data(), nullptr, static_cast<uint32_t>(fb.size()),
                               LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_obj_invalidate(lv_screen_active());
    }
    int32_t w() const { return lv_display_get_horizontal_resolution(disp); }
    int32_t h() const { return lv_display_get_vertical_resolution(disp); }
};

Headless g_hw;

struct FakeStorage : SettingsStorage {
    std::string blob, backup;
    bool has = false;
    ReadStatus read(std::string &out) override
    {
        if (!has) return ReadStatus::Empty;
        out = blob;
        return ReadStatus::Ok;
    }
    bool write(const std::string &b) override { blob = b; has = true; return true; }
    bool write_backup(const std::string &b) override { backup = b; return true; }
    ReadStatus read_backup(std::string &out) override { out = backup; return ReadStatus::Ok; }
};

struct App {
    EventBus bus{256};
    StateStore state{bus};
    PanelRegistry registry;
    FakeStorage storage;
    ModuleManager modules;
    std::string sd_dir;                 // local folder standing in for the SD card
    std::unique_ptr<PosixStorage> sd;
    int64_t sd_free = 14200;            // simulated free space (MB)
    std::unique_ptr<recipes::RecipeStore> recipe_store;
    std::unique_ptr<recipes::RecipeModuleData> recipe_data;
    recipes::RecipesPanel *recipes_panel = nullptr;  // owned by the registry
    std::unique_ptr<Settings> settings;
    std::unique_ptr<PanelHost> host;
    std::unique_ptr<shell::Shell> shell;
    std::unique_ptr<sim::FakeUpdateService> updates;
    std::unique_ptr<TimerService> timers;
};

void run_ms(App &app, uint32_t ms)
{
    const uint32_t end = g_ms + ms;
    while (g_ms < end) {
        g_ms += 4;
        if (app.sd) publish_storage_state(*app.sd, app.state);
        app.bus.dispatch();
        if (app.updates) app.updates->tick(g_ms);
        if (app.timers) app.timers->tick(g_ms);
        app.shell->tick(g_ms);
        lv_timer_handler();
    }
}

// ---- checks -------------------------------------------------------------------

int g_fail = 0, g_pass = 0;
#define EXPECT(cond, ...)                                     \
    do {                                                      \
        if (cond) {                                           \
            ++g_pass;                                         \
        } else {                                              \
            ++g_fail;                                         \
            std::printf("  FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                         \
            std::printf("\n");                                \
        }                                                     \
    } while (0)

std::string g_out = "sim-out";

void shot(const std::string &name)
{
    lv_refr_now(g_hw.disp);
    const int32_t w = g_hw.w(), h = g_hw.h();
    std::string path = g_out + "/" + name + ".png";
    bool ok = write_png(path, g_hw.fb.data(), w, h, w * 4);
    std::printf("  screenshot %s (%dx%d)%s\n", path.c_str(), w, h, ok ? "" : " FAILED");
    EXPECT(ok, "write %s", path.c_str());
}

// Every object of the current panel lies inside the content area (never under the strip).
void check_panel_inside_content(App &app, const char *where)
{
    const shell::Rect &c = app.shell->layout().content;
    int bad = 0, n = 0;
    std::vector<lv_obj_t *> stack{app.shell->page()};
    while (!stack.empty()) {
        lv_obj_t *o = stack.back();
        stack.pop_back();
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) stack.push_back(lv_obj_get_child(o, i));
        if (o == app.shell->page() || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        // What is drawn: the object clipped by its ancestors (scrolled content is clipped).
        bool visible = true;
        for (lv_obj_t *anc = lv_obj_get_parent(o); anc && anc != app.shell->page() && visible; anc = lv_obj_get_parent(anc)) {
            lv_area_t pa;
            lv_obj_get_coords(anc, &pa);
            a.x1 = std::max(a.x1, pa.x1);
            a.y1 = std::max(a.y1, pa.y1);
            a.x2 = std::min(a.x2, pa.x2);
            a.y2 = std::min(a.y2, pa.y2);
            visible = a.x1 <= a.x2 && a.y1 <= a.y2;
        }
        if (!visible) continue;
        ++n;
        if (a.y1 < c.y || a.x1 < c.x || a.x2 >= c.x + c.w || a.y2 >= c.y + c.h) {
            if (bad++ < 3)
                std::printf("    outside content: (%d,%d)-(%d,%d) content y=%d h=%d\n", (int)a.x1, (int)a.y1,
                            (int)a.x2, (int)a.y2, c.y, c.h);
        }
    }
    EXPECT(n > 0 && bad == 0, "%s: %d of %d panel objects outside the content area", where, bad, n);
}

bool has_chip(App &app, const char *id)
{
    for (const auto &c : app.shell->visible_chips())
        if (c == id) return true;
    return false;
}

std::string current_panel(App &app)
{
    Value v = app.state.get(shell::topics::kCurrentPanel);
    return std::get<std::string>(v);
}

// Pointer helpers (LVGL reads the pointer every LV_DEF_REFR_PERIOD ms).
void press(App &app, int32_t x, int32_t y)
{
    g_hw.point = {x, y};
    g_hw.pressed = true;
    run_ms(app, 40);
}
void move_to(App &app, int32_t x, int32_t y, int steps)
{
    lv_point_t from = g_hw.point;
    for (int i = 1; i <= steps; ++i) {
        g_hw.point = {from.x + (x - from.x) * i / steps, from.y + (y - from.y) * i / steps};
        run_ms(app, 20);
    }
}
void release(App &app)
{
    g_hw.pressed = false;
    run_ms(app, 40);
}
void tap(App &app, int32_t x, int32_t y)
{
    press(app, x, y);
    release(app);
}

lv_point_t chip_center(App &app, const char *id)
{
    // Chips are children of the strip row in display order; find by matching visible order.
    lv_obj_t *row = lv_obj_get_child(app.shell->strip(), 0);
    auto ids = app.shell->visible_chips();
    size_t vis = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); ++i) {
        lv_obj_t *o = lv_obj_get_child(row, i);
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_child_count(o) == 0) continue;
        if (vis < ids.size() && ids[vis] == id) {
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            return {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
        }
        ++vis;
    }
    return {-1, -1};
}

// ---- self-test ----------------------------------------------------------------

void selftest_orientation(App &app, const char *tag)
{
    const auto &l = app.shell->layout();
    std::printf("[%s] %dx%d, token=%d px\n", tag, (int)g_hw.w(), (int)g_hw.h(), l.tokens.unit);
    EXPECT(l.tokens.unit == 44, "token %d", l.tokens.unit);
    lv_area_t s;
    lv_obj_get_coords(app.shell->strip(), &s);
    EXPECT(s.y1 == 0 && lv_area_get_height(&s) == l.tokens.unit && lv_area_get_width(&s) == g_hw.w(),
           "strip %dx%d at y=%d", (int)lv_area_get_width(&s), (int)lv_area_get_height(&s), (int)s.y1);
    EXPECT(app.shell->strip_visible(), "strip visible");
    check_panel_inside_content(app, tag);

    // Time and battery never drop; dropped panel chips follow printer -> weather -> next event.
    auto chips = app.shell->visible_chips();
    std::string list;
    for (auto &c : chips) list += c + " ";
    std::printf("  chips: %s\n", list.c_str());
    EXPECT(has_chip(app, "time") && has_chip(app, "battery"), "time+battery: %s", list.c_str());
    const bool ev = has_chip(app, "next_event"), wx = has_chip(app, "weather"), pr = has_chip(app, "printer");
    EXPECT((!pr || wx) && (!wx || ev), "drop order printer>weather>next_event violated: %s", list.c_str());
    EXPECT(!has_chip(app, "rotation_lock"), "lock chip hidden while unlocked");
}

void selftest(App &app)
{
    // --- Landscape 1280x720 ---
    selftest_orientation(app, "landscape");
    EXPECT(has_chip(app, "printer") && has_chip(app, "weather") && has_chip(app, "next_event"),
           "all panel chips fit in landscape");
    shot("01-landscape-home");

    // Chip with no data is hidden.
    app.state.set("printer/progress_text", std::string());
    run_ms(app, 50);
    EXPECT(!has_chip(app, "printer"), "printer chip hidden without data");
    app.state.set("printer/progress_text", std::string("Enclosure v2 \xE2\x80\xA2 42%"));
    run_ms(app, 50);

    // Tap a chip -> its panel.
    lv_point_t p = chip_center(app, "weather");
    tap(app, p.x, p.y);
    run_ms(app, 300);
    EXPECT(current_panel(app) == "weather", "tap weather chip -> %s", current_panel(app).c_str());
    check_panel_inside_content(app, "weather landscape");
    EXPECT(lv_obj_get_child_count(app.shell->page()) == 1, "only the current panel holds LVGL objects");
    shot("02-landscape-weather");

    // Swipe left -> next page, swipe right -> back.
    press(app, 900, 400);
    move_to(app, 300, 410, 10);
    release(app);
    run_ms(app, 300);
    EXPECT(current_panel(app) == "news", "swipe left -> %s", current_panel(app).c_str());
    press(app, 300, 400);
    move_to(app, 900, 400, 10);
    release(app);
    run_ms(app, 300);
    EXPECT(current_panel(app) == "weather", "swipe right -> %s", current_panel(app).c_str());
    press(app, 700, 400);  // short drag snaps back
    move_to(app, 650, 400, 4);
    g_hw.point = {650, 400};
    run_ms(app, 200);
    release(app);
    run_ms(app, 300);
    EXPECT(current_panel(app) == "weather" && lv_obj_get_x(app.shell->page()) == 0, "short drag snaps back");

    // Swipe down from the strip -> quick settings; tap backdrop -> closed.
    // Start on a chip: a swipe must not count as a chip tap.
    lv_point_t on_chip = chip_center(app, "printer");
    press(app, on_chip.x, on_chip.y);
    move_to(app, on_chip.x, 300, 6);
    release(app);
    run_ms(app, 300);
    EXPECT(app.shell->quick_settings_open(), "swipe down from strip opens quick settings");
    EXPECT(current_panel(app) == "weather", "swipe starting on a chip is not a tap (%s)", current_panel(app).c_str());
    shot("03-landscape-quick-settings");
    {
        // Swipe up on the sheet closes it; reopen for the tap-outside check.
        lv_area_t sh;
        lv_obj_get_coords(app.shell->quick_settings(), &sh);
        press(app, 640, sh.y2 - 10);
        move_to(app, 640, sh.y1 - 200, 6);
        release(app);
        EXPECT(!app.shell->quick_settings_open(), "swipe up closes quick settings");
        app.shell->open_quick_settings();
        run_ms(app, 300);
    }
    tap(app, 640, g_hw.h() - 20);
    EXPECT(!app.shell->quick_settings_open(), "tap outside closes quick settings");

    // Long-press the strip -> rotation lock (pins current orientation), again -> off.
    press(app, 640, 20);
    run_ms(app, 700);
    release(app);
    run_ms(app, 50);
    EXPECT(std::get<std::string>(app.settings->get("ui/rotation_lock")) == "landscape", "long-press locks");
    EXPECT(has_chip(app, "rotation_lock"), "lock chip shown while locked");
    app.state.set("sensor/orientation", std::string("portrait"));
    run_ms(app, 600);
    EXPECT(g_hw.w() == 1280, "locked: rotation ignored");
    press(app, 640, 20);
    run_ms(app, 700);
    release(app);
    run_ms(app, 50);
    EXPECT(std::get<std::string>(app.settings->get("ui/rotation_lock")) == "off", "long-press unlocks");
    // Tapping the lock chip turns the lock off.
    press(app, 640, 20);
    run_ms(app, 700);
    release(app);
    run_ms(app, 50);
    lv_point_t lock_chip = chip_center(app, "rotation_lock");
    tap(app, lock_chip.x, lock_chip.y);
    EXPECT(std::get<std::string>(app.settings->get("ui/rotation_lock")) == "off" && !has_chip(app, "rotation_lock"),
           "tap lock chip unlocks");
    app.state.set("sensor/orientation", std::string("landscape"));
    for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);  // lock/unlock toasts
    EXPECT(!app.shell->toast_showing(), "lock/unlock toasts done");

    // Toast expands the strip, then collapses.
    app.bus.publish(shell::topics::kToast, std::string("Print finished: Enclosure v2"));
    run_ms(app, 100);
    lv_area_t s;
    lv_obj_get_coords(app.shell->strip(), &s);
    EXPECT(app.shell->toast_showing() && lv_area_get_height(&s) == 88, "toast expands strip to 2 tokens (%d)",
           (int)lv_area_get_height(&s));
    shot("04-landscape-toast");
    run_ms(app, shell::ToastQueue::kShowMs + 100);
    lv_obj_get_coords(app.shell->strip(), &s);
    EXPECT(!app.shell->toast_showing() && lv_area_get_height(&s) == 44, "toast collapses");

    // Full-screen mode hides the strip; tap reveals it for 3 s.
    app.state.set("ui/fullscreen", true);
    run_ms(app, 50);
    EXPECT(!app.shell->strip_visible() && app.shell->layout().content.y == 0, "full-screen hides strip");
    check_panel_inside_content(app, "fullscreen");
    shot("05-landscape-fullscreen");
    tap(app, 640, 400);
    EXPECT(app.shell->strip_visible(), "tap reveals strip");
    run_ms(app, 3100);
    EXPECT(!app.shell->strip_visible(), "strip hides again after 3 s");

    // While hidden, normal toasts wait; critical toasts show at once as an overlay.
    app.bus.publish(shell::topics::kToast, std::string("Print finished: Enclosure v2"));
    run_ms(app, 100);
    EXPECT(!app.shell->strip_visible() && !app.shell->toast_showing(), "normal toast waits while hidden");
    app.bus.publish(shell::topics::kToastCritical, std::string("Battery low: 10%"));
    run_ms(app, 100);
    EXPECT(app.shell->strip_visible() && app.shell->toast_showing() && app.shell->layout().content.y == 0,
           "critical toast shows in full-screen (overlay)");
    shot("05b-landscape-fullscreen-critical-toast");
    run_ms(app, shell::ToastQueue::kShowMs + 100);
    EXPECT(!app.shell->strip_visible(), "strip hidden again after the critical toast");

    app.state.set("ui/fullscreen", false);
    run_ms(app, 50);
    EXPECT(app.shell->strip_visible() && app.shell->layout().content.y == 44, "strip back after full-screen");
    EXPECT(app.shell->toast_showing(), "waiting toast shows once the strip is back");
    run_ms(app, shell::ToastQueue::kShowMs + 100);

    // Chip targets: time -> Home, update -> Settings > Updates request, Wi-Fi/battery -> quick settings.
    std::string opened;
    auto sub = app.bus.subscribe(shell::topics::kOpenScreen,
                                 [&](const std::string &, const Value &v) { opened = std::get<std::string>(v); });
    lv_point_t c = chip_center(app, "time");
    tap(app, c.x, c.y);
    run_ms(app, 300);
    EXPECT(current_panel(app) == "home_clock", "tap time -> Home (%s)", current_panel(app).c_str());
    c = chip_center(app, "update");
    tap(app, c.x, c.y);
    run_ms(app, 50);
    EXPECT(opened == "settings/updates" && !app.shell->quick_settings_open() && app.shell->updates().is_open(),
           "tap update -> Updates screen (%s)", opened.c_str());
    app.shell->updates().close();
    app.bus.unsubscribe(sub);
    for (const char *id : {"wifi", "battery"}) {
        c = chip_center(app, id);
        tap(app, c.x, c.y);
        run_ms(app, 300);
        EXPECT(app.shell->quick_settings_open(), "tap %s -> quick settings", id);
        app.shell->close_quick_settings();
    }

    // Settings notice: critical toast + row in quick settings with a two-tap reset.
    app.state.set(shell::topics::kSettingsNotice, std::string(notice_message(SettingsNotice::NewerVersion)));
    run_ms(app, 100);
    EXPECT(app.shell->toast_showing(), "settings notice raises a toast");
    app.shell->open_quick_settings();
    run_ms(app, 300);
    EXPECT(app.shell->settings_notice_shown(), "notice row in quick settings");
    shot("03b-landscape-settings-notice");
    app.settings->set("display/brightness", int64_t{30});
    lv_area_t rb;
    lv_obj_get_coords(app.shell->reset_button(), &rb);
    tap(app, (rb.x1 + rb.x2) / 2, (rb.y1 + rb.y2) / 2);
    EXPECT(std::get<int64_t>(app.settings->get("display/brightness")) == 30, "first tap only arms the reset");
    tap(app, (rb.x1 + rb.x2) / 2, (rb.y1 + rb.y2) / 2);
    run_ms(app, 50);
    EXPECT(std::get<int64_t>(app.settings->get("display/brightness")) == 80 && !app.shell->settings_notice_shown() &&
               !app.storage.backup.empty(),
           "second tap resets (old settings backed up), notice cleared");
    app.shell->close_quick_settings();
    for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);

    // Page order: Home first, then ui/page_order, then the rest; applied live.
    EXPECT((app.shell->pager().ids() ==
            std::vector<std::string>{"home_clock", "calendar", "weather", "news", "printer"}),
           "default page order");
    app.shell->go_to_panel("weather", false);
    app.settings->set("ui/page_order", std::string("printer,news,home_clock"));
    run_ms(app, 50);
    EXPECT((app.shell->pager().ids() ==
            std::vector<std::string>{"home_clock", "printer", "news", "calendar", "weather"}) &&
               current_panel(app) == "weather",
           "page order from settings, current page kept");
    app.settings->set("ui/page_order", std::string());
    run_ms(app, 50);

    // --- Updates screen, driven end to end through the real UpdateEngine ---
    {
        auto &up = app.shell->updates();
        auto click = [&](lv_obj_t *o) {
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            tap(app, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
        };
        auto visible = [](lv_obj_t *o) { return !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN); };
        auto stage = [&] { return std::get<std::string>(app.state.get("update/stage")); };
        lv_point_t uc = chip_center(app, "update");
        tap(app, uc.x, uc.y);
        run_ms(app, 100);
        EXPECT(up.is_open() && visible(up.check_button()), "Updates screen opens from the update chip");
        shot("10-landscape-updates");
        click(up.check_button());
        run_ms(app, 100);
        EXPECT(stage() == "checking", "check -> checking (%s)", stage().c_str());
        run_ms(app, 400);
        EXPECT(stage() == "available" && visible(up.install_button()) &&
                   std::get<std::string>(app.state.get("update/available_version")) == "0.1.0",
               "stable channel: 0.1.0 available (%s)", up.status_text().c_str());
        shot("11-landscape-update-available");

        // Pre-flight: on battery below 40% the install is refused with a reason.
        app.state.set("power/usb_power", false);
        app.state.set("power/battery_pct", int64_t{20});
        click(up.install_button());
        run_ms(app, 100);
        EXPECT(stage() == "failed" && up.status_text().find("40%") != std::string::npos, "pre-flight blocks: %s",
               up.status_text().c_str());
        app.state.set("power/usb_power", true);

        // Install: full-screen progress, strip hidden, then ready to restart.
        click(up.install_button());
        run_ms(app, 40);
        EXPECT(up.showing_progress() && !app.shell->strip_visible() && app.shell->layout().content.y == 0,
               "installing: full-screen progress, strip hidden");
        shot("12-landscape-update-progress");
        run_ms(app, 1500);
        EXPECT(stage() == "ready_to_reboot" && visible(up.restart_button()) && !up.showing_progress() &&
                   app.shell->strip_visible(),
               "image verified (size + SHA-256) and ready to restart (%s)", up.status_text().c_str());
        shot("13-landscape-update-ready");
        click(up.restart_button());
        run_ms(app, 100);
        EXPECT(std::get<std::string>(app.state.get("system/firmware_version")) == "0.1.0", "restart runs 0.1.0");

        // Beta channel: selecting it re-checks; the beta build appears.
        lv_area_t m;
        lv_obj_get_coords(up.channel_buttons(), &m);
        tap(app, m.x1 + lv_area_get_width(&m) / 2, (m.y1 + m.y2) / 2);
        run_ms(app, 500);
        EXPECT(std::get<std::string>(app.settings->get("update/channel")) == "beta" &&
                   std::get<std::string>(app.state.get("update/available_version")) == "0.2.0-beta.1",
               "beta channel offers 0.2.0-beta.1");
        app.settings->set("update/channel", std::string("stable"));  // e.g. from the web UI
        run_ms(app, 500);
        EXPECT(std::get<std::string>(app.state.get("update/available_version")) == "",
               "back on stable: re-checked, 0.1.0 is installed so nothing newer (%s)", up.status_text().c_str());
        EXPECT(visible(up.previous_button()), "previous version offered");
        // Previous version asks first; Cancel does nothing, Yes sends the command.
        std::string cmd;
        auto csub = app.bus.subscribe("update/command",
                                      [&](const std::string &, const Value &v) { cmd = std::get<std::string>(v); });
        click(up.previous_button());
        run_ms(app, 50);
        EXPECT(up.confirm_shown() && cmd.empty() &&
                   up.confirm_text() ==
                       "Go back to v0.0.1-alpha.3? Settings from the newer version may become read-only.",
               "previous version asks for confirmation (%s)", up.confirm_text().c_str());
        shot("13b-landscape-previous-confirm");
        click(up.confirm_cancel());
        run_ms(app, 50);
        EXPECT(!up.confirm_shown() && cmd.empty(), "cancel: nothing happens");
        click(up.previous_button());
        run_ms(app, 50);
        click(up.confirm_yes());
        run_ms(app, 50);
        EXPECT(!up.confirm_shown() && cmd == "previous", "yes: previous version requested");
        app.bus.unsubscribe(csub);
        up.close();
        run_ms(app, 50);
        EXPECT(!up.is_open(), "Updates screen closes");
        sim::publish_placeholders(app.state);
        for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);
    }

    // --- Modules screen: requirements, install adds the page, remove hides it ---
    {
        auto &ms = *app.shell->modules_screen();
        auto click = [&](lv_obj_t *o) {
            run_ms(app, 40);  // let a just-changed row (Delete button shown/hidden) settle
            lv_obj_update_layout(o);
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            tap(app, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
            run_ms(app, 60);
        };
        auto pages = [&] { return app.shell->pager().ids(); };
        auto has_page = [&](const char *id) {
            for (const auto &p : pages())
                if (p == id) return true;
            return false;
        };
        EXPECT(!has_page("recipes"), "uninstalled module has no page");
        app.shell->open_quick_settings();
        run_ms(app, 300);
        click(app.shell->modules_button());
        EXPECT(ms.is_open() && !app.shell->quick_settings_open(), "Modules opens from quick settings");
        EXPECT(ms.action_text("recipes") == "Install" && ms.status_text("recipes") == "Needs a microSD card with 200 MB free",
               "module listed with requirements (%s)", ms.status_text("recipes").c_str());
        shot("15-landscape-modules");

        app.sd->set_present(false);
        click(ms.action_button("recipes"));
        EXPECT(ms.status_text("recipes") == "Insert a microSD card" && !has_page("recipes"),
               "install without a card explains what is missing (%s)", ms.status_text("recipes").c_str());
        shot("16-landscape-modules-missing-sd");
        app.sd->set_present(true);
        app.sd_free = 120;
        click(ms.action_button("recipes"));
        EXPECT(ms.status_text("recipes").find("200 MB needed, 120 MB free") != std::string::npos, "low space explained (%s)",
               ms.status_text("recipes").c_str());
        app.sd_free = 14200;
        click(ms.action_button("recipes"));
        EXPECT(ms.action_text("recipes") == "Remove" && ms.status_text("recipes") == "Installed" && has_page("recipes") &&
                   pages().back() == "recipes",
               "install adds the page at the end");
        ms.close();
        app.shell->go_to_panel("recipes", false);
        run_ms(app, 100);
        check_panel_inside_content(app, "recipes placeholder");
        // Card pulled later: still installed, the panel says what to do.
        app.sd->set_present(false);
        run_ms(app, 100);
        EXPECT(has_page("recipes") && current_panel(app) == "recipes", "module stays installed without the card");
        shot("17-landscape-recipes-no-sd");
        app.sd->set_present(true);
        // Remove while its page is showing: back to Home, page gone, setting off.
        app.bus.publish(shell::topics::kOpenScreen, std::string("settings/modules"));
        run_ms(app, 100);
        click(ms.action_button("recipes"));
        EXPECT(!has_page("recipes") && current_panel(app) == "home_clock" && ms.action_text("recipes") == "Install",
               "remove hides the page and returns to Home (%s)", current_panel(app).c_str());
        EXPECT(std::get<bool>(app.settings->get("modules/recipes/installed")) == false, "setting cleared");
        ms.close();
        for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);
    }

    // --- Recipes panel: library, search/tags, card mode ---
    {
        using recipes::RecipesPanel;
        RecipesPanel &rp = *app.recipes_panel;
        auto click = [&](lv_obj_t *o) {
            EXPECT(o != nullptr, "control exists");
            if (!o) return;
            lv_obj_scroll_to_view_recursive(o, LV_ANIM_OFF);
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            tap(app, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
            run_ms(app, 60);
        };
        auto flag = [&](const char *k) { Value v = app.state.get(k); return std::get_if<bool>(&v) && std::get<bool>(v); };
        EXPECT(app.modules.install(*app.settings, "recipes", {true, 14200}) == ModuleResult::Ok, "module installed");
        run_ms(app, 50);
        app.shell->go_to_panel("recipes", false);
        run_ms(app, 100);
        EXPECT(rp.view() == RecipesPanel::View::Library &&
                   (rp.visible_ids() == std::vector<std::string>{"apple-pie", "grandmas-stew", "pancakes", "tomato-soup"}),
               "library lists the recipes on the card");
        check_panel_inside_content(app, "recipes library");
        EXPECT(rp.thumbnail_ids() == std::vector<std::string>{"apple-pie"},
               "only recipes with a thumb.jpg show a thumbnail (%zu)", rp.thumbnail_ids().size());
        shot("20-landscape-recipes-library");
        rp.set_search("SOUP");
        EXPECT((rp.visible_ids() == std::vector<std::string>{"tomato-soup"}), "search by title");
        rp.set_search("");
        click(rp.find(RecipesPanel::kTag, 2));  // tags: baking, breakfast, ...
        EXPECT((rp.visible_ids() == std::vector<std::string>{"pancakes"}), "filter by tag");
        click(rp.find(RecipesPanel::kTag, 0));
        EXPECT(rp.visible_ids().size() == 4, "All shows everything");

        click(rp.find(RecipesPanel::kOpen, 2));
        EXPECT(rp.view() == RecipesPanel::View::Card && rp.session().recipe().title == "Pancakes", "tap opens card mode");
        EXPECT(flag("ui/swipe_lock") && flag("ui/keep_awake") &&
                   std::get<std::string>(app.state.get("recipes/cooking")) == "Pancakes",
               "open recipe: swipes stay in the recipe, screen kept awake, cooking flagged");
        check_panel_inside_content(app, "recipe card landscape");
        shot("21-landscape-recipe-card");
        click(rp.find(RecipesPanel::kPlus));
        click(rp.find(RecipesPanel::kPlus));
        EXPECT(rp.session().servings() == 6 && rp.session().ingredient_text(0) == "2 1/4 cup flour, sifted",
               "servings + scales quantities (%s)", rp.session().ingredient_text(0).c_str());
        click(rp.find(RecipesPanel::kTick, 1));
        EXPECT(rp.session().ticked(1), "tap ticks an ingredient");
        click(rp.find(RecipesPanel::kNext));
        EXPECT(rp.session().step() == 1, "Next goes to step 2");
        std::string timer;
        auto tsub = app.bus.subscribe("timers/start", [&](const std::string &, const Value &v) { timer = std::get<std::string>(v); });
        click(rp.find(RecipesPanel::kTimer, 0));
        EXPECT(timer == "1800|Pancakes: Step 2 of 3", "tap a time requests a timer (%s)", timer.c_str());
        app.bus.unsubscribe(tsub);
        // The timer runs: bar in the panel, chip in the strip, counting down.
        auto chip_text = [&] { return std::get<std::string>(app.state.get("timers/chip_text")); };
        EXPECT(app.timers->timers().all().size() == 1 && chip_text() == "30:00" && has_chip(app, "timers"),
               "timer started, chip shows the countdown (%s)", chip_text().c_str());
        run_ms(app, 2000);
        EXPECT(chip_text() == "29:58", "chip counts down (%s)", chip_text().c_str());
        EXPECT(rp.find(RecipesPanel::kCancelTimer, app.timers->timers().all()[0].id) != nullptr, "timer listed in the panel");
        shot("22-landscape-recipe-step-timer");
        // A second, short timer: both run; when it ends a critical toast shows and it rings until stopped.
        app.bus.publish("timers/start", std::string("3|Eggs"));
        run_ms(app, 500);
        EXPECT(app.timers->timers().all().size() == 2 && chip_text() == "0:03 +1", "two timers at once (%s)", chip_text().c_str());
        run_ms(app, 2800);  // it ends while the "Timer started" toast may still be up: critical interrupts
        EXPECT(flag("timers/ringing") && chip_text() == "Timer done" && app.shell->toast_showing(),
               "finished timer rings and shows a toast (%s)", chip_text().c_str());
        shot("22b-landscape-timer-done");
        click(rp.find(RecipesPanel::kDismissTimers));
        EXPECT(!flag("timers/ringing") && app.timers->timers().all().size() == 1, "Stop silences the finished timer");
        press(app, 1100, 420);  // swipe left on the step: next step, not next page
        move_to(app, 700, 425, 8);
        release(app);
        run_ms(app, 100);
        EXPECT(rp.session().step() == 2 && current_panel(app) == "recipes", "swipe left = next step, page unchanged");
        press(app, 700, 420);
        move_to(app, 1100, 425, 8);
        release(app);
        run_ms(app, 100);
        EXPECT(rp.session().step() == 1, "swipe right = previous step");
        app.bus.publish("input/double_tap", true);  // double-tap on the case
        run_ms(app, 60);
        EXPECT(rp.session().step() == 2, "double-tap = next step");

        // Another page: normal swiping and dimming again; the recipe stays open.
        app.shell->go_to_panel("home_clock", false);
        run_ms(app, 100);
        EXPECT(!flag("ui/swipe_lock") && !flag("ui/keep_awake") &&
                   std::get<std::string>(app.state.get("recipes/cooking")) == "Pancakes",
               "leaving the page restores swipes and idle dim; still cooking");
        // On another page: "Cooking now" and timer chips; tapping the cooking chip returns to the recipe.
        EXPECT(has_chip(app, "cooking") && has_chip(app, "timers") && has_chip(app, "time") && has_chip(app, "battery"),
               "Cooking now + timer chips on other pages");
        for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);
        shot("22c-landscape-home-cooking-chips");
        lv_point_t cc = chip_center(app, "cooking");
        tap(app, cc.x, cc.y);
        run_ms(app, 400);
        EXPECT(current_panel(app) == "recipes" && !has_chip(app, "cooking"), "cooking chip returns to the recipe and hides");
        click(rp.find(RecipesPanel::kCancelTimer, app.timers->timers().all()[0].id));
        EXPECT(app.timers->timers().all().empty() && !has_chip(app, "timers"), "Cancel removes the timer and its chip");
        for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);
        EXPECT(rp.view() == RecipesPanel::View::Card && rp.session().step() == 2 && rp.session().ticked(1),
               "coming back resumes the same step");
        click(rp.find(RecipesPanel::kBack));
        EXPECT(rp.view() == RecipesPanel::View::Library && !flag("ui/swipe_lock") &&
                   std::get<std::string>(app.state.get("recipes/cooking")).empty(),
               "back returns to the library");

        // Book mode: the original pages as images.
        {
            auto img_w = [&] { return rp.page_image() ? lv_obj_get_width(rp.page_image()) : 0; };
            auto img_h = [&] { return rp.page_image() ? lv_obj_get_height(rp.page_image()) : 0; };
            rp.open_recipe("apple-pie");
            run_ms(app, 100);
            EXPECT(rp.view() == RecipesPanel::View::Card && rp.session().book(), "recipe with steps and pages opens in card mode");
            click(rp.find(RecipesPanel::kPages));
            EXPECT(rp.view() == RecipesPanel::View::Book && rp.session().page() == 1 && rp.page_image(), "Pages shows book mode");
            const int32_t area_w = lv_obj_get_content_width(lv_obj_get_parent(rp.page_image()));
            EXPECT(img_w() == area_w && img_h() == area_w * 1280 / 905, "landscape: page fits the width (%dx%d in %d)",
                   (int)img_w(), (int)img_h(), (int)area_w);
            check_panel_inside_content(app, "book mode landscape");
            shot("26-landscape-recipe-book");
            // The page really decoded: the image area is not one flat colour.
            {
                lv_refr_now(g_hw.disp);
                lv_area_t a;
                lv_obj_get_coords(rp.page_image(), &a);
                int distinct = 0;
                uint32_t first = 0;
                for (int32_t y = a.y1 + 10; y < a.y1 + 300 && y < g_hw.h(); y += 7)
                    for (int32_t x = a.x1 + 10; x < a.x2 - 10; x += 11) {
                        uint32_t px;
                        std::memcpy(&px, &g_hw.fb[(static_cast<size_t>(y) * g_hw.w() + x) * 4], 4);
                        if (distinct == 0) first = px, distinct = 1;
                        else if (px != first) ++distinct;
                    }
                EXPECT(distinct > 50, "page image pixels are drawn (%d differing samples)", distinct);
            }
            click(rp.find(RecipesPanel::kNext));
            EXPECT(rp.session().page() == 2, "Next = page 2");
            click(rp.find(RecipesPanel::kZoom, 1));
            EXPECT(rp.zoomed() && img_w() == 2 * area_w, "zoom doubles the page");
            shot("27-landscape-recipe-book-zoom");
            click(rp.find(RecipesPanel::kZoom, 1));
            EXPECT(!rp.zoomed() && img_w() == area_w, "fit again");
            press(app, 500, 300);
            move_to(app, 900, 305, 8);
            release(app);
            run_ms(app, 100);
            EXPECT(rp.session().page() == 1 && current_panel(app) == "recipes", "swipe right = previous page");
            app.bus.publish("input/double_tap", true);
            run_ms(app, 60);
            EXPECT(rp.session().page() == 2, "double-tap on the case = next page");
            click(rp.find(RecipesPanel::kSteps));
            EXPECT(rp.view() == RecipesPanel::View::Card, "Steps returns to card mode");
            rp.open_recipe("grandmas-stew");
            run_ms(app, 100);
            EXPECT(rp.view() == RecipesPanel::View::Book && rp.session().pages() == 3 && !rp.find(RecipesPanel::kSteps),
                   "a scan (pages only) opens straight in book mode");
            shot("28-landscape-recipe-scan");
            rp.back_to_library();
            run_ms(app, 100);
        }

        // New PDFs in the inbox: notice with the web UI address.
        app.state.set("net/ip", std::string("192.168.1.42"));
        app.sd->write("/recipes/inbox/grandmas-stew.pdf", "%PDF-1.4");
        app.sd->write("/recipes/inbox/scan0001.pdf", "%PDF-1.4");
        app.shell->go_to_panel("home_clock", false);
        app.shell->go_to_panel("recipes", false);
        run_ms(app, 100);
        EXPECT(app.recipe_store->inbox().size() == 2, "inbox PDFs found");
        check_panel_inside_content(app, "recipes library with inbox notice");
        shot("23-landscape-recipes-inbox");

        // Card pulled / re-inserted.
        app.sd->set_present(false);
        run_ms(app, 100);
        EXPECT(rp.view() == RecipesPanel::View::NoCard, "no card: message instead of the library");
        app.sd->set_present(true);
        run_ms(app, 100);
        EXPECT(rp.view() == RecipesPanel::View::Library && rp.visible_ids().size() == 4, "card back: library again");
        app.shell->go_to_panel("home_clock", false);
        run_ms(app, 100);
    }

    // --- Rotate to portrait 720x1280 (400 ms debounce, then re-layout) ---
    app.shell->go_to_panel("home_clock", false);
    app.state.set("sensor/orientation", std::string("portrait"));
    run_ms(app, 300);
    EXPECT(g_hw.w() == 1280, "no rotation before 400 ms");
    run_ms(app, 200);
    EXPECT(g_hw.w() == 720 && g_hw.h() == 1280, "rotated after debounce");
    run_ms(app, 400);  // morph
    selftest_orientation(app, "portrait");
    shot("06-portrait-home");

    // Narrow width: lengthen chip texts so they must drop (printer first, then weather).
    app.state.set("calendar/next_text", std::string("Standup with the hardware team in 19 min"));
    app.state.set("weather/now_text", std::string("4\xC2\xB0" "C Cloudy, rain later"));
    run_ms(app, 50);
    EXPECT(!has_chip(app, "printer"), "portrait: printer dropped first");
    selftest_orientation(app, "portrait-narrow");
    shot("07-portrait-chips-dropped");
    sim::publish_placeholders(app.state);
    run_ms(app, 50);

    for (const char *id : {"calendar", "weather", "news", "printer"}) {
        app.shell->go_to_panel(id, false);
        run_ms(app, 100);
        check_panel_inside_content(app, id);
        shot(std::string("08-portrait-") + id);
    }
    {
        // Recipes in portrait: library, then card mode (ingredients above, step below).
        recipes::RecipesPanel &rp = *app.recipes_panel;
        app.shell->go_to_panel("recipes", false);
        run_ms(app, 100);
        check_panel_inside_content(app, "recipes library portrait");
        EXPECT(rp.thumbnail_ids().size() == 1, "thumbnail in portrait too");
        shot("24-portrait-recipes-library");
        rp.open_recipe("tomato-soup");
        run_ms(app, 100);
        EXPECT(rp.view() == recipes::RecipesPanel::View::Card, "card mode in portrait");
        check_panel_inside_content(app, "recipe card portrait");
        shot("25-portrait-recipe-card");
        rp.open_recipe("grandmas-stew");
        run_ms(app, 100);
        lv_obj_t *area = lv_obj_get_parent(rp.page_image());
        EXPECT(lv_obj_get_height(rp.page_image()) <= lv_obj_get_content_height(area) &&
                   lv_obj_get_width(rp.page_image()) <= lv_obj_get_content_width(area),
               "portrait: the whole page is visible");
        check_panel_inside_content(app, "book mode portrait");
        shot("29-portrait-recipe-book");
        rp.back_to_library();
        run_ms(app, 100);
        app.shell->go_to_panel("home_clock", false);
        run_ms(app, 100);
    }
    app.shell->updates().open();
    run_ms(app, 100);
    shot("14-portrait-updates");
    app.shell->updates().close();

    // --- Delete recipes (D-010): separate button, confirm names the count ---
    {
        auto &ms = *app.shell->modules_screen();
        auto click = [&](lv_obj_t *o) {
            run_ms(app, 40);
            lv_obj_update_layout(o);
            lv_obj_scroll_to_view_recursive(o, LV_ANIM_OFF);
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            tap(app, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
            run_ms(app, 60);
        };
        for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);
        const int n = app.recipe_store->count();
        const std::string ns = std::to_string(n);
        app.recipes_panel->open_recipe("pancakes");
        app.bus.publish(shell::topics::kOpenScreen, std::string("settings/modules"));
        run_ms(app, 100);
        EXPECT(n > 1 && ms.delete_text("recipes") == "Delete " + ns + " recipes" && ms.action_text("recipes") == "Remove",
               "Delete is its own button next to Remove (%s)", ms.delete_text("recipes").c_str());
        // Remove keeps the recipes and the Delete button.
        click(ms.action_button("recipes"));
        EXPECT(ms.action_text("recipes") == "Install" && app.recipe_store->count() == n &&
                   ms.delete_text("recipes") == "Delete " + ns + " recipes",
               "Remove deletes nothing; Delete is still offered");
        click(ms.delete_button("recipes"));
        EXPECT(ms.confirm_shown() &&
                   ms.confirm_text() == "Delete " + ns + " recipes from the SD card? This can't be undone.",
               "confirm names the count (%s)", ms.confirm_text().c_str());
        EXPECT(app.recipe_store->count() == n, "nothing deleted before confirming");
        shot("30-portrait-delete-recipes-confirm");
        click(ms.confirm_cancel());
        EXPECT(!ms.confirm_shown() && app.recipe_store->count() == n, "Cancel deletes nothing");
        click(ms.delete_button("recipes"));
        click(ms.confirm_yes());
        EXPECT(!ms.confirm_shown() && app.recipe_store->count() == 0 && ms.delete_text("recipes").empty(),
               "Delete removes every recipe and the button goes away");
        EXPECT(!app.sd->exists("/recipes/pancakes"), "recipe folders are gone");
        shot("31-portrait-modules-after-delete");
        // Back in the Recipe reader: the recipe that was open is closed, the library is empty.
        click(ms.action_button("recipes"));
        ms.close();
        app.shell->go_to_panel("recipes", false);
        run_ms(app, 100);
        EXPECT(app.recipes_panel->view() == recipes::RecipesPanel::View::Library && app.recipes_panel->visible_ids().empty(),
               "an open recipe is closed; the library is empty");
        // Deleted while the recipe is on screen: same result.
        sim::seed_recipes(*app.recipe_store);
        app.recipes_panel->open_recipe("pancakes");
        run_ms(app, 100);
        EXPECT(app.recipes_panel->view() == recipes::RecipesPanel::View::Card, "recipe open again");
        int gone = 0;
        EXPECT(app.recipe_data->delete_all(&gone) && gone == 2, "deleted 2 (%d)", gone);
        run_ms(app, 100);
        EXPECT(app.recipes_panel->view() == recipes::RecipesPanel::View::Library && app.recipes_panel->visible_ids().empty(),
               "recipe on screen is closed when deleted");
        app.shell->go_to_panel("home_clock", false);
        run_ms(app, 100);
        for (int i = 0; i < 100 && app.shell->toast_showing(); ++i) run_ms(app, 200);
    }

    // System chip icons are LVGL's symbols.
    auto sys = shell::system_chips();
    EXPECT(sys[1].icon == LV_SYMBOL_WIFI && sys[2].icon == LV_SYMBOL_DOWNLOAD && sys[3].icon == LV_SYMBOL_LOOP &&
               sys[4].icon == LV_SYMBOL_BATTERY_FULL,
           "system chip icons match LV_SYMBOL_*");
}

int start_app(App &app, shell::Shell::ApplyOrientation apply)
{
    sim::register_demo_panels(app.registry);
    {
        // The "SD card": a fresh temp folder seeded with sample recipes.
        char tmpl[] = "/tmp/summit-sim-sd-XXXXXX";
        app.sd_dir = mkdtemp(tmpl);
        App *a = &app;
        app.sd = std::make_unique<PosixStorage>(app.sd_dir, [a] { return a->sd_free; });
        // Sample card (tests/fixtures/sd: recipes with page images) plus two seeded recipes.
        std::error_code ec;
        std::filesystem::copy(SUMMIT_FIXTURE_SD, app.sd_dir, std::filesystem::copy_options::recursive, ec);
        app.recipe_store = std::make_unique<recipes::RecipeStore>(*app.sd);
        sim::seed_recipes(*app.recipe_store);
        PosixStorage *sd = app.sd.get();
        auto panel = std::make_unique<recipes::RecipesPanel>(*app.recipe_store, [sd](const std::string &card_path) {
            const std::string host = sd->host_path(card_path);
            return host.empty() || !sd->exists(card_path) ? std::string() : "S:" + host;
        });
        app.recipes_panel = panel.get();
        app.registry.add(std::move(panel));  // last: a module's page is appended
    }
    SettingsSchema schema = system_settings_schema();
    sim::register_demo_modules(app.modules);
    app.recipe_data = std::make_unique<recipes::RecipeModuleData>(*app.recipe_store, app.state);
    app.modules.set_data("recipes", app.recipe_data.get());
    for (const auto &d : app.registry.setting_defs()) schema.defs.push_back(d);
    for (const auto &d : app.modules.setting_defs()) schema.defs.push_back(d);
    app.settings = std::make_unique<Settings>(schema, app.storage, &app.bus);
    app.settings->load();
    app.host = std::make_unique<PanelHost>(app.registry, app.bus, app.state, *app.settings);
    app.timers = std::make_unique<TimerService>(app.bus, app.state);
    app.updates = std::make_unique<sim::FakeUpdateService>(app.bus, app.state, *app.settings, "0.0.1");
    sim::publish_placeholders(app.state);
    publish_storage_state(*app.sd, app.state);
    app.shell = std::make_unique<shell::Shell>(
        shell::Shell::Deps{app.bus, app.state, *app.settings, app.registry, *app.host, &app.modules}, std::move(apply));
    std::string err;
    if (!app.shell->start(g_ms, &err)) {
        std::printf("shell start failed: %s\n", err.c_str());
        return 1;
    }
    return 0;
}

#if SUMMIT_SIM_SDL
// Interactive window. Mouse = touch. Keys: R rotate, T toast, F full-screen, Q quit.
int run_sdl()
{
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(1280, 720);
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_tick_set_cb([] { return static_cast<uint32_t>(SDL_GetTicks()); });
    App app;
    if (start_app(app, [disp](Orientation o) {
            if (o == kLandscape) lv_display_set_resolution(disp, 1280, 720);
            else lv_display_set_resolution(disp, 720, 1280);
        }) != 0)
        return 1;
    app.shell->attach_input(mouse);
    std::printf("R rotate, T toast, F full-screen, Q quit. Swipe pages, swipe down / long-press the strip.\n");
    bool portrait = false, fullscreen = false;
    bool prev[4] = {false, false, false, false};
    for (;;) {
        app.bus.dispatch();
        app.shell->tick(lv_tick_get());
        uint32_t wait = lv_timer_handler();
        const uint8_t *keys = SDL_GetKeyboardState(nullptr);
        const bool now[4] = {keys[SDL_SCANCODE_R] != 0, keys[SDL_SCANCODE_T] != 0, keys[SDL_SCANCODE_F] != 0,
                             keys[SDL_SCANCODE_Q] != 0};
        if (now[0] && !prev[0]) {
            portrait = !portrait;
            app.state.set("sensor/orientation", std::string(portrait ? "portrait" : "landscape"));
        }
        if (now[1] && !prev[1]) app.bus.publish(shell::topics::kToast, std::string("Print finished: Enclosure v2"));
        if (now[2] && !prev[2]) app.state.set("ui/fullscreen", fullscreen = !fullscreen);
        if (now[3]) break;
        std::memcpy(prev, now, sizeof prev);
        SDL_Delay(wait < 5 ? wait : 5);
    }
    return 0;
}
#endif

}  // namespace

namespace {

// --web: the device's web page + WebApi on http://127.0.0.1:PORT, driven by the same
// framework, settings, shell and fake update service (real UpdateEngine) as the self-test.
struct SimControl : web::UpdateControl {
    sim::FakeUpdateService *svc;
    bool upload_begin(int64_t n, std::string *e) override { return svc->upload_begin(n, e); }
    bool upload_data(const uint8_t *d, size_t n) override { return svc->upload_data(d, n); }
    bool upload_complete(std::string *e) override { return svc->upload_complete(e); }
    void upload_abort(const char *r) override { svc->upload_abort(r); }
};

struct SimWifi : web::WifiStore {
    std::string s = "home", pw = "home-network-password";
    std::string ssid() override { return s; }
    std::string password() override { return pw; }
    bool save(const std::string &a, const std::string &b, std::string *) override
    {
        s = a;
        pw = b;
        return true;
    }
};

// Stands in for the internet: https://recipes.example.com/<name> serves
// tests/fixtures/web/<name>.html; .../photo.jpg serves a sample JPEG.
struct SimFetcher : recipes::PageFetcher {
    bool get(const std::string &url, size_t max_bytes, std::string &body, std::string *error) override
    {
        const std::string prefix = "https://recipes.example.com/";
        std::string file;
        if (url.compare(0, prefix.size(), prefix) == 0 && url.find("..") == std::string::npos)
            file = std::string(SUMMIT_FIXTURE_SD) + "/../web/" + url.substr(prefix.size()) + ".html";
        if (url == "https://cdn.example-recipes.com/banana-bread-1x1.jpg")
            file = std::string(SUMMIT_FIXTURE_SD) + "/recipes/apple-pie/photo.jpg";
        std::ifstream f(file, std::ios::binary);
        if (file.empty() || !f) {
            if (error) *error = "HTTP 404";
            return false;
        }
        body.assign(std::istreambuf_iterator<char>(f), {});
        return body.size() <= max_bytes;
    }
};

struct SimFacts : web::DeviceFacts {
    std::vector<std::pair<std::string, std::string>> facts() override
    {
        return {{"uptime_s", std::to_string(g_ms / 1000)}, {"running_partition", "ota_0"}, {"simulator", "yes"}};
    }
};

int run_web(int port)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    g_hw.set_size(1280, 720);
    App app;
    if (start_app(app, [](Orientation o) {
            if (o == kLandscape) g_hw.set_size(1280, 720);
            else g_hw.set_size(720, 1280);
        }) != 0)
        return 1;
    app.state.set("net/ssid", std::string("home"));
    app.state.set("net/ip", std::string("192.168.1.42"));
    app.state.set("net/scan", std::string(R"([{"ssid":"home","rssi":-48,"secure":true},)"
                                          R"({"ssid":"cafe","rssi":-71,"secure":false}])"));
    app.state.set("diag/warnings", std::string("LIS2DW12 accelerometer not found (add-on, does not affect updates)"));
    SimControl control;
    control.svc = app.updates.get();
    SimWifi wifi;
    SimFacts facts;
    web::WebApi api(app.bus, app.state, *app.settings, control, wifi, facts);
    api.set_modules(&app.modules);
    {
        // A PDF "copied onto the SD card from a computer", waiting in the inbox.
        std::error_code ec;
        std::filesystem::create_directories(app.sd_dir + "/recipes/inbox", ec);
        std::filesystem::copy_file(std::string(SUMMIT_FIXTURE_SD) + "/../pdf/lemon-cake.pdf",
                                   app.sd_dir + "/recipes/inbox/from-sd-card.pdf", ec);
    }
    recipes::RecipeWebApi recipe_api(*app.recipe_store, *app.sd);
    SimFetcher fetcher;
    recipe_api.set_fetcher(&fetcher);
    api.add_extension(&recipe_api);
    sim::HttpSim http(api, SUMMIT_WEB_ROOT, app.sd.get());
    if (!http.listen(port)) {
        std::printf("cannot listen on 127.0.0.1:%d\n", port);
        return 1;
    }
    std::printf("web UI: http://127.0.0.1:%d/\n", port);
    std::fflush(stdout);
    const auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        http.poll();
        g_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
        app.bus.dispatch();
        app.updates->tick(g_ms);
        app.timers->tick(g_ms);
        app.shell->tick(g_ms);
        lv_timer_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

}  // namespace

int main(int argc, char **argv)
{
    bool selftest_mode = false, sdl = false;
    int web_port = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--selftest")) selftest_mode = true;
        else if (!std::strcmp(argv[i], "--sdl")) sdl = true;
        else if (!std::strcmp(argv[i], "--web") && i + 1 < argc) web_port = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) g_out = argv[++i];
    }
    if (web_port) return run_web(web_port);
    if (sdl) {
#if SUMMIT_SIM_SDL
        return run_sdl();
#else
        std::printf("built without SDL2; install libsdl2-dev and rebuild\n");
        return 2;
#endif
    }
    if (!selftest_mode) {
        std::printf("usage: summit_sim --selftest [--out DIR] | --sdl | --web PORT\n");
        return 2;
    }
    mkdir(g_out.c_str(), 0755);
    lv_init();
    lv_tick_set_cb(tick_cb);
    g_hw.set_size(1280, 720);
    g_hw.pointer = lv_indev_create();
    lv_indev_set_type(g_hw.pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_hw.pointer, [](lv_indev_t *, lv_indev_data_t *d) {
        d->point = g_hw.point;
        d->state = g_hw.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    });

    App app;
    if (start_app(app, [](Orientation o) {
            // Device: lv_display_set_rotation on the native 720x1280 panel.
            if (o == kLandscape) g_hw.set_size(1280, 720);
            else g_hw.set_size(720, 1280);
        }) != 0)
        return 1;
    app.shell->attach_input(g_hw.pointer);
    run_ms(app, 500);
    selftest(app);
    std::printf("selftest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
