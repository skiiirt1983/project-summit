#include "summit/system_settings.h"

namespace summit {

SettingsSchema system_settings_schema()
{
    SettingsSchema s;
    s.version = kSystemSettingsVersion;
    s.defs = {
        {"update/channel", std::string("stable"), {"stable", "beta", "alpha"}},
        {"ui/rotation_lock", std::string("off"), {"off", "landscape", "portrait"}},
    };
    // v1 is the first schema: no migrations yet. Add {from, apply} per bump.
    s.migrations = {};
    return s;
}

}  // namespace summit
