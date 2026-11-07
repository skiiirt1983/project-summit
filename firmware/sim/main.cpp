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
#include "png.h"
#include "summit/event_bus.h"
#include "summit/registry.h"
#include "summit/settings.h"
#include "summit/shell/shell.h"
#include "summit/state_store.h"
#include "summit/system_settings.h"

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
    std::unique_ptr<Settings> settings;
    std::unique_ptr<PanelHost> host;
    std::unique_ptr<shell::Shell> shell;
};

void run_ms(App &app, uint32_t ms)
{
    const uint32_t end = g_ms + ms;
    while (g_ms < end) {
        g_ms += 4;
        app.bus.dispatch();
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
    EXPECT(opened == "settings/updates" && !app.shell->quick_settings_open(), "tap update -> Updates (%s)",
           opened.c_str());
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

    // System chip icons are LVGL's symbols.
    auto sys = shell::system_chips();
    EXPECT(sys[1].icon == LV_SYMBOL_WIFI && sys[2].icon == LV_SYMBOL_DOWNLOAD && sys[3].icon == LV_SYMBOL_LOOP &&
               sys[4].icon == LV_SYMBOL_BATTERY_FULL,
           "system chip icons match LV_SYMBOL_*");
}

int start_app(App &app, shell::Shell::ApplyOrientation apply)
{
    sim::register_demo_panels(app.registry);
    SettingsSchema schema = system_settings_schema();
    for (const auto &d : app.registry.setting_defs()) schema.defs.push_back(d);
    app.settings = std::make_unique<Settings>(schema, app.storage, &app.bus);
    app.settings->load();
    app.host = std::make_unique<PanelHost>(app.registry, app.bus, app.state, *app.settings);
    sim::publish_placeholders(app.state);
    app.shell = std::make_unique<shell::Shell>(
        shell::Shell::Deps{app.bus, app.state, *app.settings, app.registry, *app.host}, std::move(apply));
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

int main(int argc, char **argv)
{
    bool selftest_mode = false, sdl = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--selftest")) selftest_mode = true;
        else if (!std::strcmp(argv[i], "--sdl")) sdl = true;
        else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) g_out = argv[++i];
    }
    if (sdl) {
#if SUMMIT_SIM_SDL
        return run_sdl();
#else
        std::printf("built without SDL2; install libsdl2-dev and rebuild\n");
        return 2;
#endif
    }
    if (!selftest_mode) {
        std::printf("usage: summit_sim --selftest [--out DIR] | --sdl\n");
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
