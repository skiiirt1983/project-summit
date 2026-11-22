#include <algorithm>
#include <cstdlib>

#include "summit/shell/shell.h"
#include "summit/shell/ui_motion.h"
#include "theme.h"

namespace summit::shell {

namespace {

using namespace theme;
constexpr uint32_t kBackdropOpa = LV_OPA_50;
constexpr uint32_t kSlideMs = motion::kPageMs;

lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

void anim_opa(lv_obj_t *obj, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(o), static_cast<lv_opa_t>(v), 0);
    });
    lv_anim_start(&a);
}

bool state_bool(StateStore &s, const char *key)
{
    Value v = s.get(key);
    const bool *b = std::get_if<bool>(&v);
    return b && *b;
}

const char *kRotationMap[] = {"Off", "Landscape", "Portrait", ""};
const char *kRotationValues[] = {"off", "landscape", "portrait"};

}  // namespace

const lv_font_t *font_for(int16_t px)
{
    struct F {
        int16_t px;
        const lv_font_t *font;
    };
    static const F fonts[] = {
#if LV_FONT_MONTSERRAT_14
        {14, &lv_font_montserrat_14},
#endif
#if LV_FONT_MONTSERRAT_16
        {16, &lv_font_montserrat_16},
#endif
#if LV_FONT_MONTSERRAT_20
        {20, &lv_font_montserrat_20},
#endif
#if LV_FONT_MONTSERRAT_24
        {24, &lv_font_montserrat_24},
#endif
#if LV_FONT_MONTSERRAT_32
        {32, &lv_font_montserrat_32},
#endif
#if LV_FONT_MONTSERRAT_40
        {40, &lv_font_montserrat_40},
#endif
#if LV_FONT_MONTSERRAT_48
        {48, &lv_font_montserrat_48},
#endif
    };
    const lv_font_t *best = LV_FONT_DEFAULT;
    int best_d = 1 << 30;
    for (const F &f : fonts) {
        int d = std::abs(f.px - px);
        if (d < best_d) {
            best_d = d;
            best = f.font;
        }
    }
    return best;
}

Shell::Shell(Deps deps, ApplyOrientation apply) : d_(deps), apply_(std::move(apply)) {}

Shell::~Shell()
{
    for (auto id : {toast_sub_, critical_sub_, link_sub_, order_sub_, open_sub_})
        if (id) d_.bus.unsubscribe(id);
    for (auto id : module_subs_) d_.bus.unsubscribe(id);
    drop_ghost();
}

void Shell::rebuild_pages()
{
    Value v = d_.settings.get("ui/page_order");
    const std::string *order = std::get_if<std::string>(&v);
    const std::string current = pager_ ? pager_->current() : std::string();
    std::vector<std::string> hidden;
    if (d_.modules) hidden = d_.modules->hidden_panels(d_.settings);  // uninstalled modules
    pager_ = std::make_unique<Pager>(page_order(d_.registry.ids(), order ? *order : std::string(), hidden));
    if (!current.empty()) pager_->go_to(current);  // stay on the same panel
}

void Shell::on_modules_changed()
{
    const std::string before = pager_->current();
    rebuild_pages();
    // The page being shown was removed: fall back to the first page.
    if (pager_->current() != before) show_current(0);
}

