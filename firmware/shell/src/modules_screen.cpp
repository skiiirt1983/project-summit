#include "summit/shell/modules_screen.h"

#include "summit/shell/shell.h"
#include "summit/shell/ui_motion.h"
#include "theme.h"

namespace summit::shell {

using namespace theme;

namespace {

lv_obj_t *plain(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_height(o, LV_SIZE_CONTENT);
    return o;
}

lv_obj_t *label(lv_obj_t *parent, const std::string &text, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text.c_str());
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    return l;
}

void set_text(lv_obj_t *l, const std::string &text)
{
    if (text != lv_label_get_text(l)) lv_label_set_text(l, text.c_str());
}

}  // namespace

ModuleEnv ModulesScreen::env() const
{
    ModuleEnv e;
    Value p = state_.get("storage/sd_present"), f = state_.get("storage/sd_free_mb");
    e.sd_present = std::get_if<bool>(&p) && std::get<bool>(p);
    e.sd_free_mb = std::get_if<int64_t>(&f) ? std::get<int64_t>(f) : 0;
    return e;
}

void ModulesScreen::build(lv_obj_t *parent)
{
    root_ = plain(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *header = plain(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    title_ = lv_label_create(header);
    lv_label_set_text(title_, "Modules");
    lv_obj_set_style_text_color(title_, lv_color_hex(kText), 0);
    close_btn_ = lv_button_create(header);
    lv_obj_set_style_bg_color(close_btn_, lv_color_hex(kChipBg), 0);
    lv_obj_set_style_shadow_width(close_btn_, 0, 0);
    ui_motion::press_feedback(close_btn_);
    lv_obj_t *x = lv_label_create(close_btn_);
    lv_label_set_text(x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(x, lv_color_hex(kText), 0);
    lv_obj_add_event_cb(close_btn_, event_cb, LV_EVENT_CLICKED, this);

    list_ = plain(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(list_, 1);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);

    for (const ModuleManifest &m : modules_.modules()) {
        Row r;
        r.id = m.id;
        r.card = plain(list_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r.card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(r.card, lv_color_hex(kStripBg), 0);
        lv_obj_set_style_bg_opa(r.card, LV_OPA_COVER, 0);
        lv_obj_t *text = plain(r.card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_grow(text, 1);
        r.name = label(text, m.icon.empty() ? m.name : m.icon + "  " + m.name, kText);
        r.desc = label(text, m.description, kMuted);
        r.status = label(text, "", kMuted);
        lv_obj_t *actions = plain(r.card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(actions, LV_SIZE_CONTENT);
        lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        r.button = lv_button_create(actions);
        lv_obj_set_style_shadow_width(r.button, 0, 0);
        ui_motion::press_feedback(r.button);
        r.button_label = lv_label_create(r.button);
        lv_label_set_text(r.button_label, "Install");
        lv_obj_add_event_cb(r.button, event_cb, LV_EVENT_CLICKED, this);
        // Separate from Remove: deleting the module's data needs its own confirmed step.
        r.del = lv_button_create(actions);
        lv_obj_set_style_shadow_width(r.del, 0, 0);
        ui_motion::press_feedback(r.del);
        lv_obj_set_style_bg_color(r.del, lv_color_hex(kChipBg), 0);
        r.del_label = lv_label_create(r.del);
        lv_label_set_text(r.del_label, "");
        lv_obj_set_style_text_color(r.del_label, lv_color_hex(kError), 0);
        lv_obj_add_event_cb(r.del, event_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_flag(r.del, LV_OBJ_FLAG_HIDDEN);
        rows_.push_back(r);
    }
    // Delete confirmation: modal over everything.
    confirm_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(confirm_);
    lv_obj_remove_flag(confirm_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(confirm_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(confirm_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(confirm_, LV_OPA_60, 0);
    confirm_card_ = plain(confirm_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(confirm_card_, lv_color_hex(kStripBg), 0);
    lv_obj_set_style_bg_opa(confirm_card_, LV_OPA_COVER, 0);
    confirm_label_ = label(confirm_card_, "", kText);
    confirm_buttons_ = plain(confirm_card_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(confirm_buttons_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto make = [&](const char *text, uint32_t bg, uint32_t fg) {
        lv_obj_t *b = lv_button_create(confirm_buttons_);
        lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        ui_motion::press_feedback(b);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
        lv_obj_add_event_cb(b, event_cb, LV_EVENT_CLICKED, this);
        return b;
    };
    cancel_btn_ = make("Cancel", kChipBg, kText);  // the safe choice comes first
    yes_btn_ = make("Delete", kError, kBg);
    lv_obj_add_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ModulesScreen::apply_layout(const Layout &l)
{
    const Tokens &t = l.tokens;
    lv_obj_set_pos(root_, l.content.x, l.content.y);
    lv_obj_set_size(root_, l.content.w, l.content.h);
    lv_obj_set_style_pad_all(root_, t.space_l, 0);
    lv_obj_set_style_pad_row(root_, t.space_m, 0);
    lv_obj_set_style_text_font(root_, font_for(t.text_m), 0);
    lv_obj_set_style_text_font(title_, font_for(t.text_l), 0);
    lv_obj_set_style_pad_row(list_, t.space_m, 0);
    lv_obj_set_style_pad_hor(close_btn_, t.space_m, 0);
    lv_obj_set_style_pad_ver(close_btn_, t.space_s, 0);
    lv_obj_set_style_radius(close_btn_, t.radius, 0);
    for (Row &r : rows_) {
        lv_obj_set_style_pad_all(r.card, t.space_m, 0);
        lv_obj_set_style_pad_column(r.card, t.space_m, 0);
        lv_obj_set_style_radius(r.card, t.radius, 0);
        lv_obj_set_style_pad_row(lv_obj_get_parent(r.name), t.space_s / 2, 0);
        lv_obj_set_style_text_font(r.desc, font_for(t.text_s), 0);
        lv_obj_set_style_text_font(r.status, font_for(t.text_s), 0);
        lv_obj_set_style_pad_hor(r.button, t.space_m, 0);
        lv_obj_set_style_pad_ver(r.button, t.space_s, 0);
        lv_obj_set_style_radius(r.button, t.radius, 0);
        lv_obj_set_style_pad_hor(r.del, t.space_m, 0);
        lv_obj_set_style_pad_ver(r.del, t.space_s, 0);
        lv_obj_set_style_radius(r.del, t.radius, 0);
        lv_obj_set_style_text_font(r.del_label, font_for(t.text_s), 0);
        lv_obj_set_style_pad_row(lv_obj_get_parent(r.button), t.space_s, 0);
    }
    lv_display_t *disp = lv_display_get_default();
    lv_obj_set_size(confirm_, lv_display_get_horizontal_resolution(disp), lv_display_get_vertical_resolution(disp));
    lv_obj_set_width(confirm_card_, l.orientation == kLandscape ? l.content.w / 2 : l.content.w - 2 * t.space_l);
    lv_obj_set_style_pad_all(confirm_card_, t.space_l, 0);
    lv_obj_set_style_pad_row(confirm_card_, t.space_l, 0);
    lv_obj_set_style_radius(confirm_card_, t.radius, 0);
    lv_obj_set_style_text_font(confirm_card_, font_for(t.text_m), 0);
    lv_obj_set_style_pad_column(confirm_buttons_, t.space_m, 0);
    for (lv_obj_t *b : {yes_btn_, cancel_btn_}) {
        lv_obj_set_style_pad_hor(b, t.space_m, 0);
        lv_obj_set_style_pad_ver(b, t.space_s, 0);
        lv_obj_set_style_radius(b, t.radius, 0);
    }
    lv_obj_center(confirm_card_);
}

void ModulesScreen::open()
{
    const bool was_open = open_;
    open_ = true;
    for (Row &r : rows_) r.message.clear();
    lv_obj_move_foreground(root_);
    if (!was_open) ui_motion::push_in(root_, lv_obj_get_width(root_));  // in from the right
    tick();
}

void ModulesScreen::close()
{
    open_ = false;
    ui_motion::pull_out(root_, lv_obj_get_width(root_));
    lv_obj_add_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    confirm_id_.clear();
}

void ModulesScreen::tick()
{
    if (!open_) return;
    const ModuleEnv e = env();
    for (Row &r : rows_) {
        const ModuleManifest *m = modules_.find(r.id);
        const bool installed = modules_.installed(settings_, r.id);
        std::string status;
        bool problem = false;
        if (installed) {
            status = "Installed";
            if (m->requires_.sd_card && !e.sd_present) {
                status += ". Insert the microSD card to use it";
                problem = true;
            }
        } else if (!r.message.empty()) {
            // Keep showing why the last Install was refused until the cause is fixed.
            std::vector<std::string> missing = missing_requirements(m->requires_, e);
            if (missing.empty()) r.message.clear();
            else r.message = missing.front();
            status = r.message.empty() ? requirements_text(m->requires_) : r.message;
            problem = !r.message.empty();
        } else {
            status = requirements_text(m->requires_);
        }
        set_text(r.status, status);
        lv_obj_set_style_text_color(r.status, lv_color_hex(problem ? kError : kMuted), 0);
        // "Delete recipes" whenever there is data, installed or not.
        ModuleData *data = modules_.data(r.id);
        const int n = data ? data->count() : 0;
        if (n > 0) {
            set_text(r.del_label, "Delete " + std::to_string(n) + " " + data->noun(n));
            lv_obj_remove_flag(r.del, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(r.del, LV_OBJ_FLAG_HIDDEN);
        }
        set_text(r.button_label, installed ? "Remove" : "Install");
        lv_obj_set_style_bg_color(r.button, lv_color_hex(installed ? kChipBg : kAccent), 0);
        lv_obj_set_style_text_color(r.button_label, lv_color_hex(installed ? kText : kBg), 0);
    }
}

const ModulesScreen::Row *ModulesScreen::row(const std::string &id) const
{
    for (const Row &r : rows_)
        if (r.id == id) return &r;
    return nullptr;
}

lv_obj_t *ModulesScreen::delete_button(const std::string &id) const { return row(id) ? row(id)->del : nullptr; }
std::string ModulesScreen::delete_text(const std::string &id) const
{
    const Row *r = row(id);
    return r && !lv_obj_has_flag(r->del, LV_OBJ_FLAG_HIDDEN) ? lv_label_get_text(r->del_label) : "";
}
bool ModulesScreen::confirm_shown() const { return !lv_obj_has_flag(confirm_, LV_OBJ_FLAG_HIDDEN); }
std::string ModulesScreen::confirm_text() const { return lv_label_get_text(confirm_label_); }

lv_obj_t *ModulesScreen::action_button(const std::string &id) const { return row(id) ? row(id)->button : nullptr; }
std::string ModulesScreen::action_text(const std::string &id) const
{
    return row(id) ? lv_label_get_text(row(id)->button_label) : "";
}
std::string ModulesScreen::status_text(const std::string &id) const
{
    return row(id) ? lv_label_get_text(row(id)->status) : "";
}

void ModulesScreen::event_cb(lv_event_t *e)
{
    auto *self = static_cast<ModulesScreen *>(lv_event_get_user_data(e));
    lv_obj_t *t = lv_event_get_current_target_obj(e);
    if (t == self->close_btn_) {
        self->close();
        return;
    }
    if (t == self->cancel_btn_ || t == self->yes_btn_) {
        ModuleData *data = self->modules_.data(self->confirm_id_);
        lv_obj_add_flag(self->confirm_, LV_OBJ_FLAG_HIDDEN);
        if (t == self->yes_btn_ && data) {
            int deleted = 0;
            const bool ok = data->delete_all(&deleted);
            self->bus_.publish("notify/toast", ok ? "Deleted " + std::to_string(deleted) + " " + data->noun(deleted)
                                                  : std::string("Some files could not be deleted"));
        }
        self->confirm_id_.clear();
        self->tick();
        return;
    }
    for (Row &r : self->rows_) {
        if (t == r.del) {
            ModuleData *data = self->modules_.data(r.id);
            if (!data || data->count() == 0) return;
            self->confirm_id_ = r.id;
            lv_label_set_text(self->confirm_label_, delete_data_question(*data).c_str());
            lv_obj_remove_flag(self->confirm_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(self->confirm_);
            return;
        }
    }
    for (Row &r : self->rows_) {
        if (t != r.button) continue;
        if (self->modules_.installed(self->settings_, r.id)) {
            // Remove only hides the module; its data on the SD card is kept.
            self->modules_.remove(self->settings_, r.id);
            self->bus_.publish("notify/toast", std::string("Module removed. Its data was kept."));
        } else {
            std::vector<std::string> missing;
            ModuleResult res = self->modules_.install(self->settings_, r.id, self->env(), &missing);
            if (res == ModuleResult::MissingRequirements) r.message = missing.front();
            else if (res == ModuleResult::Ok) self->bus_.publish("notify/toast", std::string("Module installed"));
            else r.message = "Could not save the setting";
        }
        self->tick();
    }
}

}  // namespace summit::shell
