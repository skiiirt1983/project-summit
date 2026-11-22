#include "summit/shell/updates_screen.h"

#include "summit/shell/shell.h"
#include "summit/shell/ui_motion.h"
#include "theme.h"

namespace summit::shell {

using namespace theme;

namespace {

const char *kChannelMap[] = {"Stable", "Beta", "Alpha", ""};
const char *kChannelValues[] = {"stable", "beta", "alpha"};

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

lv_obj_t *label(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, "");
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

lv_obj_t *button(lv_obj_t *parent, const char *text, bool primary)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_style_bg_color(b, lv_color_hex(primary ? kAccent : kChipBg), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(primary ? kBg : kText), 0);
    ui_motion::press_feedback(b);
    return b;
}

void show(lv_obj_t *o, bool visible) { ui_motion::set_hidden(o, !visible); }

void set_text(lv_obj_t *l, const std::string &text)
{
    if (text != lv_label_get_text(l)) lv_label_set_text(l, text.c_str());
}

}  // namespace

std::string UpdatesScreen::str(const char *key) const
{
    Value v = state_.get(key);
    const std::string *s = std::get_if<std::string>(&v);
    return s ? *s : std::string();
}

void UpdatesScreen::build(lv_obj_t *parent)
{
    root_ = plain(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);  // swallow touches meant for the page below

    lv_obj_t *header = plain(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    title_ = label(header, kText);
    lv_label_set_text(title_, "Updates");
    close_btn_ = button(header, LV_SYMBOL_CLOSE, false);

    info_ = plain(root_, LV_FLEX_FLOW_COLUMN);
    installed_ = label(info_, kText);
    hw_ = label(info_, kMuted);

    channel_row_ = plain(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(channel_row_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *cl = label(channel_row_, kMuted);
    lv_label_set_text(cl, "Channel");
    channel_ = lv_buttonmatrix_create(channel_row_);
    lv_buttonmatrix_set_map(channel_, kChannelMap);
    lv_buttonmatrix_set_button_ctrl_all(channel_, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(channel_, true);
    lv_obj_set_style_bg_color(channel_, lv_color_hex(kChipBg), LV_PART_MAIN);
    lv_obj_set_style_border_width(channel_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(channel_, lv_color_hex(kChipBg), LV_PART_ITEMS);
    lv_obj_set_style_text_color(channel_, lv_color_hex(kText), LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(channel_, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(channel_, lv_color_hex(kAccent), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(channel_, lv_color_hex(kBg), LV_PART_ITEMS | LV_STATE_CHECKED);

    status_ = label(root_, kText);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(status_, lv_pct(100));

    available_ = plain(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(available_, lv_color_hex(kStripBg), 0);
    lv_obj_set_style_bg_opa(available_, LV_OPA_COVER, 0);
    available_title_ = label(available_, kText);
    notes_ = label(available_, kMuted);
    lv_label_set_long_mode(notes_, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(notes_, lv_pct(100));

    buttons_ = plain(root_, LV_FLEX_FLOW_ROW_WRAP);
    check_btn_ = button(buttons_, "Check now", false);
    install_btn_ = button(buttons_, "Install", true);
    restart_btn_ = button(buttons_, "Restart now", true);
    previous_btn_ = button(buttons_, "Previous version", false);

    progress_ = plain(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(progress_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(progress_, 1);
    progress_title_ = label(progress_, kText);
    bar_ = lv_bar_create(progress_);
    lv_bar_set_range(bar_, 0, 100);
    lv_obj_set_width(bar_, lv_pct(80));
    lv_obj_set_style_bg_color(bar_, lv_color_hex(kChipBg), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(kAccent), LV_PART_INDICATOR);
    pct_ = label(progress_, kText);
    keep_ = label(progress_, kMuted);
    lv_label_set_text(keep_, "Keep the device powered. It restarts when the update is ready.");

    // "Previous version" confirmation: modal over everything (top layer).
    confirm_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(confirm_);
    lv_obj_remove_flag(confirm_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(confirm_, LV_OBJ_FLAG_CLICKABLE);  // modal: swallow touches
    lv_obj_set_style_bg_color(confirm_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(confirm_, LV_OPA_60, 0);
    confirm_card_ = plain(confirm_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(confirm_card_, lv_color_hex(kStripBg), 0);
    lv_obj_set_style_bg_opa(confirm_card_, LV_OPA_COVER, 0);
    lv_obj_center(confirm_card_);
    confirm_label_ = label(confirm_card_, kText);
    lv_label_set_long_mode(confirm_label_, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(confirm_label_, lv_pct(100));
    confirm_buttons_ = plain(confirm_card_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(confirm_buttons_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    cancel_btn_ = button(confirm_buttons_, "Cancel", false);
    yes_btn_ = button(confirm_buttons_, "Yes", true);
    show(confirm_, false);

    for (lv_obj_t *o : {close_btn_, check_btn_, install_btn_, restart_btn_, previous_btn_, yes_btn_, cancel_btn_})
        lv_obj_add_event_cb(o, event_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(channel_, event_cb, LV_EVENT_VALUE_CHANGED, this);
    show(root_, false);
}

void UpdatesScreen::apply_layout(const Layout &l)
{
    layout_ = l;
    const Tokens &t = l.tokens;
    lv_obj_set_pos(root_, l.content.x, l.content.y);
    lv_obj_set_size(root_, l.content.w, l.content.h);
    lv_obj_set_style_pad_all(root_, t.space_l, 0);
    lv_obj_set_style_pad_row(root_, t.space_m, 0);
    lv_obj_set_style_text_font(root_, font_for(t.text_m), 0);
    lv_obj_set_style_text_font(title_, font_for(t.text_l), 0);
    lv_obj_set_style_text_font(hw_, font_for(t.text_s), 0);
    lv_obj_set_style_text_font(notes_, font_for(t.text_s), 0);
    lv_obj_set_style_text_font(progress_title_, font_for(t.text_l), 0);
    lv_obj_set_style_text_font(pct_, font_for(t.text_xl), 0);
    lv_obj_set_style_pad_all(available_, t.space_m, 0);
    lv_obj_set_style_pad_row(available_, t.space_s, 0);
    lv_obj_set_style_radius(available_, t.radius, 0);
    lv_obj_set_style_pad_gap(buttons_, t.space_m, 0);
    lv_obj_set_style_pad_row(progress_, t.space_m, 0);
    lv_obj_set_height(bar_, t.space_m);
    lv_obj_set_style_radius(bar_, t.space_s, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_, t.space_s, LV_PART_INDICATOR);
    const int16_t channel_w = l.orientation == kLandscape ? l.content.w / 2 : l.content.w * 2 / 3;
    lv_obj_set_size(channel_, channel_w, t.unit);
    lv_obj_set_style_radius(channel_, t.radius, LV_PART_MAIN);
    lv_obj_set_style_radius(channel_, t.radius, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(channel_, t.space_s / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(channel_, t.space_s / 2, LV_PART_MAIN);
    lv_display_t *disp = lv_display_get_default();
    lv_obj_set_size(confirm_, lv_display_get_horizontal_resolution(disp), lv_display_get_vertical_resolution(disp));
    lv_obj_set_width(confirm_card_, l.orientation == kLandscape ? l.content.w / 2 : l.content.w - 2 * t.space_l);
    lv_obj_set_style_pad_all(confirm_card_, t.space_l, 0);
    lv_obj_set_style_pad_row(confirm_card_, t.space_l, 0);
    lv_obj_set_style_radius(confirm_card_, t.radius, 0);
    lv_obj_set_style_text_font(confirm_card_, font_for(t.text_m), 0);
    lv_obj_set_style_pad_column(confirm_buttons_, t.space_m, 0);
    lv_obj_center(confirm_card_);
    for (lv_obj_t *b : {close_btn_, check_btn_, install_btn_, restart_btn_, previous_btn_, yes_btn_, cancel_btn_}) {
        lv_obj_set_style_pad_hor(b, t.space_m, 0);
        lv_obj_set_style_pad_ver(b, t.space_s, 0);
        lv_obj_set_style_radius(b, t.radius, 0);
    }
}

void UpdatesScreen::open()
{
    const bool was_open = open_;
    open_ = true;
    lv_obj_move_foreground(root_);
    if (!was_open) ui_motion::push_in(root_, lv_obj_get_width(root_));  // in from the right, like a phone's settings
    else show(root_, true);
    tick();
}

void UpdatesScreen::close()
{
    open_ = false;
    ui_motion::pull_out(root_, lv_obj_get_width(root_));
    show(confirm_, false);
}

bool UpdatesScreen::confirm_shown() const { return !lv_obj_has_flag(confirm_, LV_OBJ_FLAG_HIDDEN); }

std::string UpdatesScreen::confirm_text() const { return lv_label_get_text(confirm_label_); }

bool UpdatesScreen::showing_progress() const { return open_ && !lv_obj_has_flag(progress_, LV_OBJ_FLAG_HIDDEN); }

std::string UpdatesScreen::status_text() const { return lv_label_get_text(status_); }

void UpdatesScreen::tick()
{
    if (!open_) return;
    const std::string stage = str("update/stage");
    Value ip = state_.get("update/in_progress");
    const bool installing = std::get_if<bool>(&ip) && std::get<bool>(ip);
    Value pv = state_.get("update/progress_pct");
    const int64_t pct = std::get_if<int64_t>(&pv) ? std::get<int64_t>(pv) : 0;

    // Full-screen progress while installing (the shell hides the strip via update/in_progress).
    for (lv_obj_t *o : {info_, channel_row_, status_, available_, buttons_}) show(o, !installing);
    show(progress_, installing);
    show(close_btn_, !installing);
    if (installing) {
        set_text(progress_title_, "Installing " + str("update/target_version"));
        if (lv_bar_get_value(bar_) != static_cast<int32_t>(pct)) lv_bar_set_value(bar_, static_cast<int32_t>(pct), LV_ANIM_OFF);
        set_text(pct_, std::to_string(pct) + "%");
        return;
    }

    set_text(installed_, "Installed: " + str("system/firmware_version"));
    set_text(hw_, str("system/hardware_id"));
    Value ch = settings_.get("update/channel");
    const std::string *chs = std::get_if<std::string>(&ch);
    for (uint32_t i = 0; i < 3; ++i) {
        const bool want = chs && *chs == kChannelValues[i];
        if (want == lv_buttonmatrix_has_button_ctrl(channel_, i, LV_BUTTONMATRIX_CTRL_CHECKED)) continue;  // no change, no redraw
        if (want) lv_buttonmatrix_set_button_ctrl(channel_, i, LV_BUTTONMATRIX_CTRL_CHECKED);
        else lv_buttonmatrix_clear_button_ctrl(channel_, i, LV_BUTTONMATRIX_CTRL_CHECKED);
    }

    const std::string available = str("update/available_version");
    const std::string error = str("update/error");
    std::string status;
    if (stage == "checking") status = "Checking for updates...";
    else if (stage == "up_to_date") status = "Up to date.";
    else if (stage == "available") status = "An update is available.";
    else if (stage == "check_failed") status = "Could not check for updates: " + error;
    else if (stage == "failed") status = "Update failed: " + error;
    else if (stage == "ready_to_reboot") status = "Update " + str("update/target_version") + " is ready. Restart to finish.";
    else status = "Check for updates to see what's new.";
    set_text(status_, status);
    const lv_color_t status_color = lv_color_hex(stage == "failed" || stage == "check_failed" ? kError : kText);
    if (!lv_color_eq(lv_obj_get_style_text_color(status_, LV_PART_MAIN), status_color))
        lv_obj_set_style_text_color(status_, status_color, 0);

    show(available_, !available.empty() && stage != "ready_to_reboot");
    set_text(available_title_, "Version " + available);
    set_text(notes_, str("update/notes"));
    show(install_btn_, !available.empty() && (stage == "available" || stage == "failed"));
    set_text(lv_obj_get_child(install_btn_, 0), "Install " + available);
    show(restart_btn_, stage == "ready_to_reboot");
    show(check_btn_, stage != "checking" && stage != "ready_to_reboot");
    const std::string previous = str("update/previous_version");
    show(previous_btn_, !previous.empty() && stage != "ready_to_reboot");
    set_text(lv_obj_get_child(previous_btn_, 0), "Previous version (" + previous + ")");
}

void UpdatesScreen::event_cb(lv_event_t *e)
{
    auto *self = static_cast<UpdatesScreen *>(lv_event_get_user_data(e));
    lv_obj_t *t = lv_event_get_current_target_obj(e);
    if (t == self->close_btn_) {
        self->close();
    } else if (t == self->check_btn_) {
        self->bus_.publish("update/command", std::string("check"));
    } else if (t == self->install_btn_) {
        self->bus_.publish("update/command", "install:" + self->str("update/available_version"));
    } else if (t == self->restart_btn_) {
        self->bus_.publish("update/command", std::string("reboot"));
    } else if (t == self->previous_btn_) {
        // Confirm first: going back can leave newer settings read-only.
        lv_label_set_text(self->confirm_label_, ("Go back to v" + self->str("update/previous_version") +
                                                 "? Settings from the newer version may become read-only.")
                                                    .c_str());
        show(self->confirm_, true);
        lv_obj_move_foreground(self->confirm_);
    } else if (t == self->yes_btn_) {
        show(self->confirm_, false);
        self->bus_.publish("update/command", std::string("previous"));
    } else if (t == self->cancel_btn_) {
        show(self->confirm_, false);
    } else if (t == self->channel_) {
        uint32_t i = lv_buttonmatrix_get_selected_button(t);
        // The update service re-checks when update/channel changes (also from the web UI).
        if (i < 3) self->settings_.set("update/channel", std::string(kChannelValues[i]));
    }
}

}  // namespace summit::shell
