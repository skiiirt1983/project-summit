// Settings -> Updates (ARCHITECTURE.md "Summit additions" 1). A view over the
// update engine's state topics; it never touches the engine directly, it sends
// "update/command" events (check, install:<v>, previous, reboot) like the web UI.
#pragma once
#include <string>

#include "lvgl.h"
#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/shell/layout.h"
#include "summit/state_store.h"

namespace summit::shell {

class UpdatesScreen {
public:
    UpdatesScreen(EventBus &bus, StateStore &state, Settings &settings) : bus_(bus), state_(state), settings_(settings) {}

    void build(lv_obj_t *parent);
    void apply_layout(const Layout &layout);  // fills the content area (full screen while installing)
    void open();
    void close();
    bool is_open() const { return open_; }
    void tick();  // refresh from state

    // Introspection for the simulator self-test.
    lv_obj_t *root() const { return root_; }
    lv_obj_t *check_button() const { return check_btn_; }
    lv_obj_t *install_button() const { return install_btn_; }
    lv_obj_t *restart_button() const { return restart_btn_; }
    lv_obj_t *previous_button() const { return previous_btn_; }
    lv_obj_t *channel_buttons() const { return channel_; }
    bool showing_progress() const;
    bool confirm_shown() const;
    lv_obj_t *confirm_yes() const { return yes_btn_; }
    lv_obj_t *confirm_cancel() const { return cancel_btn_; }
    std::string confirm_text() const;
    std::string status_text() const;

private:
    std::string str(const char *key) const;
    static void event_cb(lv_event_t *e);

    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    bool open_ = false;
    Layout layout_{};
    lv_obj_t *root_ = nullptr, *title_ = nullptr, *close_btn_ = nullptr;
    lv_obj_t *info_ = nullptr, *installed_ = nullptr, *hw_ = nullptr;
    lv_obj_t *channel_row_ = nullptr, *channel_ = nullptr;
    lv_obj_t *status_ = nullptr;
    lv_obj_t *available_ = nullptr, *available_title_ = nullptr, *notes_ = nullptr;
    lv_obj_t *buttons_ = nullptr, *check_btn_ = nullptr, *install_btn_ = nullptr, *restart_btn_ = nullptr,
             *previous_btn_ = nullptr;
    lv_obj_t *confirm_ = nullptr, *confirm_label_ = nullptr, *yes_btn_ = nullptr, *cancel_btn_ = nullptr;
    lv_obj_t *confirm_card_ = nullptr, *confirm_buttons_ = nullptr;
    lv_obj_t *progress_ = nullptr, *progress_title_ = nullptr, *bar_ = nullptr, *pct_ = nullptr, *keep_ = nullptr;
};

}  // namespace summit::shell
