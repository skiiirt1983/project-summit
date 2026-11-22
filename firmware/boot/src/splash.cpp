#include "summit/boot/splash.h"

#include "summit/boot/boot_screen.h"

namespace summit::boot {

void Splash::show(uint32_t now_ms)
{
    if (active_) return;
    main_ = lv_screen_active();
    // The shell's strip and sheets live on the display's top layer, which is shared by every screen.
    // Transparent, not HIDDEN: LVGL cannot un-hide a display layer (it has no screen to mark dirty).
    lv_obj_set_style_opa(lv_layer_top(), LV_OPA_TRANSP, 0);
    boot_screen_show();
    timer_.start(now_ms);
    active_ = true;
}

void Splash::set_status(const char *text)
{
    if (active_) boot_screen_set_status(text);
}

bool Splash::tick(uint32_t now_ms)
{
    if (!active_ || !timer_.done(now_ms)) return false;
    lv_screen_load(main_);  // the main screen first, then the boot screen goes (boot_screen.h)
    lv_obj_set_style_opa(lv_layer_top(), LV_OPA_COVER, 0);
    boot_screen_hide();
    active_ = false;
    return true;
}

}  // namespace summit::boot
