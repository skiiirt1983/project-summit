#pragma once
#include "summit/registry.h"
#include "summit/state_store.h"

namespace sim {

// home_clock, calendar, weather, news, printer: placeholders for the shell.
void register_demo_panels(summit::PanelRegistry &registry);
// Placeholder glance data (time, next event, weather, printer, Wi-Fi, update, battery).
void publish_placeholders(summit::StateStore &state);

}  // namespace sim
