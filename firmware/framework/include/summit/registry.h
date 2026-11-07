// Panel registry (compile-time panels registered at boot) and the host that
// drives each panel's lifecycle and event subscriptions.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "summit/event_bus.h"
#include "summit/panel_api.h"
#include "summit/settings.h"
#include "summit/state_store.h"

namespace summit {

class PanelRegistry {
public:
    // Rejects an invalid manifest or duplicate id.
    bool add(std::unique_ptr<Panel> panel, std::string *error = nullptr);
    Panel *find(const std::string &id) const;
    std::vector<std::string> ids() const;  // registration order

    // Panel settings, namespaced "panels/<id>/<key>", to append to the
    // settings schema before Settings::load().
    std::vector<SettingDef> setting_defs() const;

private:
    std::vector<std::unique_ptr<Panel>> panels_;
};

enum class PanelState { Registered, Created, Shown };

class PanelHost {
public:
    PanelHost(PanelRegistry &registry, EventBus &bus, StateStore &state, Settings &settings);
    ~PanelHost();
    PanelHost(const PanelHost &) = delete;
    PanelHost &operator=(const PanelHost &) = delete;

    // Each returns false (and does nothing) for an unknown id or a call out of
    // lifecycle order.
    bool create(const std::string &id);
    bool show(const std::string &id, const PanelView &view);
    bool hide(const std::string &id);
    bool destroy(const std::string &id);

    // Ticks every shown panel. UI task only.
    void tick(uint32_t now_ms);

    PanelState state(const std::string &id) const;

private:
    class Context;
    struct Slot {
        Panel *panel = nullptr;
        PanelState state = PanelState::Registered;
        std::unique_ptr<Context> ctx;
        std::vector<EventBus::SubscriptionId> subs;
    };
    Slot *slot(const std::string &id);

    PanelRegistry &registry_;
    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    std::map<std::string, Slot> slots_;
};

}  // namespace summit
