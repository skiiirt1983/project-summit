#pragma once
#include "summit/modules.h"
#include "summit/recipes/store.h"
#include "summit/registry.h"
#include "summit/state_store.h"

namespace sim {

// home_clock, calendar, weather, news, printer: placeholders for the shell.
void register_demo_panels(summit::PanelRegistry &registry);
// The Recipe reader module (its panel is a placeholder until step R4).
void register_demo_modules(summit::ModuleManager &modules);
// Two sample recipes on the simulated SD card.
void seed_recipes(summit::recipes::RecipeStore &store);
// Placeholder glance data (time, next event, weather, printer, Wi-Fi, update, battery).
void publish_placeholders(summit::StateStore &state);

}  // namespace sim
