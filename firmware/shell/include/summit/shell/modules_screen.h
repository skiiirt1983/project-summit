// Settings -> Modules (ARCHITECTURE.md "Modules"): each module with its description,
// requirements and an Install / Remove button. Install checks requirements against
// storage/sd_present and storage/sd_free_mb and explains what is missing.
#pragma once
#include <string>
#include <vector>

#include "lvgl.h"
#include "summit/event_bus.h"
#include "summit/modules.h"
#include "summit/settings.h"
#include "summit/shell/layout.h"
#include "summit/state_store.h"

namespace summit::shell {

class ModulesScreen {
public:
    ModulesScreen(EventBus &bus, StateStore &state, Settings &settings, const ModuleManager &modules)
        : bus_(bus), state_(state), settings_(settings), modules_(modules) {}

    void build(lv_obj_t *parent);
    void apply_layout(const Layout &layout);
    void open();
    void close();
    bool is_open() const { return open_; }
    void tick();

    ModuleEnv env() const;
    // Introspection for the simulator self-test.
    lv_obj_t *action_button(const std::string &id) const;
    std::string action_text(const std::string &id) const;
    std::string status_text(const std::string &id) const;
    lv_obj_t *delete_button(const std::string &id) const;
    std::string delete_text(const std::string &id) const;
    bool confirm_shown() const;
    std::string confirm_text() const;
    lv_obj_t *confirm_yes() const { return yes_btn_; }
    lv_obj_t *confirm_cancel() const { return cancel_btn_; }

private:
    struct Row {
        std::string id;
        lv_obj_t *card = nullptr, *name = nullptr, *desc = nullptr, *status = nullptr, *button = nullptr,
                 *button_label = nullptr, *del = nullptr, *del_label = nullptr;
        std::string message;  // result of the last Install attempt
    };
    const Row *row(const std::string &id) const;
    static void event_cb(lv_event_t *e);

    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    const ModuleManager &modules_;
    bool open_ = false;
    lv_obj_t *root_ = nullptr, *title_ = nullptr, *close_btn_ = nullptr, *list_ = nullptr;
    std::vector<Row> rows_;
    lv_obj_t *confirm_ = nullptr, *confirm_card_ = nullptr, *confirm_label_ = nullptr, *confirm_buttons_ = nullptr,
             *yes_btn_ = nullptr, *cancel_btn_ = nullptr;
    std::string confirm_id_;  // module whose data is about to be deleted
};

}  // namespace summit::shell
