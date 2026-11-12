#include "summit/recipes/panel.h"

#include "summit/recipes/module.h"
#include "summit/shell/shell.h"

namespace summit::recipes {

namespace {

constexpr uint32_t kBg = 0x0F1317, kCard = 0x1A2027, kChip = 0x262E37, kText = 0xE6EDF3, kMuted = 0x9AA7B4,
                   kAccent = 0x4EA1FF;

lv_obj_t *box(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_height(o, LV_SIZE_CONTENT);
    return o;
}

lv_obj_t *label(lv_obj_t *parent, const std::string &text, int16_t px, uint32_t color = kText)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text.c_str());
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_font(l, shell::font_for(px), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

void card_style(lv_obj_t *o, const SizeTokens &t)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(kCard), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, t.radius, 0);
    lv_obj_set_style_pad_all(o, t.space_m, 0);
    lv_obj_set_style_pad_row(o, t.space_s, 0);
}

void scroll_y(lv_obj_t *o)
{
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_VER);
}

intptr_t pack(int action, int index) { return (static_cast<intptr_t>(action) << 16) | (index & 0xFFFF); }

}  // namespace

RecipesPanel::RecipesPanel(RecipeStore &store, ImageSource image_source)
    : store_(store), image_source_(std::move(image_source))
{
    manifest_.id = kPanelId;
    manifest_.name = "Recipes";
    manifest_.version = "0.1.0";
    manifest_.sizes = kSizeFull;
    manifest_.subscriptions = {"storage/sd_present", "input/double_tap"};
}

bool RecipesPanel::create(PanelContext &) { return true; }
void RecipesPanel::destroy(PanelContext &) {}

void RecipesPanel::show(PanelContext &ctx, const PanelView &view)
{
    ctx_ = &ctx;
    pv_ = view;
    shown_ = true;
    root_ = box(view.lv_parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_GESTURE_BUBBLE);  // swipes inside a recipe stop here
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root_, gesture_cb, LV_EVENT_GESTURE, this);
    render();
}

void RecipesPanel::hide(PanelContext &ctx)
{
    shown_ = false;
    if (root_) lv_obj_delete(root_);  // hidden panels hold no LVGL objects
    root_ = grid_ = search_box_ = keyboard_ = page_area_ = page_img_ = nullptr;
    // Back to normal while another page is showing; the open recipe is kept.
    ctx.set_state("ui/swipe_lock", false);
    ctx.set_state("ui/keep_awake", false);
}

void RecipesPanel::tick(PanelContext &, uint32_t)
{
    if (dirty_ && shown_) {
        dirty_ = false;
        render();
    }
}

void RecipesPanel::on_event(PanelContext &, const std::string &topic, const Value &)
{
    if (topic == "storage/sd_present") {
        dirty_ = true;  // card inserted or removed
    } else if (topic == "input/double_tap" && (view_ == View::Card || view_ == View::Book)) {
        act(kNext, 0);  // double-tap the case: next step / next page
    }
}

void RecipesPanel::sync_flags()
{
    if (!ctx_ || !shown_) return;
    const bool cooking = view_ == View::Card || view_ == View::Book;
    ctx_->set_state("ui/swipe_lock", cooking);
    ctx_->set_state("ui/keep_awake", cooking);  // screen stays on while a recipe is open
}

bool RecipesPanel::open_recipe(const std::string &id)
{
    Recipe r;
    if (!store_.load(id, r)) return false;
    int pages = 0;
    for (const IndexEntry &e : index_)
        if (e.id == id) pages = e.pages;
    session_ = CookSession(r, pages);
    open_id_ = id;
    view_ = session_.card() ? View::Card : View::Book;  // pages only: straight to book mode
    zoomed_ = false;
    if (ctx_) {
        ctx_->set_state("recipes/cooking", r.title);
        Value now = ctx_->state("time/unix");
        if (const int64_t *t = std::get_if<int64_t>(&now)) store_.mark_cooked(id, *t);
    }
    dirty_ = true;
    return true;
}

void RecipesPanel::back_to_library()
{
    view_ = View::Library;
    open_id_.clear();
    session_ = CookSession();
    if (ctx_) ctx_->set_state("recipes/cooking", std::string());
    dirty_ = true;
}

void RecipesPanel::set_search(const std::string &text)
{
    search_ = text;
    if (shown_ && view_ == View::Library && grid_) render_grid();
}

