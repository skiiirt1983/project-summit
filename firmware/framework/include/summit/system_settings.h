// The device settings schema (hard rule 4).
// Adding a key with a default: no version bump. Renaming/removing a key or
// changing its type or meaning: bump kSystemSettingsVersion, add a Migration and
// update tests/host/test_settings_schema.cpp.
#pragma once
#include "summit/settings.h"

namespace summit {

constexpr uint32_t kSystemSettingsVersion = 1;

// System keys (from ARCHITECTURE.md): update channel, rotation lock.
// Panel settings are appended from PanelRegistry::setting_defs().
SettingsSchema system_settings_schema();

}  // namespace summit
