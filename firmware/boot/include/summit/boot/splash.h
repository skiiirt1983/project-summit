// The Slopeform Summit boot screen (media kit, boot_screen.c) on top of the shell: shown for at least
// 2 s and until start-up is ready, then the shell's own screen is brought back.
#pragma once
#include <cstdint>

#include "lvgl.h"
#include "summit/boot/splash_timer.h"

namespace summit::boot {

class Splash {
public:
    // Call after the shell has been built on the active screen. Shows the boot screen over it
    // (the shell's top layer, strip and sheets, is hidden meanwhile).
    void show(uint32_t now_ms);
    // Replaces "LOADING" (e.g. "CONNECTING WI-FI").
    void set_status(const char *text);
    // Start-up has finished; the screen goes at the later of this and the 2 s minimum.
    void set_ready() { timer_.set_ready(); }
    // From the UI loop. Returns true on the call that removes the boot screen.
    bool tick(uint32_t now_ms);
    bool active() const { return active_; }

private:
    SplashTimer timer_;
    lv_obj_t *main_ = nullptr;
    bool active_ = false;
};

}  // namespace summit::boot
