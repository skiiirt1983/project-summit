// The Recipe reader module (DECISIONS.md D-008): manifest shared by the device,
// the simulator and tests.
#pragma once
#include "summit/modules.h"
#include "summit/recipes/store.h"
#include "summit/state_store.h"

namespace summit::recipes {

constexpr const char *kModuleId = "recipes";
constexpr const char *kPanelId = "recipes";

ModuleManifest module_manifest();

// The recipes on the SD card, for the Modules screen's "Delete recipes" action.
// After a delete it closes any open recipe and bumps recipes/library_rev so the
// Recipes panel shows the (now empty) library.
class RecipeModuleData : public ModuleData {
public:
    RecipeModuleData(RecipeStore &store, StateStore &state) : store_(store), state_(state) {}
    std::string noun(int count) override { return count == 1 ? "recipe" : "recipes"; }
    int count() override { return store_.available() ? store_.count() : 0; }
    bool delete_all(int *deleted) override;

private:
    RecipeStore &store_;
    StateStore &state_;
    int64_t rev_ = 0;
};

}  // namespace summit::recipes
