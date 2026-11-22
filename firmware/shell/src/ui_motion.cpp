#include "summit/shell/ui_motion.h"

#include "summit/shell/layout.h"

namespace summit::shell::ui_motion {

namespace {

void set_x(void *o, int32_t v) { lv_obj_set_x(static_cast<lv_obj_t *>(o), v); }
void set_tx(void *o, int32_t v) { lv_obj_set_style_translate_x(static_cast<lv_obj_t *>(o), v, 0); }
void set_ty(void *o, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(o), v, 0); }
void set_bg_opa(void *o, int32_t v)
{
    lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(o), static_cast<lv_opa_t>(v), 0);
}

lv_anim_exec_xcb_t exec_for(Prop p)
{
    switch (p) {
        case Prop::X: return set_x;
        case Prop::TranslateX: return set_tx;
        case Prop::TranslateY: return set_ty;
        case Prop::BgOpa: return set_bg_opa;
    }
    return set_x;
}

void hide_done(lv_anim_t *a) { lv_obj_add_flag(static_cast<lv_obj_t *>(a->var), LV_OBJ_FLAG_HIDDEN); }

}  // namespace

void animate(lv_obj_t *obj, Prop prop, int32_t from, int32_t to, uint32_t ms, const motion::Bezier &curve,
             bool hide_when_done)
{
    if (!obj) return;
    const lv_anim_exec_xcb_t exec = exec_for(prop);
    lv_anim_delete(obj, exec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_path_cb(&a, lv_anim_path_custom_bezier3);
    lv_anim_set_bezier3_param(&a, curve.x1, curve.y1, curve.x2, curve.y2);
    if (hide_when_done) lv_anim_set_completed_cb(&a, hide_done);
    lv_anim_start(&a);
}

void stop(lv_obj_t *obj)
{
    if (obj) lv_anim_delete(obj, nullptr);
}

void push_in(lv_obj_t *obj, int32_t width)
{
    stop(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    animate(obj, Prop::TranslateX, width, 0, motion::kScreenInMs, motion::kEnter);
}

void pull_out(lv_obj_t *obj, int32_t width)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    stop(obj);
    animate(obj, Prop::TranslateX, lv_obj_get_style_translate_x(obj, LV_PART_MAIN), width, motion::kScreenOutMs, motion::kExit,
            true);
}

void sheet_down(lv_obj_t *sheet, lv_obj_t *backdrop, int32_t height, lv_opa_t backdrop_opa)
{
    stop(sheet);
    stop(backdrop);
    lv_obj_remove_flag(sheet, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
    animate(sheet, Prop::TranslateY, -height, 0, motion::kSheetInMs, motion::kEnter);
    animate(backdrop, Prop::BgOpa, LV_OPA_TRANSP, backdrop_opa, motion::kSheetInMs, motion::kStandard);
}

void sheet_up(lv_obj_t *sheet, lv_obj_t *backdrop, int32_t height, lv_opa_t backdrop_opa)
{
    stop(sheet);
    stop(backdrop);
    animate(sheet, Prop::TranslateY, lv_obj_get_style_translate_y(sheet, LV_PART_MAIN), -height, motion::kSheetOutMs,
            motion::kExit, true);
    animate(backdrop, Prop::BgOpa, backdrop_opa, LV_OPA_TRANSP, motion::kSheetOutMs, motion::kStandard, true);
}

int32_t press_travel()
{
    lv_display_t *d = lv_display_get_default();
    if (!d) return 1;
    const Tokens t = tokens_for(static_cast<int16_t>(lv_display_get_horizontal_resolution(d)),
                                static_cast<int16_t>(lv_display_get_vertical_resolution(d)));
    return t.space_s / 4 > 1 ? t.space_s / 4 : 1;
}

void press_feedback(lv_obj_t *obj, int32_t shrink)
{
    static const lv_style_prop_t props[] = {LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT,
                                            LV_STYLE_COLOR_FILTER_OPA, LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t press, release;
    static lv_color_filter_dsc_t lighten;
    static bool ready = false;
    if (!ready) {
        ready = true;
        lv_style_transition_dsc_init(&press, props, lv_anim_path_ease_out, motion::kPressMs, 0, nullptr);
        lv_style_transition_dsc_init(&release, props, lv_anim_path_ease_out, motion::kReleaseMs, 0, nullptr);
        lv_color_filter_dsc_init(&lighten, [](const lv_color_filter_dsc_t *, lv_color_t c, lv_opa_t opa) {
            return lv_color_lighten(c, opa);
        });
    }
    // The transition of the state being entered is the one used: fast in, slow out.
    lv_obj_set_style_transition(obj, &release, 0);
    lv_obj_set_style_transition(obj, &press, LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(obj, 0, 0);
    lv_obj_set_style_transform_height(obj, 0, 0);
    lv_obj_set_style_transform_width(obj, -shrink, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(obj, -shrink, LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_dsc(obj, &lighten, 0);
    lv_obj_set_style_color_filter_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_color_filter_dsc(obj, &lighten, LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(obj, LV_OPA_20, LV_STATE_PRESSED);
}

}  // namespace summit::shell::ui_motion
