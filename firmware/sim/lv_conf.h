// LVGL configuration for the PC simulator. Unlisted options use LVGL defaults.
// Fonts must match the device (firmware/sdkconfig.defaults) so text measures the same.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 16

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#define LV_USE_QRCODE 1

#ifndef SUMMIT_SIM_SDL
#define SUMMIT_SIM_SDL 0
#endif
#define LV_USE_SDL SUMMIT_SIM_SDL
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>

#endif  // LV_CONF_H
