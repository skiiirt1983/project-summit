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
        // Hardware debugging while the screen flashing is chased (0.0.2-alpha.10). Additive keys: no version bump.
        // Remove once the flashing is fixed.
        SettingDef("debug/display_flush", std::string("normal"), {"normal", "cpu_copy", "skip"}, "Debug: how pixels reach the panel",
                   "normal = DMA2D copy. cpu_copy = plain CPU copy into the frame buffer. skip = draw nothing "
                   "(the screen freezes; if it still flashes, drawing is not the cause)."),
        SettingDef("debug/backlight", std::string("pwm"), {"pwm", "full"}, "Debug: backlight",
                   "pwm = brightness slider (PWM). full = constant full brightness, no PWM (slider ignored)."),
        // Additive key (no version bump), released in alpha.24/25 and so never removed. Flash auto suspend was tried
        // for a lit install screen and made the firmware panic at boot on this board (CONFIG_SPI_FLASH_AUTO_SUSPEND
        // must stay off): the default is therefore pulse.
        SettingDef("update/write_display", std::string("pulse"), {"pulse", "lit"}, "Screen while an update is written",
                   "pulse = the screen is switched off during the flash writes and lit briefly between them with the "
                   "progress (default; the flash writes glitch a lit screen). lit = keep the screen on while writing: "
                   "expect glitching."),
    };
    // v1 is the first schema: no migrations yet. Add {from, apply} per version bump.
    s.migrations = {};
    return s;
}

}  // namespace summit
