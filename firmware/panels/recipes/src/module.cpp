#include "summit/recipes/module.h"

namespace summit::recipes {

ModuleManifest module_manifest()
{
    ModuleManifest m;
    m.id = kModuleId;
    m.name = "Recipe reader";
    m.icon = "\xEF\x80\x8B";  // LV_SYMBOL_LIST
    m.description = "Cook from your own recipes: step-by-step cards or the original pages.";
    m.panel_id = kPanelId;
    m.requires_.sd_card = true;
    m.requires_.min_free_mb = 200;
    return m;
}

bool RecipeModuleData::delete_all(int *deleted)
{
    const bool ok = store_.delete_all(deleted);
    state_.set("recipes/cooking", std::string());
    state_.set("recipes/cooking_chip", std::string());
    state_.set("recipes/library_rev", ++rev_);
    return ok;
}

}  // namespace summit::recipes
