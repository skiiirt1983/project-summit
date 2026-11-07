// The device settings schema. Bump kSystemSettingsVersion, add a Migration and
// update tests/host/test_settings_schema.cpp for EVERY change (hard rule 4).
#pragma once
#include "summit/settings.h"

namespace summit {

constexpr uint32_t kSystemSettingsVersion = 1;

// System keys (from ARCHITECTURE.md): update channel, rotation lock.
// Panel settings are appended from PanelRegistry::setting_defs().
SettingsSchema system_settings_schema();

}  // namespace summit
