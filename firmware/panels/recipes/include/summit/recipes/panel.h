// The Recipes panel (D-008): library, card mode (ingredients + one big step), and
// book mode (original pages; arrives in the next step). LVGL front end over
// cook.h / store.h. Lays out with size tokens for both orientations.
#pragma once
#include <string>
#include <vector>

#include "lvgl.h"
#include "summit/panel_api.h"
#include "summit/recipes/cook.h"
#include "summit/recipes/store.h"

namespace summit::recipes {

class RecipesPanel : public Panel {
public:
    enum class View { NoCard, Library, Card, PagesOnly };

    explicit RecipesPanel(RecipeStore &store);

    const PanelManifest &manifest() const override { return manifest_; }
    bool create(PanelContext &ctx) override;
    void show(PanelContext &ctx, const PanelView &view) override;
    void tick(PanelContext &ctx, uint32_t now_ms) override;
    void on_event(PanelContext &ctx, const std::string &topic, const Value &value) override;
    void hide(PanelContext &ctx) override;
    void destroy(PanelContext &ctx) override;

    // Actions (also used by the simulator self-test).
    View view() const { return view_; }
    bool open_recipe(const std::string &id);
    void back_to_library();
    void set_search(const std::string &text);
    void set_tag(const std::string &tag);
    std::vector<std::string> visible_ids() const;
    const CookSession &session() const { return session_; }
    lv_obj_t *find(int action, int index = 0) const;  // a clickable by action code

    enum Action { kOpen = 1, kBack, kTag, kNext, kPrev, kMinus, kPlus, kTick, kTimer };

private:
    void render();
    void render_library();
    void render_grid();
    void render_card();
    void render_message(const std::string &text, bool with_back);
    void sync_flags();
    void act(int action, int index);
    lv_obj_t *button(lv_obj_t *parent, const std::string &text, int action, int index, bool primary = false);
    static void event_cb(lv_event_t *e);
    static void search_cb(lv_event_t *e);
    static void gesture_cb(lv_event_t *e);

    PanelManifest manifest_;
    RecipeStore &store_;
    PanelContext *ctx_ = nullptr;
    PanelView pv_;
    bool shown_ = false, dirty_ = false;
    View view_ = View::Library;
    std::vector<IndexEntry> index_, filtered_;
    std::vector<std::string> tags_;
    std::string search_, tag_, open_id_;
    CookSession session_;
    lv_obj_t *root_ = nullptr, *grid_ = nullptr, *search_box_ = nullptr, *keyboard_ = nullptr;
};

}  // namespace summit::recipes
