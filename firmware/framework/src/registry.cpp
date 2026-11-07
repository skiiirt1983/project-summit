#include "summit/registry.h"

#include <set>
#include <utility>

namespace summit {

std::string panel_setting_key(const std::string &panel_id, const std::string &key)
{
    return "panels/" + panel_id + "/" + key;
}

bool validate_manifest(const PanelManifest &m, std::string *error)
{
    auto fail = [error, &m](const std::string &msg) {
        if (error) *error = "panel '" + m.id + "': " + msg;
        return false;
    };
    if (!valid_topic(m.id) || m.id.find('/') != std::string::npos) return fail("invalid id");
    if (m.name.empty()) return fail("missing name");
    if (m.version.empty()) return fail("missing version");
    if (m.sizes == 0 || (m.sizes & ~(kSizeFull | kSizeHalf | kSizeTile)) != 0)
        return fail("sizes must be a non-empty set of full/half/tile");
    if (m.orientations != kBothOrientations)
        return fail("must support landscape and portrait");
    std::set<std::string> topics;
    for (const std::string &t : m.subscriptions) {
        if (!valid_topic(t)) return fail("invalid subscription: " + t);
        if (!topics.insert(t).second) return fail("duplicate subscription: " + t);
    }
    SettingsSchema schema;
    for (const SettingDef &d : m.settings) {
        SettingDef full = d;
        full.key = panel_setting_key(m.id, d.key);
        schema.defs.push_back(full);
    }
    std::string err;
    if (!validate_schema(schema, &err)) return fail("settings: " + err);
    std::set<std::string> chips;
    for (const GlanceChipDef &c : m.glances) {
        if (!valid_topic(c.id) || c.id.find('/') != std::string::npos) return fail("invalid chip id: " + c.id);
        if (!chips.insert(c.id).second) return fail("duplicate chip: " + c.id);
        if (!valid_topic(c.topic)) return fail("invalid chip topic: " + c.topic);
        if (!c.icon_topic.empty() && !valid_topic(c.icon_topic)) return fail("invalid chip icon topic");
    }
    return true;
}

bool PanelRegistry::add(std::unique_ptr<Panel> panel, std::string *error)
{
    if (!panel) {
        if (error) *error = "null panel";
        return false;
    }
    const PanelManifest &m = panel->manifest();
    if (!validate_manifest(m, error)) return false;
    if (find(m.id)) {
        if (error) *error = "duplicate panel id: " + m.id;
        return false;
    }
    for (const GlanceChipDef &c : m.glances) {
        for (const GlanceChipDef &other : glance_chips()) {
            if (other.id == c.id) {
                if (error) *error = "duplicate glance chip id: " + c.id;
                return false;
            }
        }
    }
    panels_.push_back(std::move(panel));
    return true;
}

Panel *PanelRegistry::find(const std::string &id) const
{
    for (const auto &p : panels_) {
        if (p->manifest().id == id) return p.get();
    }
    return nullptr;
}

std::vector<std::string> PanelRegistry::ids() const
{
    std::vector<std::string> out;
    for (const auto &p : panels_) out.push_back(p->manifest().id);
    return out;
}

std::vector<SettingDef> PanelRegistry::setting_defs() const
{
    std::vector<SettingDef> out;
    for (const auto &p : panels_) {
        for (const SettingDef &d : p->manifest().settings) {
            SettingDef full = d;
            full.key = panel_setting_key(p->manifest().id, d.key);
            out.push_back(full);
        }
    }
    return out;
}

std::vector<GlanceChipDef> PanelRegistry::glance_chips() const
{
    std::vector<GlanceChipDef> out;
    for (const auto &p : panels_) {
        for (GlanceChipDef c : p->manifest().glances) {
            c.panel_id = p->manifest().id;
            out.push_back(c);
        }
    }
    return out;
}

class PanelHost::Context : public PanelContext {
public:
    Context(std::string id, StateStore &state, Settings &settings)
        : id_(std::move(id)), state_(state), settings_(settings) {}
    Value state(const std::string &key) const override { return state_.get(key); }
    Value setting(const std::string &key) const override
    {
        return settings_.get(panel_setting_key(id_, key));
    }
    SetResult set_setting(const std::string &key, Value value) override
    {
        return settings_.set(panel_setting_key(id_, key), std::move(value));
    }

private:
    std::string id_;
    StateStore &state_;
    Settings &settings_;
};

PanelHost::PanelHost(PanelRegistry &registry, EventBus &bus, StateStore &state, Settings &settings)
    : registry_(registry), bus_(bus), state_(state), settings_(settings) {}

PanelHost::~PanelHost()
{
    for (auto &kv : slots_) {
        for (auto id : kv.second.subs) bus_.unsubscribe(id);
    }
}

PanelHost::Slot *PanelHost::slot(const std::string &id)
{
    auto it = slots_.find(id);
    if (it != slots_.end()) return &it->second;
    Panel *p = registry_.find(id);
    if (!p) return nullptr;
    Slot &s = slots_[id];
    s.panel = p;
    s.ctx = std::make_unique<Context>(id, state_, settings_);
    return &s;
}

bool PanelHost::create(const std::string &id)
{
    Slot *s = slot(id);
    if (!s || s->state != PanelState::Registered) return false;
    if (!s->panel->create(*s->ctx)) return false;
    s->state = PanelState::Created;
    return true;
}

bool PanelHost::show(const std::string &id, const PanelView &view)
{
    Slot *s = slot(id);
    if (!s || s->state != PanelState::Created) return false;
    if ((s->panel->manifest().sizes & view.size) == 0) return false;
    s->panel->show(*s->ctx, view);
    s->state = PanelState::Shown;
    Panel *panel = s->panel;
    Context *ctx = s->ctx.get();
    for (const std::string &topic : panel->manifest().subscriptions) {
        s->subs.push_back(bus_.subscribe(topic, [panel, ctx](const std::string &t, const Value &v) {
            panel->on_event(*ctx, t, v);
        }));
    }
    return true;
}

bool PanelHost::hide(const std::string &id)
{
    Slot *s = slot(id);
    if (!s || s->state != PanelState::Shown) return false;
    for (auto sub : s->subs) bus_.unsubscribe(sub);
    s->subs.clear();
    s->panel->hide(*s->ctx);
    s->state = PanelState::Created;
    return true;
}

bool PanelHost::destroy(const std::string &id)
{
    Slot *s = slot(id);
    if (!s || s->state != PanelState::Created) return false;
    s->panel->destroy(*s->ctx);
    s->state = PanelState::Registered;
    return true;
}

void PanelHost::tick(uint32_t now_ms)
{
    for (auto &kv : slots_) {
        if (kv.second.state == PanelState::Shown) kv.second.panel->tick(*kv.second.ctx, now_ms);
    }
}

PanelState PanelHost::state(const std::string &id) const
{
    auto it = slots_.find(id);
    return it == slots_.end() ? PanelState::Registered : it->second.state;
}

}  // namespace summit