void RecipesPanel::set_tag(const std::string &tag)
{
    tag_ = tag;
    dirty_ = true;
}

std::vector<std::string> RecipesPanel::visible_ids() const
{
    std::vector<std::string> ids;
    for (const IndexEntry &e : filtered_) ids.push_back(e.id);
    return ids;
}

lv_obj_t *RecipesPanel::button(lv_obj_t *parent, const std::string &text, int action, int index, bool primary)
{
    const SizeTokens &t = pv_.tokens;
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_style_bg_color(b, lv_color_hex(primary ? kAccent : kChip), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_radius(b, t.radius, 0);
    lv_obj_set_style_pad_hor(b, t.space_m, 0);
    lv_obj_set_style_pad_ver(b, t.space_s, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text.c_str());
    lv_obj_set_style_text_font(l, shell::font_for(t.text_m), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(primary ? kBg : kText), 0);
    lv_obj_set_user_data(b, reinterpret_cast<void *>(pack(action, index)));
    lv_obj_add_event_cb(b, event_cb, LV_EVENT_CLICKED, this);
    return b;
}

lv_obj_t *RecipesPanel::find(int action, int index) const
{
    if (!root_) return nullptr;
    std::vector<lv_obj_t *> stack{root_};
    while (!stack.empty()) {
        lv_obj_t *o = stack.back();
        stack.pop_back();
        if (reinterpret_cast<intptr_t>(lv_obj_get_user_data(o)) == pack(action, index)) return o;
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) stack.push_back(lv_obj_get_child(o, i));
    }
    return nullptr;
}

void RecipesPanel::render()
{
    if (!root_) return;
    lv_obj_clean(root_);
    grid_ = search_box_ = keyboard_ = page_area_ = page_img_ = nullptr;
    const SizeTokens &t = pv_.tokens;
    lv_obj_set_style_pad_all(root_, t.space_l, 0);
    lv_obj_set_style_pad_row(root_, t.space_m, 0);
    if (!store_.available()) {
        view_ = open_id_.empty() ? View::NoCard : view_;
        render_message("Insert the SD card with your recipes", false);
    } else {
        if (view_ == View::NoCard) view_ = View::Library;
        if (view_ == View::Library) render_library();
        else if (view_ == View::Card) render_card();
        else render_book();
    }
    sync_flags();
}

void RecipesPanel::render_message(const std::string &text, bool with_back)
{
    const SizeTokens &t = pv_.tokens;
    if (with_back) button(root_, std::string(LV_SYMBOL_LEFT) + "  Recipes", kBack, 0);
    label(root_, text, t.text_m, kMuted);
}

