#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "fake_storage.h"
#include "summit/registry.h"

using namespace summit;

struct FakePanel : Panel {
    PanelManifest m;
    std::vector<std::string> *log;
    bool lv_objects = false;  // stands in for "holds LVGL objects"
    bool create_ok = true;

    FakePanel(std::string id, std::vector<std::string> *l) : log(l)
    {
        m.id = id;
        m.name = "Fake";
        m.icon = "x";
        m.version = "1";
        m.sizes = kSizeFull | kSizeHalf;
        m.subscriptions = {"weather/now"};
        m.settings = {{"units", std::string("metric"), {"metric", "imperial"}}};
    }
    const PanelManifest &manifest() const override { return m; }
    bool create(PanelContext &) override { log->push_back("create"); return create_ok; }
    void show(PanelContext &, const PanelView &) override { lv_objects = true; log->push_back("show"); }
    void tick(PanelContext &, uint32_t) override { log->push_back("tick"); }
    void on_event(PanelContext &ctx, const std::string &t, const Value &) override
    {
        log->push_back("event:" + t + ":" + std::get<std::string>(ctx.setting("units")));
    }
    void hide(PanelContext &) override { lv_objects = false; log->push_back("hide"); }
    void destroy(PanelContext &) override { log->push_back("destroy"); }
};

struct World {
    std::vector<std::string> log;
    EventBus bus;
    StateStore state{bus};
    PanelRegistry reg;
    FakeStorage storage;
    FakePanel *panel = nullptr;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<PanelHost> host;

    World()
    {
        auto p = std::make_unique<FakePanel>("weather", &log);
        panel = p.get();
        CHECK(reg.add(std::move(p)));
        SettingsSchema s;
        s.defs = reg.setting_defs();
        settings = std::make_unique<Settings>(s, storage, &bus);
        settings->load();
        host = std::make_unique<PanelHost>(reg, bus, state, *settings);
    }
};

TEST(manifest_validation)
{
    std::vector<std::string> log;
    std::string err;
    FakePanel ok("clock", &log);
    CHECK(validate_manifest(ok.m, &err));

    FakePanel p("clock", &log);
    p.m.orientations = kLandscape;
    CHECK(!validate_manifest(p.m, &err) && err.find("landscape and portrait") != std::string::npos);
    p = FakePanel("clock", &log);
    p.m.sizes = 0;
    CHECK(!validate_manifest(p.m, &err));
    p = FakePanel("a/b", &log);
    CHECK(!validate_manifest(p.m, &err));
    p = FakePanel("clock", &log);
    p.m.subscriptions = {"x", "x"};
    CHECK(!validate_manifest(p.m, &err));
    p = FakePanel("clock", &log);
    p.m.settings = {{"Bad", true, {}}};
    CHECK(!validate_manifest(p.m, &err));
}

TEST(registry_rejects_duplicates_and_namespaces_settings)
{
    std::vector<std::string> log;
    PanelRegistry reg;
    CHECK(reg.add(std::make_unique<FakePanel>("news", &log)));
    std::string err;
    CHECK(!reg.add(std::make_unique<FakePanel>("news", &log), &err));
    CHECK(err == "duplicate panel id: news");
    CHECK((reg.ids() == std::vector<std::string>{"news"}));
    auto defs = reg.setting_defs();
    CHECK(defs.size() == 1 && defs[0].key == "panels/news/units");
}

TEST(lifecycle_order_is_enforced)
{
    World w;
    PanelView view;
    CHECK(!w.host->show("weather", view));   // not created
    CHECK(!w.host->destroy("weather"));
    CHECK(w.host->create("weather"));
    CHECK(!w.host->create("weather"));
    CHECK(w.host->show("weather", view));
    CHECK(w.panel->lv_objects);
    CHECK(!w.host->destroy("weather"));      // must hide first
    w.host->tick(16);
    CHECK(w.host->hide("weather"));
    CHECK(!w.panel->lv_objects);              // hidden panels hold no LVGL objects
    w.host->tick(32);                         // hidden: no tick
    CHECK(w.host->destroy("weather"));
    CHECK(w.host->state("weather") == PanelState::Registered);
    CHECK((w.log == std::vector<std::string>{"create", "show", "tick", "hide", "destroy"}));
    CHECK(!w.host->create("unknown"));
}

TEST(show_rejects_unsupported_size)
{
    World w;
    w.host->create("weather");
    PanelView tile;
    tile.size = kSizeTile;
    CHECK(!w.host->show("weather", tile));
    CHECK(w.host->state("weather") == PanelState::Created);
}

TEST(failed_create_stays_registered)
{
    World w;
    w.panel->create_ok = false;
    CHECK(!w.host->create("weather"));
    CHECK(w.host->state("weather") == PanelState::Registered);
}

TEST(events_only_while_shown_and_only_declared_topics)
{
    World w;
    w.host->create("weather");
    w.state.set("weather/now", std::string("a"));
    w.bus.dispatch();                         // created, not shown: nothing
    w.host->show("weather", PanelView{});
    w.state.set("weather/now", std::string("b"));
    w.state.set("printer/status", std::string("idle"));  // not declared
    w.bus.dispatch();
    w.state.set("weather/now", std::string("c"));         // queued...
    w.host->hide("weather");                              // ...then hidden
    w.bus.dispatch();
    std::vector<std::string> events;
    for (auto &l : w.log) if (l.rfind("event:", 0) == 0) events.push_back(l);
    CHECK((events == std::vector<std::string>{"event:weather/now:metric"}));
}

TEST(context_reads_state_and_own_settings_only)
{
    World w;
    struct Probe : FakePanel {
        using FakePanel::FakePanel;
        Value seen_state, seen_setting;
        SetResult set_result = SetResult::UnknownKey;
        void show(PanelContext &ctx, const PanelView &) override
        {
            seen_state = ctx.state("weather/now");
            set_result = ctx.set_setting("units", std::string("imperial"));
            seen_setting = ctx.setting("units");
        }
    };
    std::vector<std::string> log;
    PanelRegistry reg;
    auto p = std::make_unique<Probe>("probe", &log);
    Probe *probe = p.get();
    reg.add(std::move(p));
    SettingsSchema s;
    s.defs = reg.setting_defs();
    FakeStorage st;
    Settings settings(s, st, &w.bus);
    settings.load();
    PanelHost host(reg, w.bus, w.state, settings);
    w.state.set("weather/now", std::string("sun"));
    host.create("probe");
    host.show("probe", PanelView{});
    CHECK(std::get<std::string>(probe->seen_state) == "sun");
    CHECK(probe->set_result == SetResult::Ok);
    CHECK(std::get<std::string>(probe->seen_setting) == "imperial");
    CHECK(std::get<std::string>(settings.get("panels/probe/units")) == "imperial");
}

int main()
{
    RUN(manifest_validation);
    RUN(registry_rejects_duplicates_and_namespaces_settings);
    RUN(lifecycle_order_is_enforced);
    RUN(show_rejects_unsupported_size);
    RUN(failed_create_stays_registered);
    RUN(events_only_while_shown_and_only_declared_topics);
    RUN(context_reads_state_and_own_settings_only);
    return report("panels");
}
