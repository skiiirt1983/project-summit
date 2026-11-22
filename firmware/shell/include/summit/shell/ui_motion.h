// LVGL side of the motion tokens (summit/shell/motion.h): the few moves every screen shares.
// Only cheap properties are animated (position, translate, background opacity), never whole-
// screen opacity or scaling, so the same moves stay smooth on the ESP32-P4 software renderer.
#pragma once
#include <cstdint>

#include "lvgl.h"
#include "summit/shell/motion.h"

namespace summit::shell::ui_motion {

enum class Prop { X, TranslateX, TranslateY, BgOpa };

// Animates one property. Any running animation of the same property on `obj` is replaced.
// `hide_when_done` adds LV_OBJ_FLAG_HIDDEN at the end (for things leaving the screen).
void animate(lv_obj_t *obj, Prop prop, int32_t from, int32_t to, uint32_t ms, const motion::Bezier &curve,
             bool hide_when_done = false);
void stop(lv_obj_t *obj);  // all animations on obj

// A full-screen page pushed in from the right / pulled back out (Updates, Modules), like a phone's settings.
void push_in(lv_obj_t *obj, int32_t width);
void pull_out(lv_obj_t *obj, int32_t width);

// A sheet hanging from the strip: drops down / goes back up. `backdrop` dims behind it.
void sheet_down(lv_obj_t *sheet, lv_obj_t *backdrop, int32_t height, lv_opa_t backdrop_opa);
void sheet_up(lv_obj_t *sheet, lv_obj_t *backdrop, int32_t height, lv_opa_t backdrop_opa);

// Press feedback for anything tappable: under the finger it shrinks by `shrink` px a side and
// lightens within kPressMs, and relaxes back over kReleaseMs. `shrink` comes from a size token.
void press_feedback(lv_obj_t *obj, int32_t shrink);
inline void press_feedback(lv_obj_t *obj);  // with press_travel()
// Press travel from the size tokens of the default display (about 2 px on the 5" panel).
int32_t press_travel();
inline void press_feedback(lv_obj_t *obj) { press_feedback(obj, press_travel()); }

// Hide or show an object only when that changes anything. LVGL redraws the object's area on every
// add_flag / remove_flag call, even when the flag is already as asked, and the screens call these every tick:
// the top bar and the Updates screen were redrawn about 50 times a second and flashed.
inline void set_hidden(lv_obj_t *o, bool hidden)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace summit::shell::ui_motion