void RecipesPanel::render_library()
{
    const SizeTokens &t = pv_.tokens;
    std::vector<std::string> skipped;
    store_.load_index(index_, &skipped);
    tags_ = all_tags(index_);

    lv_obj_t *header = box(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = label(header, "Recipes", t.text_l, kMuted);
    lv_obj_set_width(title, LV_SIZE_CONTENT);
    search_box_ = lv_textarea_create(header);
    lv_textarea_set_one_line(search_box_, true);
    lv_textarea_set_placeholder_text(search_box_, "Search");
    lv_textarea_set_text(search_box_, search_.c_str());
    lv_obj_set_width(search_box_, lv_pct(pv_.orientation == kLandscape ? 35 : 55));
    lv_obj_set_style_text_font(search_box_, shell::font_for(t.text_m), 0);
    lv_obj_set_style_bg_color(search_box_, lv_color_hex(kChip), 0);
    lv_obj_set_style_text_color(search_box_, lv_color_hex(kText), 0);
    lv_obj_set_style_border_width(search_box_, 0, 0);
    lv_obj_set_style_radius(search_box_, t.radius, 0);
    lv_obj_add_event_cb(search_box_, search_cb, LV_EVENT_ALL, this);

    if (!tags_.empty()) {
        lv_obj_t *row = box(root_, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_gap(row, t.space_s, 0);
        button(row, "All", kTag, 0, tag_.empty());
        for (size_t i = 0; i < tags_.size(); ++i) button(row, tags_[i], kTag, static_cast<int>(i + 1), tags_[i] == tag_);
    }

    const size_t inbox = store_.inbox().size();
    if (inbox > 0) {
        lv_obj_t *note = box(root_, LV_FLEX_FLOW_ROW);
        card_style(note, t);
        lv_obj_set_style_pad_column(note, t.space_m, 0);
        lv_obj_set_flex_align(note, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        Value ip = ctx_ ? ctx_->state("net/ip") : Value{};
        const std::string *ips = std::get_if<std::string>(&ip);
        const std::string url = ips && !ips->empty() ? "http://" + *ips + "/" : "";
#if LV_USE_QRCODE
        if (!url.empty()) {
            lv_obj_t *qr = lv_qrcode_create(note);
            lv_qrcode_set_size(qr, t.unit * 2);
            lv_qrcode_set_dark_color(qr, lv_color_black());
            lv_qrcode_set_light_color(qr, lv_color_white());
            lv_qrcode_update(qr, url.c_str(), static_cast<uint32_t>(url.size()));
        }
#endif
        lv_obj_t *text = box(note, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_grow(text, 1);
        label(text, std::to_string(inbox) + (inbox == 1 ? " new PDF" : " new PDFs") + " - open the web UI to convert",
              t.text_m);
        if (!url.empty()) label(text, url, t.text_s, kMuted);
    }

    grid_ = box(root_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_grow(grid_, 1);
    lv_obj_set_style_pad_gap(grid_, t.space_m, 0);
    scroll_y(grid_);
    render_grid();
}

void RecipesPanel::render_grid()
{
    const SizeTokens &t = pv_.tokens;
    lv_obj_clean(grid_);
    filtered_ = filter_library(index_, search_, tag_);
    if (filtered_.empty()) {
        label(grid_, index_.empty() ? "No recipes yet. Add some from the web UI." : "No recipes match.", t.text_m, kMuted);
        return;
    }
    const int cols = pv_.orientation == kLandscape ? 4 : 2;
    for (size_t i = 0; i < filtered_.size(); ++i) {
        const IndexEntry &e = filtered_[i];
        lv_obj_t *c = box(grid_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(c, lv_pct((100 - 3 * (cols - 1)) / cols));
        card_style(c, t);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(c, reinterpret_cast<void *>(pack(kOpen, static_cast<int>(i))));
        lv_obj_add_event_cb(c, event_cb, LV_EVENT_CLICKED, this);
        label(c, e.title, t.text_m);
        std::string info = e.has_card ? "Steps" : "";
        if (e.has_pages) info += (info.empty() ? "" : " + ") + std::to_string(e.pages) + (e.pages == 1 ? " page" : " pages");
        for (const auto &tag : e.tags) info += "  #" + tag;
        label(c, info, t.text_s, kMuted);
    }
}

void RecipesPanel::render_card()
{
    const SizeTokens &t = pv_.tokens;
    const bool landscape = pv_.orientation == kLandscape;

    lv_obj_t *header = box(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, t.space_m, 0);
    button(header, LV_SYMBOL_LEFT, kBack, 0);
    lv_obj_t *title = label(header, session_.recipe().title, t.text_l);
    lv_obj_set_flex_grow(title, 1);
    if (session_.book()) button(header, "Pages", kPages, 0);

    lv_obj_t *body = box(root_, landscape ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_gap(body, t.space_m, 0);

    // Ingredients: left in landscape, above the step in portrait.
    lv_obj_t *ing = box(body, LV_FLEX_FLOW_COLUMN);
    card_style(ing, t);
    scroll_y(ing);
    if (landscape) {
        lv_obj_set_size(ing, lv_pct(38), lv_pct(100));
    } else {
        lv_obj_set_size(ing, lv_pct(100), lv_pct(40));
    }
    if (session_.can_scale()) {
        lv_obj_t *row = box(ing, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, t.space_m, 0);
        lv_obj_t *s = label(row, "Serves " + std::to_string(session_.servings()), t.text_m, kMuted);
        lv_obj_set_flex_grow(s, 1);
        button(row, LV_SYMBOL_MINUS, kMinus, 0);
        button(row, LV_SYMBOL_PLUS, kPlus, 0);
    }
    for (size_t i = 0; i < session_.recipe().ingredients.size(); ++i) {
        const bool done = session_.ticked(i);
        lv_obj_t *l = label(ing, (done ? std::string(LV_SYMBOL_OK) + "  " : "") + session_.ingredient_text(i), t.text_m,
                            done ? kMuted : kText);
        lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);  // tap to tick off
        lv_obj_set_style_pad_ver(l, t.space_s / 2, 0);
        lv_obj_set_user_data(l, reinterpret_cast<void *>(pack(kTick, static_cast<int>(i))));
        lv_obj_add_event_cb(l, event_cb, LV_EVENT_CLICKED, this);
    }

    // One big step at a time.
    lv_obj_t *step = box(body, LV_FLEX_FLOW_COLUMN);
    card_style(step, t);
    lv_obj_set_flex_grow(step, 1);
    if (landscape) lv_obj_set_height(step, lv_pct(100));
    else lv_obj_set_width(step, lv_pct(100));
    label(step, session_.step_label(), t.text_s, kMuted);
    lv_obj_t *text_area = box(step, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(text_area, 1);
    scroll_y(text_area);
    label(text_area, session_.step_text(), t.text_l);
    const std::vector<TimeSpan> times = session_.step_times();
    if (!times.empty()) {
        lv_obj_t *row = box(step, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_gap(row, t.space_s, 0);
        for (size_t i = 0; i < times.size(); ++i)
            button(row, std::string(LV_SYMBOL_BELL) + "  Timer " + format_duration(times[i].seconds), kTimer,
                   static_cast<int>(i));
    }
    lv_obj_t *nav = box(step, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *prev = button(nav, std::string(LV_SYMBOL_LEFT) + "  Back", kPrev, 0);
    lv_obj_t *next = button(nav, "Next  " LV_SYMBOL_RIGHT, kNext, 0, true);
    for (lv_obj_t *b : {prev, next}) lv_obj_set_style_pad_ver(b, t.space_m, 0);  // big targets for messy hands
    if (session_.step() == 0) lv_obj_add_state(prev, LV_STATE_DISABLED);
    if (session_.step() + 1 >= session_.step_count()) lv_obj_add_state(next, LV_STATE_DISABLED);
}

void RecipesPanel::render_book()
{
    const SizeTokens &t = pv_.tokens;
    lv_obj_t *header = box(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, t.space_m, 0);
    button(header, LV_SYMBOL_LEFT, kBack, 0);
    lv_obj_t *title = label(header, session_.recipe().title, t.text_l);
    lv_obj_set_flex_grow(title, 1);
    if (session_.card()) button(header, "Steps", kSteps, 0);

    page_area_ = box(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(page_area_, 1);
    lv_obj_set_flex_align(page_area_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(page_area_, LV_OBJ_FLAG_SCROLLABLE);  // pan when the page is larger than the area
    lv_obj_set_style_bg_color(page_area_, lv_color_hex(kCard), 0);
    lv_obj_set_style_bg_opa(page_area_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(page_area_, t.radius, 0);
    const std::string src = image_source_ ? image_source_(RecipeStore::page_path(open_id_, session_.page())) : "";
    if (src.empty()) {
        label(page_area_, "Page images are not available on this build.", t.text_m, kMuted);
    } else {
        page_img_ = lv_image_create(page_area_);
        lv_image_set_src(page_img_, src.c_str());
        lv_image_set_inner_align(page_img_, LV_IMAGE_ALIGN_STRETCH);
        lv_obj_add_flag(page_img_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(page_img_, reinterpret_cast<void *>(pack(kZoom, 0)));
        lv_obj_add_event_cb(page_img_, event_cb, LV_EVENT_DOUBLE_CLICKED, this);  // double-tap the page: zoom
    }

    lv_obj_t *nav = box(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *prev = button(nav, std::string(LV_SYMBOL_LEFT) + "  Back", kPrev, 0);
    lv_obj_t *mid = box(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(mid, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mid, t.space_m, 0);
    lv_obj_t *count = label(mid, "Page " + std::to_string(session_.page()) + " of " + std::to_string(session_.pages()),
                            t.text_m, kMuted);
    lv_obj_set_width(count, LV_SIZE_CONTENT);
    button(mid, zoomed_ ? LV_SYMBOL_MINUS "  Fit" : LV_SYMBOL_PLUS "  Zoom", kZoom, 1);
    lv_obj_t *next = button(nav, "Next  " LV_SYMBOL_RIGHT, kNext, 0, true);
    for (lv_obj_t *b : {prev, next}) lv_obj_set_style_pad_ver(b, t.space_m, 0);
    if (session_.page() <= 1) lv_obj_add_state(prev, LV_STATE_DISABLED);
    if (session_.page() >= session_.pages()) lv_obj_add_state(next, LV_STATE_DISABLED);
    size_page_image();
}

// Fit: the whole page in portrait, the full width in landscape (scroll down to read
// on). Zoom: twice that, pan by dragging.
void RecipesPanel::size_page_image()
{
    if (!page_img_) return;
    lv_image_header_t h;
    if (lv_image_decoder_get_info(lv_image_get_src(page_img_), &h) != LV_RESULT_OK || h.w == 0 || h.h == 0) return;
    lv_obj_update_layout(root_);
    const int32_t aw = lv_obj_get_content_width(page_area_), ah = lv_obj_get_content_height(page_area_);
    int32_t w = aw, hh = static_cast<int32_t>(static_cast<int64_t>(aw) * h.h / h.w);
    if (pv_.orientation == kPortrait && hh > ah) {  // whole page visible
        hh = ah;
        w = static_cast<int32_t>(static_cast<int64_t>(ah) * h.w / h.h);
    }
    if (zoomed_) {
        w *= 2;
        hh *= 2;
    }
    lv_obj_set_size(page_img_, w, hh);
    lv_obj_scroll_to(page_area_, 0, 0, LV_ANIM_OFF);
}

void RecipesPanel::act(int action, int index)
{
    switch (action) {
    case kOpen:
        if (index >= 0 && static_cast<size_t>(index) < filtered_.size()) open_recipe(filtered_[index].id);
        break;
    case kBack: back_to_library(); break;
    case kTag: set_tag(index == 0 || static_cast<size_t>(index) > tags_.size() ? "" : tags_[index - 1]); break;
    case kNext:
        dirty_ = (view_ == View::Book ? session_.next_page() : session_.next_step()) || dirty_;
        if (view_ == View::Book && dirty_) zoomed_ = false;
        break;
    case kPrev:
        dirty_ = (view_ == View::Book ? session_.prev_page() : session_.prev_step()) || dirty_;
        if (view_ == View::Book && dirty_) zoomed_ = false;
        break;
    case kPages:
        if (session_.book()) view_ = View::Book;
        dirty_ = true;
        break;
    case kSteps:
        if (session_.card()) view_ = View::Card;
        dirty_ = true;
        break;
    case kZoom:
        zoomed_ = !zoomed_;
        dirty_ = true;
        break;
    case kMinus: dirty_ = session_.change_servings(-1) || dirty_; break;
    case kPlus: dirty_ = session_.change_servings(+1) || dirty_; break;
    case kTick:
        session_.toggle(static_cast<size_t>(index));
        dirty_ = true;
        break;
    case kTimer: {
        const std::vector<TimeSpan> times = session_.step_times();
        if (ctx_ && index >= 0 && static_cast<size_t>(index) < times.size()) {
            // "<seconds>|<name>": the timer service (step R5) starts a named timer.
            ctx_->publish("timers/start", std::to_string(times[index].seconds) + "|" + session_.recipe().title + ": " +
                                              session_.step_label());
        }
        break;
    }
    }
}

// Re-rendering is deferred to tick(): never delete the object whose event is running.
void RecipesPanel::event_cb(lv_event_t *e)
{
    auto *self = static_cast<RecipesPanel *>(lv_event_get_user_data(e));
    const intptr_t v = reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_current_target_obj(e)));
    self->act(static_cast<int>(v >> 16), static_cast<int>(v & 0xFFFF));
}

void RecipesPanel::search_cb(lv_event_t *e)
{
    auto *self = static_cast<RecipesPanel *>(lv_event_get_user_data(e));
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        self->search_ = lv_textarea_get_text(self->search_box_);
        self->render_grid();
    } else if (code == LV_EVENT_FOCUSED && !self->keyboard_) {
        self->keyboard_ = lv_keyboard_create(self->root_);
        lv_obj_add_flag(self->keyboard_, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(self->keyboard_, lv_pct(100), lv_pct(45));
        lv_obj_align(self->keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(self->keyboard_, self->search_box_);
    } else if ((code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) && self->keyboard_) {
        lv_obj_delete_async(self->keyboard_);
        self->keyboard_ = nullptr;
    }
}

void RecipesPanel::gesture_cb(lv_event_t *e)
{
    auto *self = static_cast<RecipesPanel *>(lv_event_get_user_data(e));
    if (self->view_ != View::Card && self->view_ != View::Book) return;
    if (self->view_ == View::Book && self->zoomed_) return;  // dragging pans a zoomed page
    const lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT) self->act(kNext, 0);
    if (dir == LV_DIR_RIGHT) self->act(kPrev, 0);
}

}  // namespace summit::recipes
