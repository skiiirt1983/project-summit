// The Recipe reader module (DECISIONS.md D-008): manifest shared by the device,
// the simulator and tests.
#pragma once
#include "summit/modules.h"

namespace summit::recipes {

constexpr const char *kModuleId = "recipes";
constexpr const char *kPanelId = "recipes";

ModuleManifest module_manifest();

}  // namespace summit::recipes
