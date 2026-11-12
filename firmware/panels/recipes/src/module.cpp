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

}  // namespace summit::recipes
