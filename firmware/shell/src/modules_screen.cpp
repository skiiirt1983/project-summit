#include "summit/shell/modules_screen.h"

#include "summit/shell/shell.h"
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
        r.button = lv_button_create(r.card);
        lv_obj_set_style_shadow_width(r.button, 0, 0);
        r.button_label = lv_label_create(r.button);
        lv_label_set_text(r.button_label, "Install");
        lv_obj_add_event_cb(r.button, event_cb, LV_EVENT_CLICKED, this);
        rows_.push_back(r);
    }
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
    }
}

void ModulesScreen::open()
{
    open_ = true;
    for (Row &r : rows_) r.message.clear();
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
    tick();
}

void ModulesScreen::close()
{
    open_ = false;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
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
