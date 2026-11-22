/* Slopeform Summit boot screen — LVGL 9.x
 * Summit spark rotates + pulses on the top peak; "LOADING" with stepping dots below the logo.
 * Needs: LV_FONT_MONTSERRAT_20 enabled in lv_conf.h / menuconfig.
 * LVGL 8: rename lv_image_* -> lv_img_*, lv_image_set_rotation -> lv_img_set_angle,
 *         lv_image_set_scale -> lv_img_set_zoom, lv_image_dsc_t -> lv_img_dsc_t.
 */
#include "summit/boot/boot_screen.h"
#include <stdio.h>

LV_IMAGE_DECLARE(boot_logo);   /* 448x348 */
LV_IMAGE_DECLARE(boot_spark);  /* 208x208 */
LV_IMAGE_DECLARE(boot_rays);   /* 128x128 */

#define SUMMIT_X   217   /* summit position inside boot_logo, px */
#define SUMMIT_Y   4
#define LOOP_MS    2000
#define COL_LIME   lv_color_hex(0xA3E635)
#define COL_FOG    lv_color_hex(0x8A949C)

static lv_obj_t *scr, *lbl, *dots[3], *spark, *rays;
static lv_timer_t *dot_timer;

static void rot_cb(void *o, int32_t v)   { lv_image_set_rotation(o, v); }
static void scale_cb(void *o, int32_t v) { lv_image_set_scale(o, v); }
static void opa_cb(void *o, int32_t v)   { lv_obj_set_style_image_opa(o, (lv_opa_t)v, 0); }

static void dot_tick(lv_timer_t *t)
{
    static int n = 0;
    n = (n + 1) % 4;
    for (int i = 0; i < 3; i++)
        lv_obj_set_style_bg_opa(dots[i], i < n ? LV_OPA_COVER : LV_OPA_20, 0);
}

static void anim(lv_obj_t *o, lv_anim_exec_xcb_t cb, int32_t a, int32_t b, uint32_t ms,
                 bool back, lv_anim_path_cb_t path)
{
    lv_anim_t an;
    lv_anim_init(&an);
    lv_anim_set_var(&an, o);
    lv_anim_set_exec_cb(&an, cb);
    lv_anim_set_values(&an, a, b);
    lv_anim_set_duration(&an, back ? ms / 2 : ms);
    if (back) lv_anim_set_playback_duration(&an, ms / 2);
    lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&an, path);
    lv_anim_start(&an);
}

/* Slopeform Summit addition: the grow-in from the media kit video. The spark scales up from nothing on
 * the peak (with a small overshoot) while it fades in and starts to spin; when it has landed it settles
 * into the pulse below. Rays fade in a little later. */
#define GROW_MS 550
#define GROW_FROM_SCALE 8

static void grow_done(lv_anim_t *a)
{
    (void)a;
    if (spark) anim(spark, scale_cb, 215, 297, LOOP_MS, true, lv_anim_path_ease_in_out);
}

static void one_shot(lv_obj_t *o, lv_anim_exec_xcb_t cb, int32_t a, int32_t b, uint32_t ms, uint32_t delay,
                     lv_anim_path_cb_t path, lv_anim_completed_cb_t done)
{
    lv_anim_t an;
    lv_anim_init(&an);
    lv_anim_set_var(&an, o);
    lv_anim_set_exec_cb(&an, cb);
    lv_anim_set_values(&an, a, b);
    lv_anim_set_duration(&an, ms);
    lv_anim_set_delay(&an, delay);
    lv_anim_set_path_cb(&an, path);
    if (done) lv_anim_set_completed_cb(&an, done);
    lv_anim_start(&an);
}

lv_obj_t *boot_screen_show(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    /* Slopeform Summit change: a solid very dark grey instead of the kit's gradient (Paul). */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0D0F), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Logo, centred, nudged up to leave room for the status line */
    lv_obj_t *logo = lv_image_create(scr);
    lv_image_set_src(logo, &boot_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -40);
    lv_obj_update_layout(scr);
    int32_t lx = lv_obj_get_x(logo), ly = lv_obj_get_y(logo);

    /* Faint 8-point rays, counter-rotating */
    rays = lv_image_create(scr);
    lv_image_set_src(rays, &boot_rays);
    lv_image_set_pivot(rays, boot_rays.header.w / 2, boot_rays.header.h / 2);
    lv_obj_set_pos(rays, lx + SUMMIT_X - (int32_t)boot_rays.header.w / 2, ly + SUMMIT_Y - (int32_t)boot_rays.header.h / 2);
    anim(rays, rot_cb, 0, -450, LOOP_MS, false, lv_anim_path_linear);   /* 0.1 deg units */

    /* Main lime spark: rotate 90 deg per loop + pulse 0.84x..1.16x */
    spark = lv_image_create(scr);
    lv_image_set_src(spark, &boot_spark);
    lv_image_set_pivot(spark, boot_spark.header.w / 2, boot_spark.header.h / 2);
    lv_obj_set_pos(spark, lx + SUMMIT_X - (int32_t)boot_spark.header.w / 2, ly + SUMMIT_Y - (int32_t)boot_spark.header.h / 2);
    anim(spark, rot_cb, 0, 900, LOOP_MS, false, lv_anim_path_linear);
    /* Grow in (256 = 1.0x), then the pulse 0.84x..1.16x starts from grow_done(). */
    lv_image_set_scale(spark, GROW_FROM_SCALE);
    lv_obj_set_style_image_opa(spark, LV_OPA_TRANSP, 0);
    one_shot(spark, scale_cb, GROW_FROM_SCALE, 215, GROW_MS, 0, lv_anim_path_overshoot, grow_done);
    one_shot(spark, opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, GROW_MS / 2, 0, lv_anim_path_ease_out, NULL);
    lv_obj_set_style_image_opa(rays, LV_OPA_TRANSP, 0);
    one_shot(rays, opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, GROW_MS, GROW_MS / 3, lv_anim_path_ease_out, NULL);

    /* LOADING + three stepping dots */
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_align_to(row, logo, LV_ALIGN_OUT_BOTTOM_MID, 0, 70);

    lbl = lv_label_create(row);
    lv_label_set_text(lbl, "LOADING");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_letter_space(lbl, 7, 0);
    lv_obj_set_style_text_color(lbl, COL_FOG, 0);
    for (int i = 0; i < 3; i++) {
        dots[i] = lv_obj_create(row);
        lv_obj_remove_style_all(dots[i]);
        lv_obj_set_size(dots[i], 6, 6);
        lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dots[i], COL_LIME, 0);
        lv_obj_set_style_bg_opa(dots[i], LV_OPA_20, 0);
    }
    dot_timer = lv_timer_create(dot_tick, LOOP_MS / 4, NULL);

    lv_screen_load(scr);
    return scr;
}

void boot_screen_set_status(const char *txt)
{
    if (lbl) lv_label_set_text(lbl, txt);
}

void boot_screen_hide(void)
{
    if (!scr) return;
    if (dot_timer) { lv_timer_delete(dot_timer); dot_timer = NULL; }
    lv_anim_delete(spark, NULL);
    lv_anim_delete(rays, NULL);
    lv_obj_delete_delayed(scr, 0);
    scr = NULL; lbl = NULL;
}
