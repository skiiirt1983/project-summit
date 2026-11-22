#include "summit/system_settings.h"

namespace summit {

SettingsSchema system_settings_schema()
{
    SettingsSchema s;
    s.version = kSystemSettingsVersion;
    s.defs = {
        // "alpha" while the project is in alpha: only alpha builds exist. Change back to "stable" at 1.0.
        {"update/channel", std::string("alpha"), {"stable", "beta", "alpha"}},
        {"ui/rotation_lock", std::string("off"), {"off", "landscape", "portrait"}},
        // Page order after Home (always first): comma-separated panel ids, set from the web config.
        {"ui/page_order", std::string(""), {}},
        // Quick-settings sheet (ARCHITECTURE.md "Glance strip"). Added within v1:
        // adding keys with defaults needs no version bump.
        {"display/brightness", int64_t{80}, {}},
        {"audio/volume", int64_t{60}, {}},
        {"system/dnd", false, {}},
        // Sleep (D-012, ARCHITECTURE.md "Sleep"). Additive keys: no version bump.
        SettingDef("power/idle_sleep_min", int64_t{5}, {}, "Sleep after (minutes)",
                   "On battery, the device sleeps after this long without a touch. 0 = never. "
                   "Timers and alarms still wake it."),
        SettingDef("power/stay_on_charging", true, {}, "Stay on when charging",
                   "On USB power the device does not go to sleep."),
    };
    // v1 is the first schema: no migrations yet. Add {from, apply} per version bump.
    s.migrations = {};
    return s;
}

}  // namespace summit