bool Shell::start(uint32_t now_ms, std::string *error)
{
    now_ = now_ms;
    std::vector<GlanceChipDef> defs;
    if (!collect_chips(d_.registry.glance_chips(), defs, error)) return false;
    if (d_.registry.ids().empty()) {
        if (error) *error = "no panels registered";
        return false;
    }
    rebuild_pages();

    lv_display_t *disp = lv_display_get_default();
    const int32_t w = lv_display_get_horizontal_resolution(disp);
    const int32_t h = lv_display_get_vertical_resolution(disp);
    rotation_ = RotationController(w >= h ? kLandscape : kPortrait);

    screen_ = lv_screen_active();
    lv_obj_set_style_bg_color(screen_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    content_ = plain(screen_);
    page_ = plain(content_);
    // Opaque, so the picture taken of an outgoing page (show_current) has the page colour, not black.
    lv_obj_set_style_bg_color(page_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(page_, LV_OPA_COVER, 0);
    dots_ = plain(lv_layer_top());
    lv_obj_set_flex_flow(dots_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(dots_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dots_, LV_OBJ_FLAG_HIDDEN);
    updates_ = std::make_unique<UpdatesScreen>(d_.bus, d_.state, d_.settings);
    updates_->build(screen_);  // above the page, below the strip (top layer)
    if (d_.modules) {
        modules_ = std::make_unique<ModulesScreen>(d_.bus, d_.state, d_.settings, *d_.modules);
        modules_->build(screen_);
        auto changed = [this](const std::string &, const Value &) { on_modules_changed(); };
        for (const ModuleManifest &m : d_.modules->modules())
            module_subs_.push_back(d_.bus.subscribe("settings/" + ModuleManager::installed_key(m.id), changed));
        module_subs_.push_back(d_.bus.subscribe("settings/reset", changed));
    }

    for (const GlanceChipDef &def : defs) {
        ChipUi c;
        c.def = def;
        chips_.push_back(c);
    }
    build_strip();
    build_quick_settings();

    toast_sub_ = d_.bus.subscribe(topics::kToast, [this](const std::string &, const Value &v) { on_toast(v, false); });
    critical_sub_ = d_.bus.subscribe(topics::kToastCritical,
                                     [this](const std::string &, const Value &v) { on_toast(v, true); });
    link_sub_ = d_.bus.subscribe(topics::kToastLink, [this](const std::string &, const Value &v) { on_toast_link(v); });
    open_sub_ = d_.bus.subscribe(topics::kOpenScreen, [this](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);
        if (s && *s == "settings/updates") {
            close_quick_settings();
            if (modules_) modules_->close();
            updates_->open();
        } else if (s && *s == "settings/modules" && modules_) {
            close_quick_settings();
            updates_->close();
            modules_->open();
        }
    });
    order_sub_ = d_.bus.subscribe("settings/ui/page_order",
                                  [this](const std::string &, const Value &) { rebuild_pages(); });

    update_hide_reasons();
    apply_layout(false);
    refresh_chips(true);
    refresh_strip_visibility();
    return true;
}

void Shell::build_strip()
{
    strip_ = plain(lv_layer_top());
    lv_obj_set_style_bg_color(strip_, lv_color_hex(kStripBg), 0);
    lv_obj_set_style_bg_opa(strip_, LV_OPA_COVER, 0);
    lv_obj_add_flag(strip_, LV_OBJ_FLAG_CLICKABLE);
    // Gestures go to the first ancestor that doesn't bubble them: stop here.
    lv_obj_remove_flag(strip_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(strip_, strip_gesture_cb, LV_EVENT_GESTURE, this);
    lv_obj_add_event_cb(strip_, strip_click_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(strip_, strip_long_press_cb, LV_EVENT_LONG_PRESSED, this);

    row_ = plain(strip_);
    lv_obj_set_flex_flow(row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(row_, LV_OBJ_FLAG_EVENT_BUBBLE);

    for (size_t i = 0; i < chips_.size(); ++i) {
        ChipUi &c = chips_[i];
        if (c.def.side == ChipSide::Right && !spacer_) {
            spacer_ = plain(row_);
            lv_obj_set_flex_grow(spacer_, 1);
            lv_obj_set_height(spacer_, 1);
        }
        c.obj = plain(row_);
        lv_obj_set_flex_flow(c.obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(c.obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_size(c.obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(c.obj, lv_color_hex(kChipBg), 0);
        lv_obj_set_style_bg_opa(c.obj, c.def.side == ChipSide::Left ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_add_flag(c.obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(c.obj, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_user_data(c.obj, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(c.obj, chip_clicked_cb, LV_EVENT_SHORT_CLICKED, this);
        ui_motion::press_feedback(c.obj, ui_motion::press_travel());
        c.icon = lv_label_create(c.obj);
        c.text = lv_label_create(c.obj);
        lv_obj_set_style_text_color(c.icon, lv_color_hex(kText), 0);
        lv_obj_set_style_text_color(c.text, lv_color_hex(kText), 0);
        lv_label_set_text(c.icon, "");
        lv_label_set_text(c.text, "");
        lv_obj_add_flag(c.obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (!spacer_) {
        spacer_ = plain(row_);
        lv_obj_set_flex_grow(spacer_, 1);
    }

    toast_label_ = lv_label_create(strip_);
    lv_obj_set_style_text_color(toast_label_, lv_color_hex(kText), 0);
    lv_label_set_long_mode(toast_label_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(toast_label_, LV_OBJ_FLAG_HIDDEN);
}

void Shell::build_quick_settings()
{
    backdrop_ = plain(lv_layer_top());
    lv_obj_set_style_bg_color(backdrop_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop_, kBackdropOpa, 0);
    lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(backdrop_, sheet_event_cb, LV_EVENT_CLICKED, this);

    sheet_ = plain(lv_layer_top());
    lv_obj_set_style_bg_color(sheet_, lv_color_hex(kStripBg), 0);
    lv_obj_set_style_bg_opa(sheet_, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(sheet_, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(sheet_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(sheet_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(sheet_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(sheet_, sheet_event_cb, LV_EVENT_GESTURE, this);

    auto row = [this](const char *title) {
        lv_obj_t *r = plain(sheet_);
        lv_obj_set_width(r, lv_pct(100));
        lv_obj_set_height(r, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *l = lv_label_create(r);
        lv_label_set_text(l, title);
        lv_obj_set_style_text_color(l, lv_color_hex(kMuted), 0);
        return r;
    };
    brightness_ = lv_slider_create(row("Brightness"));
    lv_slider_set_range(brightness_, 5, 100);
    volume_ = lv_slider_create(row("Volume"));
    lv_slider_set_range(volume_, 0, 100);
    rotation_buttons_ = lv_buttonmatrix_create(row("Rotation lock"));
    lv_buttonmatrix_set_map(rotation_buttons_, kRotationMap);
    lv_buttonmatrix_set_button_ctrl_all(rotation_buttons_, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(rotation_buttons_, true);
    dnd_ = lv_switch_create(row("Do not disturb"));

    // System screens (there is no settings menu yet).
    {
        lv_obj_t *r = row("More");
        lv_obj_t *box = plain(r);
        lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        auto make = [&](const char *text) {
            lv_obj_t *b = lv_button_create(box);
            lv_obj_set_style_bg_color(b, lv_color_hex(kChipBg), 0);
            lv_obj_set_style_shadow_width(b, 0, 0);
            lv_obj_t *l = lv_label_create(b);
            lv_label_set_text(l, text);
            lv_obj_set_style_text_color(l, lv_color_hex(kText), 0);
            lv_obj_add_event_cb(b, sheet_event_cb, LV_EVENT_CLICKED, this);
            ui_motion::press_feedback(b, ui_motion::press_travel());
            return b;
        };
        if (d_.modules) modules_btn_ = make("Modules");
        updates_btn_ = make("Updates");
        more_box_ = box;
    }

    // Settings notice (newer version / unreadable) with an explicit, two-tap reset.
    notice_row_ = plain(sheet_);
    lv_obj_set_width(notice_row_, lv_pct(100));
    lv_obj_set_height(notice_row_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(notice_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(notice_row_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    notice_label_ = lv_label_create(notice_row_);
    lv_label_set_long_mode(notice_label_, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_flex_grow(notice_label_, 1);
    lv_obj_set_style_text_color(notice_label_, lv_color_hex(kText), 0);
    reset_btn_ = lv_button_create(notice_row_);
    reset_label_ = lv_label_create(reset_btn_);
    lv_label_set_text(reset_label_, "Reset settings");
    lv_obj_set_style_bg_color(reset_btn_, lv_color_hex(kChipBg), 0);
    lv_obj_set_style_shadow_width(reset_btn_, 0, 0);
    lv_obj_add_event_cb(reset_btn_, sheet_event_cb, LV_EVENT_CLICKED, this);
    ui_motion::press_feedback(reset_btn_, ui_motion::press_travel());
    lv_obj_add_flag(notice_row_, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t *o : {brightness_, volume_}) {
        lv_obj_add_event_cb(o, sheet_event_cb, LV_EVENT_RELEASED, this);  // persist once, on release
        lv_obj_add_event_cb(o, sheet_event_cb, LV_EVENT_VALUE_CHANGED, this);  // live value while dragging
        lv_obj_set_style_bg_color(o, lv_color_hex(kAccent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(o, lv_color_hex(kAccent), LV_PART_KNOB);
    }
    lv_obj_add_event_cb(rotation_buttons_, sheet_event_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(dnd_, sheet_event_cb, LV_EVENT_VALUE_CHANGED, this);

    // Match the dark theme (colours only; sizes come from tokens in style_strip_fonts()).
    for (lv_obj_t *o : {brightness_, volume_}) lv_obj_set_style_bg_color(o, lv_color_hex(kChipBg), LV_PART_MAIN);
    lv_obj_set_style_bg_color(rotation_buttons_, lv_color_hex(kChipBg), LV_PART_MAIN);
    lv_obj_set_style_border_width(rotation_buttons_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rotation_buttons_, lv_color_hex(kChipBg), LV_PART_ITEMS);
    lv_obj_set_style_text_color(rotation_buttons_, lv_color_hex(kText), LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(rotation_buttons_, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(rotation_buttons_, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(rotation_buttons_, lv_color_hex(kAccent), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(rotation_buttons_, lv_color_hex(kBg), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(dnd_, lv_color_hex(kChipBg), LV_PART_MAIN);
    lv_obj_set_style_bg_color(dnd_, lv_color_hex(kAccent), LV_PART_INDICATOR | LV_STATE_CHECKED);
}

void Shell::style_strip_fonts()
{
    const Tokens &t = layout_.tokens;
    const lv_font_t *chip_font = font_for(t.text_m);
    for (ChipUi &c : chips_) {
        lv_obj_set_style_text_font(c.icon, chip_font, 0);
        lv_obj_set_style_text_font(c.text, chip_font, 0);
        lv_obj_set_style_pad_hor(c.obj, t.space_s, 0);
        lv_obj_set_style_pad_ver(c.obj, t.space_s / 2, 0);
        lv_obj_set_style_pad_column(c.obj, t.space_s / 2, 0);
        lv_obj_set_style_radius(c.obj, t.radius, 0);
    }
    lv_obj_set_style_pad_hor(row_, t.space_m, 0);
    lv_obj_set_style_pad_column(row_, t.space_s, 0);
    lv_obj_set_style_text_font(toast_label_, font_for(t.text_m), 0);

    lv_obj_set_style_pad_all(sheet_, t.space_l, 0);
    lv_obj_set_style_pad_row(sheet_, t.space_m, 0);
    lv_obj_set_style_radius(sheet_, t.radius, 0);
    lv_obj_set_style_text_font(sheet_, font_for(t.text_m), 0);
    for (lv_obj_t *o : {brightness_, volume_}) {
        lv_obj_set_width(o, layout_.strip.w / 2);
        lv_obj_set_height(o, t.space_s);
    }
    lv_obj_set_size(rotation_buttons_, layout_.strip.w / 2, t.unit);
    lv_obj_set_style_radius(rotation_buttons_, t.radius, LV_PART_MAIN);
    lv_obj_set_style_radius(rotation_buttons_, t.radius, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(rotation_buttons_, t.space_s / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(rotation_buttons_, t.space_s / 2, LV_PART_MAIN);
    lv_obj_set_size(dnd_, t.unit * 3 / 2, t.space_m + t.space_s);
    lv_obj_set_style_pad_column(notice_row_, t.space_m, 0);
    lv_obj_set_style_pad_column(more_box_, t.space_s, 0);
    for (lv_obj_t *b : {modules_btn_, updates_btn_}) {
        if (!b) continue;
        lv_obj_set_style_pad_hor(b, t.space_m, 0);
        lv_obj_set_style_pad_ver(b, t.space_s, 0);
        lv_obj_set_style_radius(b, t.radius, 0);
    }
    lv_obj_set_style_pad_all(reset_btn_, t.space_s, 0);
    lv_obj_set_style_radius(reset_btn_, t.radius, 0);
}

void Shell::apply_layout(bool morph)
{
    lv_display_t *disp = lv_display_get_default();
    const auto w = static_cast<int16_t>(lv_display_get_horizontal_resolution(disp));
    const auto h = static_cast<int16_t>(lv_display_get_vertical_resolution(disp));
    const Layout old = layout_;
    layout_ = layout_for(w, h, !visibility_.hidden_by_mode());

    const Rect &c = layout_.content;
    lv_obj_set_pos(content_, c.x, c.y);
    lv_obj_set_size(content_, c.w, c.h);
    lv_obj_set_size(page_, c.w, c.h);

    const Rect &s = toast_on_ ? layout_.strip_toast : layout_.strip;
    lv_obj_set_pos(strip_, s.x, s.y);
    lv_obj_set_size(strip_, s.w, s.h);
    lv_obj_set_pos(row_, 0, 0);
    lv_obj_set_size(row_, layout_.strip.w, layout_.strip.h);
    lv_obj_set_pos(toast_label_, layout_.tokens.space_m, layout_.strip.h + layout_.tokens.space_s);
    lv_obj_set_width(toast_label_, layout_.strip.w - 2 * layout_.tokens.space_m);

    lv_obj_set_size(backdrop_, w, h);
    lv_obj_set_pos(sheet_, 0, s.h);
    lv_obj_set_size(sheet_, w, LV_SIZE_CONTENT);
    style_strip_fonts();

    const bool changed = old.content.w != c.w || old.content.h != c.h || old.content.y != c.y ||
                         old.orientation != layout_.orientation;
    if (changed && pager_) show_current(0);
    if (updates_) updates_->apply_layout(layout_);
    if (modules_) modules_->apply_layout(layout_);
    fit_chip_row();
    if (morph) anim_opa(content_, RotationController::kMorphMs);
}

void Shell::show_current(int slide_dir)
{
    const std::string &id = pager_->current();
    const int32_t w = layout_.content.w;
    // Where the outgoing page is right now: under the finger after a drag, else at rest.
    const int32_t start_x = slide_dir != 0 ? lv_obj_get_x(page_) : 0;
    lv_anim_delete(page_, nullptr);
    drop_ghost();
#if LV_USE_SNAPSHOT
    // Only one panel holds LVGL objects at a time, so the outgoing page leaves as a picture
    // of itself. If there is no memory for the picture the new page still slides in.
    if (slide_dir != 0 && lv_obj_get_child_count(page_) > 0) {
        ghost_buf_ = lv_snapshot_take(page_, LV_COLOR_FORMAT_NATIVE);
        if (ghost_buf_) {
            ghost_ = lv_image_create(content_);
            lv_image_set_src(ghost_, ghost_buf_);
            lv_obj_set_pos(ghost_, start_x, 0);
            lv_obj_move_background(ghost_);
        }
    }
#endif
    // Every registered panel, not just the current pages: a module's panel may have
    // just been removed from the pages while it was showing.
    for (const std::string &other : d_.registry.ids()) {
        if (d_.host.state(other) == PanelState::Shown) d_.host.hide(other);
    }
    lv_obj_clean(page_);  // hidden panels hold no LVGL objects
    if (d_.host.state(id) == PanelState::Registered) d_.host.create(id);
    PanelView v;
    v.lv_parent = page_;
    v.size = kSizeFull;
    v.orientation = layout_.orientation;
    v.width = layout_.content.w;
    v.height = layout_.content.h;
    v.tokens = layout_.tokens;
    d_.host.show(id, v);
    d_.state.set(topics::kCurrentPanel, id);
    if (slide_dir != 0) {
        // Both pages travel together, edge to edge, and a flick keeps the finger's speed.
        const int32_t from = start_x + slide_dir * w;
        const uint32_t ms = motion::settle_ms(from, velocity_, motion::kPageMs);
        ui_motion::animate(page_, ui_motion::Prop::X, from, 0, ms, motion::kEnter);
        if (ghost_) {
            ui_motion::animate(ghost_, ui_motion::Prop::X, start_x, -slide_dir * w, ms, motion::kEnter);
            ghost_until_ = lv_tick_get() + ms;
        }
        show_dots();
    } else {
        lv_obj_set_x(page_, 0);
    }
    velocity_ = 0.0f;
}

// The picture of the outgoing page (see show_current).
void Shell::drop_ghost()
{
    if (ghost_) {
        lv_anim_delete(ghost_, nullptr);
        lv_obj_delete(ghost_);
        ghost_ = nullptr;
    }
    if (ghost_buf_) {
        lv_draw_buf_destroy(ghost_buf_);
        ghost_buf_ = nullptr;
    }
}

// Page dots: which page of how many. Shown while swiping and for a moment after, then they fade.
void Shell::show_dots()
{
    if (!pager_ || !dots_) return;
    const uint32_t n = static_cast<uint32_t>(pager_->count());
    const Tokens &t = layout_.tokens;
    const int32_t d = std::max<int32_t>(4, t.space_s / 2);
    if (lv_obj_get_child_count(dots_) != n) {
        lv_obj_clean(dots_);
        for (uint32_t i = 0; i < n; ++i) {
            lv_obj_t *dot = plain(dots_);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex(kText), 0);
        }
    }
    lv_obj_set_style_pad_column(dots_, d, 0);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t *dot = lv_obj_get_child(dots_, static_cast<int32_t>(i));
        const bool on = i == pager_->index();
        lv_obj_set_size(dot, on ? 2 * d : d, d);  // the current page is a short bar
        lv_obj_set_style_bg_opa(dot, on ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    lv_obj_set_size(dots_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(dots_, LV_ALIGN_BOTTOM_MID, 0, -t.space_s);
    lv_obj_remove_flag(dots_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(dots_, LV_OPA_COVER, 0);
    dots_shown_ms_ = lv_tick_get();
    dots_on_ = n > 1;
    if (!dots_on_) lv_obj_add_flag(dots_, LV_OBJ_FLAG_HIDDEN);
}

void Shell::refresh_chips(bool force_fit)
{
    bool changed = force_fit;
    for (ChipUi &c : chips_) {
        Value v = d_.state.get(c.def.topic);
        Value icon = c.def.icon_topic.empty() ? Value{} : d_.state.get(c.def.icon_topic);
        if (!force_fit && v == c.last_value && icon == c.last_icon) continue;
        c.last_value = v;
        c.last_icon = icon;
        ChipContent content = chip_content(v);
        c.has_data = content.visible;
        lv_label_set_text(c.text, content.text.c_str());
        if (content.text.empty()) {
            lv_obj_add_flag(c.text, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(c.text, LV_OBJ_FLAG_HIDDEN);
        }
        const std::string *icon_s = std::get_if<std::string>(&icon);
        const std::string &glyph = (icon_s && !icon_s->empty()) ? *icon_s : c.def.icon;
        lv_label_set_text(c.icon, glyph.c_str());
        if (glyph.empty()) {
            lv_obj_add_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
        }
        changed = true;
    }
    if (changed) fit_chip_row();
}

void Shell::fit_chip_row()
{
    // Lay out every chip that has data, measure, then hide the ones that don't fit.
    for (ChipUi &c : chips_) {
        if (c.has_data) {
            lv_obj_remove_flag(c.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(c.obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_update_layout(row_);
    std::vector<ChipWidth> widths;
    for (size_t i = 0; i < chips_.size(); ++i) {
        const ChipUi &c = chips_[i];
        if (c.has_data) {
            widths.push_back({c.def.id, c.def.drop_rank, static_cast<int>(i),
                              static_cast<int>(lv_obj_get_width(c.obj))});
        }
    }
    const int available = layout_.strip.w - 2 * layout_.tokens.space_m;
    std::vector<std::string> keep = fit_chips(widths, available, layout_.tokens.space_s);
    for (ChipUi &c : chips_) {
        c.fits = std::find(keep.begin(), keep.end(), c.def.id) != keep.end();
        if (c.has_data && !c.fits) lv_obj_add_flag(c.obj, LV_OBJ_FLAG_HIDDEN);
    }
}

std::vector<std::string> Shell::visible_chips() const
{
    std::vector<std::string> out;
    for (const ChipUi &c : chips_) {
        if (c.has_data && c.fits) out.push_back(c.def.id);
    }
    return out;
}

bool Shell::strip_visible() const { return !lv_obj_has_flag(strip_, LV_OBJ_FLAG_HIDDEN); }

void Shell::refresh_strip_visibility()
{
    // A critical toast shows the strip even in full-screen modes (as an overlay).
    if (visibility_.visible(now_) || (toast_on_ && toast_critical_)) {
        lv_obj_remove_flag(strip_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(strip_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Shell::update_hide_reasons()
{
    const bool was = visibility_.hidden_by_mode();
    visibility_.set_reason(kHideFullscreen, state_bool(d_.state, topics::kFullscreen));
    visibility_.set_reason(kHideAlarm, state_bool(d_.state, topics::kAlarmRinging));
    visibility_.set_reason(kHideAmbient, state_bool(d_.state, topics::kAmbient));
    visibility_.set_reason(kHideUpdate, state_bool(d_.state, topics::kUpdating));
    if (was != visibility_.hidden_by_mode() && content_) apply_layout(false);
}

void Shell::on_toast(const Value &v, bool critical)
{
    const std::string *s = std::get_if<std::string>(&v);
    if (s && !s->empty()) toasts_.push({*s, "", critical, 0, ""}, now_);
}

// "<screen id>\n<text>": a toast that opens a screen when tapped. Never opens it by itself:
// someone may be cooking (D-012 follow-up).
void Shell::on_toast_link(const Value &v)
{
    const std::string *s = std::get_if<std::string>(&v);
    if (!s) return;
    const size_t nl = s->find('\n');
    if (nl == std::string::npos || nl == 0 || nl + 1 >= s->size()) return;
    toasts_.push({s->substr(nl + 1), "", false, 0, s->substr(0, nl)}, now_);
}

// A tapped link toast: put the screen's panel on show (first path segment = panel id), hand the
// request to it through state (it may be hidden and miss the event) and publish the event too.
void Shell::open_target(const std::string &screen)
{
    d_.state.set("ui/open_request", screen);
    go_to_panel(screen.substr(0, screen.find('/')));
    d_.bus.publish(topics::kOpenScreen, screen);
}

void Shell::strip_click_cb(lv_event_t *e)
{
    auto *self = static_cast<Shell *>(lv_event_get_user_data(e));
    lv_indev_t *indev = lv_indev_active();
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) return;  // a swipe, not a tap
    if (self->toast_on_ && !self->toast_action_.empty()) {
        const std::string action = self->toast_action_;
        self->toast_action_.clear();  // one tap, one open
        self->open_target(action);
    }
}

void Shell::tick(uint32_t now_ms)
{
    now_ = now_ms;

    // Rotation: sensor report + lock setting, 400 ms debounce, 250 ms morph.
    Value o = d_.state.get(topics::kOrientation);
    if (const std::string *s = std::get_if<std::string>(&o)) {
        if (*s == "landscape") rotation_.report(kLandscape, now_ms);
        if (*s == "portrait") rotation_.report(kPortrait, now_ms);
    }
    Value lock = d_.settings.get("ui/rotation_lock");
    if (const std::string *s = std::get_if<std::string>(&lock)) rotation_.set_lock(*s);
    d_.state.set(topics::kRotationLocked, rotation_.lock() != "off");
    if (rotation_.update(now_ms)) {
        if (apply_) apply_(rotation_.applied());
        apply_layout(true);
    }

    update_hide_reasons();

    // Normal toasts wait while the strip is hidden; critical ones show anyway.
    const Toast *t = toasts_.current(now_ms, visibility_.visible(now_ms));
    if ((t != nullptr) != toast_on_ || (t && t->text != toast_text_)) {
        toast_on_ = t != nullptr;
        toast_critical_ = t && t->critical;
        toast_text_ = t ? t->text : std::string();
        toast_action_ = t ? t->action : std::string();
        if (t) {
            lv_label_set_text(toast_label_, t->text.c_str());
            lv_obj_remove_flag(toast_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(toast_label_, LV_OBJ_FLAG_HIDDEN);
        }
        const Rect &s = toast_on_ ? layout_.strip_toast : layout_.strip;
        lv_obj_set_height(strip_, s.h);
        lv_obj_set_y(sheet_, s.h);  // the sheet sits below the strip, expanded or not
    }

    d_.host.tick(now_ms);  // shown panels
    refresh_chips(false);
    updates_->tick();
    if (modules_) modules_->tick();
    refresh_settings_notice();
    refresh_strip_visibility();

    // Drag-to-swipe: LVGL does not forward PRESSING to input listeners, so follow
    // the finger here between PRESSED and RELEASED.
    if (dragging_ && indev_) {
        lv_point_t p;
        lv_indev_get_point(indev_, &p);
        const int32_t dx = p.x - press_.x, dy = p.y - press_.y;
        if (!drag_horizontal_ && std::abs(dx) > layout_.tokens.space_m && std::abs(dx) > std::abs(dy))
            drag_horizontal_ = true;
        if (drag_horizontal_) {
            const bool edge = (dx < 0 && pager_->index() + 1 >= pager_->count()) ||
                              (dx > 0 && pager_->index() == 0);
            // Past the first / last page the page follows the finger less and less.
            lv_obj_set_x(page_, edge ? motion::rubber_band(dx, layout_.content.w) : dx);
            if (!dots_on_ || lv_tick_elaps(dots_shown_ms_) > 200) show_dots();
            const uint32_t now = lv_tick_get();
            if (now != last_ms_) velocity_ = static_cast<float>(p.x - last_.x) / static_cast<float>(now - last_ms_);
            last_ = p;
            last_ms_ = now;
        }
    }

    if (ghost_ && static_cast<int32_t>(lv_tick_get() - ghost_until_) >= 0) drop_ghost();
    if (dots_on_) {
        const uint8_t opa = motion::dots_opacity(lv_tick_elaps(dots_shown_ms_));
        lv_obj_set_style_opa(dots_, opa, 0);  // a handful of small dots: cheap to fade
        if (opa == 0) {
            dots_on_ = false;
            lv_obj_add_flag(dots_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (sheet_open_) sync_quick_settings();  // settings may change elsewhere (web UI)
}

bool Shell::go_to_panel(const std::string &id, bool animate)
{
    const size_t old = pager_->index();
    if (!pager_->go_to(id)) return false;
    show_current(animate ? (pager_->index() > old ? 1 : -1) : 0);
    return true;
}

bool Shell::next_page()
{
    if (!pager_->next()) return false;
    show_current(1);
    return true;
}

bool Shell::prev_page()
{
    if (!pager_->prev()) return false;
    show_current(-1);
    return true;
}

void Shell::sync_quick_settings()
{
    auto get_int = [this](const char *key, int32_t fallback) {
        Value v = d_.settings.get(key);
        const int64_t *i = std::get_if<int64_t>(&v);
        return i ? static_cast<int32_t>(*i) : fallback;
    };
    if (!lv_obj_has_state(brightness_, LV_STATE_PRESSED))
        lv_slider_set_value(brightness_, get_int("display/brightness", 80), LV_ANIM_OFF);
    if (!lv_obj_has_state(volume_, LV_STATE_PRESSED))
        lv_slider_set_value(volume_, get_int("audio/volume", 60), LV_ANIM_OFF);
    for (uint32_t i = 0; i < 3; ++i) {
        if (rotation_.lock() == kRotationValues[i]) {
            lv_buttonmatrix_set_button_ctrl(rotation_buttons_, i, LV_BUTTONMATRIX_CTRL_CHECKED);
        } else {
            lv_buttonmatrix_clear_button_ctrl(rotation_buttons_, i, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
    }
    Value dnd = d_.settings.get("system/dnd");
    const bool *on = std::get_if<bool>(&dnd);
    if (on && *on) {
        lv_obj_add_state(dnd_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(dnd_, LV_STATE_CHECKED);
    }
}

void Shell::open_quick_settings()
{
    if (sheet_open_) return;
    sheet_open_ = true;
    sync_quick_settings();
    lv_obj_move_foreground(backdrop_);
    lv_obj_move_foreground(sheet_);
    lv_obj_move_foreground(strip_);
    // Drops out from behind the strip while the page dims.
    lv_obj_remove_flag(sheet_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(sheet_);
    ui_motion::sheet_down(sheet_, backdrop_, lv_obj_get_height(sheet_), kBackdropOpa);
}

void Shell::close_quick_settings()
{
    if (!sheet_open_) return;
    sheet_open_ = false;
    ui_motion::sheet_up(sheet_, backdrop_, lv_obj_get_height(sheet_), kBackdropOpa);
}

void Shell::toggle_rotation_lock()
{
    const std::string lock = rotation_.toggled_lock();
    d_.settings.set("ui/rotation_lock", lock);
    rotation_.set_lock(lock);
    d_.state.set(topics::kRotationLocked, lock != "off");
    toasts_.push({lock == "off" ? "Rotation unlocked" : "Rotation locked", "", false, 0, ""}, now_);
}

bool Shell::settings_notice_shown() const { return !lv_obj_has_flag(notice_row_, LV_OBJ_FLAG_HIDDEN); }

void Shell::refresh_settings_notice()
{
    Value v = d_.state.get(topics::kSettingsNotice);
    const std::string *s = std::get_if<std::string>(&v);
    const std::string text = s ? *s : std::string();
    if (reset_armed_ && static_cast<int32_t>(now_ - reset_armed_until_) >= 0) {
        reset_armed_ = false;
        lv_label_set_text(reset_label_, "Reset settings");
    }
    if (text == notice_text_) return;
    notice_text_ = text;
    if (text.empty()) {
        lv_obj_add_flag(notice_row_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(notice_label_, text.c_str());
    lv_obj_remove_flag(notice_row_, LV_OBJ_FLAG_HIDDEN);
    toasts_.push({text, "", true, 0, ""}, now_);  // the notice and its reset live in quick settings
}

void Shell::request_settings_reset()
{
    if (!reset_armed_) {
        reset_armed_ = true;
        reset_armed_until_ = now_ + StripVisibility::kRevealMs;
        lv_label_set_text(reset_label_, "Tap again to reset");
        return;
    }
    reset_armed_ = false;
    lv_label_set_text(reset_label_, "Reset settings");
    if (d_.settings.reset() == ResetResult::Ok) {
        d_.state.set(topics::kSettingsNotice, std::string());
        toasts_.push({"Settings reset. Old settings kept for export.", "", false, 0, ""}, now_);
    } else {
        toasts_.push({"Reset failed: old settings could not be saved", "", true, 0, ""}, now_);
    }
}

void Shell::tap_reveal()
{
    if (visibility_.reveal(now_)) refresh_strip_visibility();
}

void Shell::on_chip_tapped(size_t i)
{
    if (i >= chips_.size()) return;
    const GlanceChipDef &def = chips_[i].def;
    if (!def.panel_id.empty()) {
        go_to_panel(def.panel_id);
    } else if (def.id == "time") {
        go_to_panel(kHomePanel);
    } else if (def.id == "update") {
        // Settings -> Updates is built with the update engine (step 5); it listens for this.
        d_.bus.publish(topics::kOpenScreen, std::string("settings/updates"));
    } else if (def.id == "rotation_lock") {
        d_.settings.set("ui/rotation_lock", std::string("off"));
        rotation_.set_lock("off");
        d_.state.set(topics::kRotationLocked, false);
    } else {
        open_quick_settings();  // Wi-Fi, battery
    }
}

void Shell::chip_clicked_cb(lv_event_t *e)
{
    auto *self = static_cast<Shell *>(lv_event_get_user_data(e));
    // LVGL still sends a click after a gesture: a swipe that starts on a chip is not a tap.
    lv_indev_t *indev = lv_indev_active();
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) return;
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    self->on_chip_tapped(static_cast<size_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj))));
}

void Shell::strip_gesture_cb(lv_event_t *e)
{
    auto *self = static_cast<Shell *>(lv_event_get_user_data(e));
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_BOTTOM) self->open_quick_settings();
}

void Shell::strip_long_press_cb(lv_event_t *e)
{
    static_cast<Shell *>(lv_event_get_user_data(e))->toggle_rotation_lock();
}

void Shell::sheet_event_cb(lv_event_t *e)
{
    auto *self = static_cast<Shell *>(lv_event_get_user_data(e));
    lv_obj_t *t = lv_event_get_current_target_obj(e);
    const lv_event_code_t code = lv_event_get_code(e);
    if (t == self->backdrop_) {
        self->close_quick_settings();
    } else if (t == self->sheet_ && code == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_TOP) self->close_quick_settings();
    } else if (t == self->brightness_) {
        // While dragging the value goes out live (not saved); the finger lifting saves it once.
        if (code == LV_EVENT_VALUE_CHANGED) self->d_.bus.publish("ui/brightness_live", int64_t{lv_slider_get_value(t)});
        else self->d_.settings.set("display/brightness", int64_t{lv_slider_get_value(t)});
    } else if (t == self->volume_) {
        if (code == LV_EVENT_VALUE_CHANGED) self->d_.bus.publish("ui/volume_live", int64_t{lv_slider_get_value(t)});
        else self->d_.settings.set("audio/volume", int64_t{lv_slider_get_value(t)});
    } else if (t == self->rotation_buttons_) {
        uint32_t i = lv_buttonmatrix_get_selected_button(t);
        if (i < 3) {
            self->d_.settings.set("ui/rotation_lock", std::string(kRotationValues[i]));
            self->rotation_.set_lock(kRotationValues[i]);
        }
    } else if (t == self->modules_btn_ && t) {
        self->d_.bus.publish(topics::kOpenScreen, std::string("settings/modules"));
    } else if (t == self->updates_btn_) {
        self->d_.bus.publish(topics::kOpenScreen, std::string("settings/updates"));
    } else if (t == self->reset_btn_) {
        self->request_settings_reset();
    } else if (t == self->dnd_) {
        self->d_.settings.set("system/dnd", lv_obj_has_state(t, LV_STATE_CHECKED));
    }
}

void Shell::attach_input(lv_indev_t *indev)
{
    indev_ = indev;
    lv_indev_add_event_cb(indev, input_cb, LV_EVENT_PRESSED, this);
    lv_indev_add_event_cb(indev, input_cb, LV_EVENT_RELEASED, this);
}

void Shell::input_cb(lv_event_t *e) { static_cast<Shell *>(lv_event_get_user_data(e))->on_input(e); }

void Shell::on_input(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(indev_, &p);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        d_.bus.publish(topics::kActivity, true);  // any touch resets the idle-sleep timer
        press_ = last_ = p;
        last_ms_ = lv_tick_get();
        velocity_ = 0.0f;
        drag_horizontal_ = false;
        const Rect &c = layout_.content;
        const bool over_strip = strip_visible() && p.y < layout_.strip.h;
        // A panel can take horizontal swipes for itself (e.g. next step in a recipe).
        dragging_ = !state_bool(d_.state, topics::kSwipeLock) && !sheet_open_ && !updates_->is_open() && !(modules_ && modules_->is_open()) && !over_strip && p.y >= c.y && p.y < c.y + c.h;
        return;
    }
    // RELEASED
    const int32_t dx = p.x - press_.x, dy = p.y - press_.y;
    if (!strip_visible() && std::abs(dx) < layout_.tokens.space_m && std::abs(dy) < layout_.tokens.space_m) {
        tap_reveal();
    }
    if (dragging_ && drag_horizontal_) {
        int dir = Pager::swipe_direction(dx, layout_.content.w, velocity_);
        bool moved = (dir > 0 && next_page()) || (dir < 0 && prev_page());
        if (!moved) {  // snap back
            const int32_t x = lv_obj_get_x(page_);
            ui_motion::animate(page_, ui_motion::Prop::X, x, 0, motion::settle_ms(x, velocity_), motion::kEnter);
        }
    }
    dragging_ = false;
    drag_horizontal_ = false;
}

}  // namespace summit::shell
