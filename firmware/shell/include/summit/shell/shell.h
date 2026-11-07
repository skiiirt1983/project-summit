// The shell: glance strip, pager, quick-settings sheet, toasts, rotation.
// LVGL front end over the pure logic in layout/glance/strip_state/rotation/pager.
// UI task only (like all LVGL calls).
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lvgl.h"
#include "summit/event_bus.h"
#include "summit/registry.h"
#include "summit/settings.h"
#include "summit/shell/glance.h"
#include "summit/shell/layout.h"
#include "summit/shell/pager.h"
#include "summit/shell/rotation.h"
#include "summit/shell/strip_state.h"
#include "summit/state_store.h"

namespace summit::shell {

// Nearest built-in font for a token text size (only fonts enabled in lv_conf / Kconfig).
const lv_font_t *font_for(int16_t px);

// State keys the shell reads (published by services / panels).
namespace topics {
constexpr const char *kOrientation = "sensor/orientation";  // "landscape" | "portrait"
constexpr const char *kToast = "notify/toast";              // event: toast text
constexpr const char *kToastCritical = "notify/toast_critical";  // event: shown at once, even full-screen
constexpr const char *kOpenScreen = "ui/open_screen";       // event (shell -> screens): e.g. "settings/updates"
constexpr const char *kFullscreen = "ui/fullscreen";        // bool: LyricFlash visuals
constexpr const char *kAlarmRinging = "alarm/ringing";      // bool
constexpr const char *kAmbient = "ui/ambient";              // bool: night / ambient clock
constexpr const char *kUpdating = "update/in_progress";     // bool
constexpr const char *kSettingsNotice = "system/settings_notice";  // string, empty = none
constexpr const char *kRotationLocked = "shell/rotation_locked";  // written by the shell
constexpr const char *kCurrentPanel = "shell/current_panel";      // written by the shell
}  // namespace topics

class Shell {
public:
    struct Deps {
        EventBus &bus;
        StateStore &state;
        Settings &settings;
        PanelRegistry &registry;
        PanelHost &host;
    };
    // Rotates the physical display (device: lv_display_set_rotation; simulator:
    // new resolution). The shell re-lays out from the display's new size.
    using ApplyOrientation = std::function<void(Orientation)>;

    Shell(Deps deps, ApplyOrientation apply);
    ~Shell();
    Shell(const Shell &) = delete;
    Shell &operator=(const Shell &) = delete;

    // Builds the UI on the default display and shows the first panel.
    bool start(uint32_t now_ms, std::string *error);
    // Call every UI loop iteration, after bus.dispatch() and before lv_timer_handler().
    void tick(uint32_t now_ms);
    // Drag-to-swipe between pages and tap-to-reveal the hidden strip.
    void attach_input(lv_indev_t *indev);

    // Actions (used by gestures, and by the simulator's scripted runs).
    bool go_to_panel(const std::string &id, bool animate = true);
    bool next_page();
    bool prev_page();
    void open_quick_settings();
    void close_quick_settings();
    void toggle_rotation_lock();
    void tap_reveal();
    // Settings problem (newer version / corrupt): first call arms, second within 3 s resets.
    // Settings::reset() saves the old settings as a backup for export first.
    void request_settings_reset();
    void on_chip_tapped(size_t chip_index);

    // Introspection for the simulator self-test.
    const Layout &layout() const { return layout_; }
    lv_obj_t *strip() const { return strip_; }
    lv_obj_t *content() const { return content_; }
    lv_obj_t *page() const { return page_; }
    lv_obj_t *quick_settings() const { return sheet_; }
    bool quick_settings_open() const { return sheet_open_; }
    bool strip_visible() const;
    bool toast_showing() const { return toast_on_; }
    bool settings_notice_shown() const;
    lv_obj_t *reset_button() const { return reset_btn_; }
    std::vector<std::string> visible_chips() const;
    const Pager &pager() const { return *pager_; }
    const RotationController &rotation() const { return rotation_; }

private:
    struct ChipUi {
        GlanceChipDef def;
        lv_obj_t *obj = nullptr;
        lv_obj_t *icon = nullptr;
        lv_obj_t *text = nullptr;
        bool has_data = false;
        bool fits = true;
        Value last_value;
        Value last_icon;
    };

    void build_strip();
    void build_quick_settings();
    void apply_layout(bool morph);
    void style_strip_fonts();
    void refresh_chips(bool force_fit);
    void fit_chip_row();
    void refresh_strip_visibility();
    void show_current(int slide_dir);
    void sync_quick_settings();
    void refresh_settings_notice();
    void update_hide_reasons();
    void on_toast(const Value &v, bool critical);
    void rebuild_pages();
    void on_input(lv_event_t *e);

    static void chip_clicked_cb(lv_event_t *e);
    static void strip_gesture_cb(lv_event_t *e);
    static void strip_long_press_cb(lv_event_t *e);
    static void sheet_event_cb(lv_event_t *e);
    static void input_cb(lv_event_t *e);

    Deps d_;
    ApplyOrientation apply_;
    uint32_t now_ = 0;
    Layout layout_{};
    std::unique_ptr<Pager> pager_;
    RotationController rotation_;
    StripVisibility visibility_;
    ToastQueue toasts_;
    bool toast_on_ = false;        // copied from the queue each tick (no pointers into it)
    bool toast_critical_ = false;
    std::string toast_text_;
    EventBus::SubscriptionId toast_sub_ = 0;
    EventBus::SubscriptionId critical_sub_ = 0;
    EventBus::SubscriptionId order_sub_ = 0;

    lv_obj_t *screen_ = nullptr;
    lv_obj_t *content_ = nullptr;
    lv_obj_t *page_ = nullptr;
    lv_obj_t *strip_ = nullptr;
    lv_obj_t *row_ = nullptr;
    lv_obj_t *spacer_ = nullptr;
    lv_obj_t *toast_label_ = nullptr;
    lv_obj_t *backdrop_ = nullptr;
    lv_obj_t *sheet_ = nullptr;
    lv_obj_t *brightness_ = nullptr;
    lv_obj_t *volume_ = nullptr;
    lv_obj_t *rotation_buttons_ = nullptr;
    lv_obj_t *dnd_ = nullptr;
    lv_obj_t *notice_row_ = nullptr;
    lv_obj_t *notice_label_ = nullptr;
    lv_obj_t *reset_btn_ = nullptr;
    lv_obj_t *reset_label_ = nullptr;
    std::string notice_text_;
    bool reset_armed_ = false;
    uint32_t reset_armed_until_ = 0;
    bool sheet_open_ = false;
    std::vector<ChipUi> chips_;

    // Drag-to-swipe state
    lv_indev_t *indev_ = nullptr;
    bool dragging_ = false;
    bool drag_horizontal_ = false;
    lv_point_t press_{0, 0};
    lv_point_t last_{0, 0};
    uint32_t last_ms_ = 0;
    float velocity_ = 0.0f;
};

}  // namespace summit::shell
