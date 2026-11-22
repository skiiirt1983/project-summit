#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Shows the Slopeform Summit boot screen on the active display. Works in portrait or landscape. */
lv_obj_t *boot_screen_show(void);
/* Optional: replace "LOADING" with a status line, e.g. "CONNECTING WI-FI". */
void boot_screen_set_status(const char *txt);
/* Fade out and delete; load your main screen first. */
void boot_screen_hide(void);
#ifdef __cplusplus
}
#endif
