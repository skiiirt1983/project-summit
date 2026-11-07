#include "summit/system_settings.h"

namespace summit {

SettingsSchema system_settings_schema()
{
    SettingsSchema s;
    s.version = kSystemSettingsVersion;
    s.defs = {
        {"update/channel", std::string("stable"), {"stable", "beta", "alpha"}},
        {"ui/rotation_lock", std::string("off"), {"off", "landscape", "portrait"}},
        // Page order after Home (always first): comma-separated panel ids, set from the web config.
        {"ui/page_order", std::string(""), {}},
        // Quick-settings sheet (ARCHITECTURE.md "Glance strip"). Added within v1:
        // adding keys with defaults needs no version bump.
        {"display/brightness", int64_t{80}, {}},
        {"audio/volume", int64_t{60}, {}},
        {"system/dnd", false, {}},
    };
    // v1 is the first schema: no migrations yet. Add {from, apply} per version bump.
    s.migrations = {};
    return s;
}

}  // namespace summit
