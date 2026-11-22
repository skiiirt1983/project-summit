// Placeholder panels for the simulator (real panels come in later phases).
// They exist to exercise the shell: both orientations, size tokens only, glance
// chips declared in the manifest, and a single root object per panel.
#include "demo_panels.h"

#include "summit/recipes/module.h"

#include <functional>
#include <memory>

#include "summit/shell/shell.h"

namespace sim {

using namespace summit;

namespace {

constexpr uint32_t kCard = 0x1E252D;
constexpr uint32_t kText = 0xE6EDF3;
constexpr uint32_t kMuted = 0x9AA7B4;
constexpr uint32_t kAccent = 0x4EA1FF;

std::string str(PanelContext &ctx, const char *key, const char *fallback = "")
{
    Value v = ctx.state(key);
    const std::string *s = std::get_if<std::string>(&v);
    return s ? *s : fallback;
}

lv_obj_t *label(lv_obj_t *parent, const std::string &text, int16_t px, uint32_t color = kText)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text.c_str());
    lv_obj_set_style_text_font(l, shell::font_for(px), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

lv_obj_t *box(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(o, flow);
    return o;
}

lv_obj_t *card(lv_obj_t *parent, const SizeTokens &t, int32_t width_pct)
{
    lv_obj_t *c = box(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(c, lv_pct(width_pct));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c, lv_color_hex(kCard), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, t.radius, 0);
    lv_obj_set_style_pad_all(c, t.space_m, 0);
    lv_obj_set_style_pad_row(c, t.space_s, 0);
    return c;
}

// Grid of cards: `cols` per row, computed from percentages (no pixel sizes).
lv_obj_t *grid(lv_obj_t *parent, const SizeTokens &t)
{
    lv_obj_t *g = box(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_width(g, lv_pct(100));
    lv_obj_set_flex_grow(g, 1);
    lv_obj_set_style_pad_row(g, t.space_m, 0);
    lv_obj_set_style_pad_column(g, t.space_m, 0);
    return g;
}

int32_t col_pct(int cols) { return cols <= 1 ? 100 : (100 - 3 * (cols - 1)) / cols; }

using Render = std::function<void(lv_obj_t *root, PanelContext &, const PanelView &)>;

class DemoPanel : public Panel {
public:
    DemoPanel(PanelManifest m, Render render) : m_(std::move(m)), render_(std::move(render)) {}
    const PanelManifest &manifest() const override { return m_; }
    bool create(PanelContext &) override { return true; }
    void show(PanelContext &ctx, const PanelView &view) override
    {
        view_ = view;
        build(ctx);
    }
    void tick(PanelContext &, uint32_t) override {}
    void on_event(PanelContext &ctx, const std::string &, const Value &) override
    {
        if (root_) {
            lv_obj_delete(root_);
            build(ctx);
        }
    }
    void hide(PanelContext &) override
    {
        if (root_) lv_obj_delete(root_);
        root_ = nullptr;
    }
    void destroy(PanelContext &) override {}

private:
    void build(PanelContext &ctx)
    {
        const SizeTokens &t = view_.tokens;
        root_ = box(view_.lv_parent, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_pad_all(root_, t.space_l, 0);
        lv_obj_set_style_pad_row(root_, t.space_m, 0);
        label(root_, m_.name, t.text_l, kMuted);
        render_(root_, ctx, view_);
    }

    PanelManifest m_;
    Render render_;
    PanelView view_;
    lv_obj_t *root_ = nullptr;
};

PanelManifest manifest(const char *id, const char *name, std::vector<std::string> subs)
{
    PanelManifest m;
    m.id = id;
    m.name = name;
    m.icon = "";
    m.version = "0.0.1-demo";
    m.sizes = kSizeFull;
    m.subscriptions = std::move(subs);
    return m;
}

GlanceChipDef chip(const char *id, const char *topic, int order, uint8_t drop_rank)
{
    GlanceChipDef c;
    c.id = id;
    c.topic = topic;
    c.order = order;
    c.drop_rank = drop_rank;
    return c;
}

}  // namespace

void register_demo_panels(PanelRegistry &reg)
{
    // Home: big clock left, agenda + weather right (landscape); stacked (portrait).
    reg.add(std::make_unique<DemoPanel>(
        manifest("home_clock", "Home", {"time/now_text"}),
        [](lv_obj_t *root, PanelContext &ctx, const PanelView &v) {
            const SizeTokens &t = v.tokens;
            lv_obj_t *g = grid(root, t);
            const int cols = v.orientation == kLandscape ? 2 : 1;
            lv_obj_t *clock = card(g, t, col_pct(cols));
            label(clock, str(ctx, "time/now_text", "--:--"), t.text_xl);
            label(clock, str(ctx, "time/date_text"), t.text_m, kMuted);
            lv_obj_t *side = card(g, t, col_pct(cols));
            label(side, "Next", t.text_s, kMuted);
            label(side, str(ctx, "calendar/next_text"), t.text_m);
            label(side, "Weather", t.text_s, kMuted);
            label(side, str(ctx, "weather/now_text"), t.text_m);
        }));

    auto cal = manifest("calendar", "Calendar", {"calendar/next_text"});
    cal.glances = {chip("next_event", "calendar/next_text", 20, 1)};
    reg.add(std::make_unique<DemoPanel>(cal, [](lv_obj_t *root, PanelContext &, const PanelView &v) {
        const SizeTokens &t = v.tokens;
        lv_obj_t *g = grid(root, t);
        const char *events[] = {"10:00  Standup", "12:30  Lunch with Sam", "15:00  Print enclosure v2",
                                "18:00  Climbing"};
        for (const char *e : events) label(card(g, t, col_pct(v.orientation == kLandscape ? 2 : 1)), e, t.text_m);
    }));

    auto wx = manifest("weather", "Weather", {"weather/now_text"});
    wx.glances = {chip("weather", "weather/now_text", 30, 2)};
    reg.add(std::make_unique<DemoPanel>(wx, [](lv_obj_t *root, PanelContext &ctx, const PanelView &v) {
        const SizeTokens &t = v.tokens;
        label(root, str(ctx, "weather/now_text", "--"), t.text_xl);
        lv_obj_t *g = grid(root, t);
        const char *days[] = {"Mon 5\xC2\xB0", "Tue 7\xC2\xB0", "Wed 3\xC2\xB0", "Thu 2\xC2\xB0",
                              "Fri 6\xC2\xB0", "Sat 8\xC2\xB0", "Sun 9\xC2\xB0"};
        for (const char *d : days) label(card(g, t, col_pct(v.orientation == kLandscape ? 7 : 4)), d, t.text_s);
    }));

    reg.add(std::make_unique<DemoPanel>(
        manifest("news", "News", {}), [](lv_obj_t *root, PanelContext &, const PanelView &v) {
            const SizeTokens &t = v.tokens;
            lv_obj_t *g = grid(root, t);
            const char *items[] = {"Local trail reopens after storm", "New PETG blend tested",
                                   "City council approves bike lanes"};
            for (const char *s : items) {
                lv_obj_t *c = card(g, t, col_pct(v.orientation == kLandscape ? 3 : 1));
                lv_obj_t *l = label(c, s, t.text_m);
                lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
                lv_obj_set_width(l, lv_pct(100));
            }
        }));

    auto pr = manifest("printer", "Printer", {"printer/progress_pct"});
    pr.glances = {chip("printer", "printer/progress_text", 40, 3)};
    reg.add(std::make_unique<DemoPanel>(pr, [](lv_obj_t *root, PanelContext &ctx, const PanelView &v) {
        const SizeTokens &t = v.tokens;
        Value p = ctx.state("printer/progress_pct");
        const int64_t *pct = std::get_if<int64_t>(&p);
        lv_obj_t *arc = lv_arc_create(root);
        const int32_t side = (v.width < v.height ? v.width : v.height) / 2;  // from the view, not a constant
        lv_obj_set_size(arc, side, side);
        lv_arc_set_value(arc, pct ? static_cast<int32_t>(*pct) : 0);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(arc, t.space_m, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, t.space_m, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(kAccent), LV_PART_INDICATOR);
        lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        label(root, str(ctx, "printer/progress_text", "Idle"), t.text_m);
    }));
}

void register_demo_modules(ModuleManager &modules) { modules.add(summit::recipes::module_manifest()); }

void seed_recipes(recipes::RecipeStore &store)
{
    using namespace recipes;
    Recipe r;
    std::string err;
    parse_recipe(R"({"format":1,"title":"Pancakes","servings":4,
      "ingredients":[{"qty":1.5,"unit":"cup","item":"flour","note":"sifted"},{"qty":2,"unit":"","item":"eggs"},
                     {"qty":250,"unit":"ml","item":"milk"},{"qty":null,"unit":"","item":"salt","note":"to taste"}],
      "steps":[{"text":"Whisk the flour, eggs and milk until smooth."},{"text":"Rest the batter for 30 minutes.","timer_s":1800},
               {"text":"Fry 2-3 minutes per side until golden."}],
      "source":{"type":"manual"},"tags":["breakfast","quick"],"created":"2026-11-12T09:00:00"})", r, &err);
    store.save("pancakes", r);
    parse_recipe(R"({"format":1,"title":"Tomato soup","servings":2,
      "ingredients":[{"qty":800,"unit":"g","item":"tomatoes"},{"qty":1,"unit":"","item":"onion"},{"qty":0.5,"unit":"l","item":"stock"}],
      "steps":[{"text":"Soften the onion for 5 minutes."},{"text":"Add tomatoes and stock; simmer 20 to 25 minutes."},{"text":"Blend until smooth."}],
      "source":{"type":"manual"},"tags":["dinner"],"created":"2026-11-12T09:05:00"})", r, &err);
    store.save("tomato-soup", r);
}

void publish_placeholders(StateStore &s, bool with_network)
{
    s.set("time/now_text", std::string("09:41"));
    s.set("time/date_text", std::string("Saturday 7 November"));
    s.set("calendar/next_text", std::string("Standup in 19 min"));
    s.set("weather/now_text", std::string("4\xC2\xB0" "C Cloudy"));
    s.set("printer/progress_text", std::string("Enclosure v2 \xE2\x80\xA2 42%"));
    s.set("printer/progress_pct", int64_t{42});
    if (with_network) s.set("net/wifi_connected", true);  // the device has the real Wi-Fi state
    s.set("update/available", true);
    s.set("power/battery_text", std::string("85%"));
    s.set("power/battery_icon", std::string(LV_SYMBOL_BATTERY_3));
    // Device facts and update pre-flight inputs (published by boot and services on the device).
    s.set("system/firmware_version", std::string("0.0.1"));
    s.set("system/hardware_id", std::string("waveshare-p4-wifi6-lcd5-rev3"));
    s.set("power/usb_power", true);
    s.set("power/battery_pct", int64_t{85});
    if (with_network) s.set("time/synced", true);  // the device sets it when the clock has synced
    s.set("update/previous_version", std::string("0.0.1-alpha.3"));
}

}  // namespace sim
