#include "summit/jpeg_esp.h"

#include <cstdio>
#include <cstring>

#include "driver/jpeg_decode.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "src/draw/lv_draw_buf_private.h"
#include "src/draw/lv_image_decoder_private.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "summit/jpeg_info.h"

namespace summit::jpeg_hw {

namespace {

const char *TAG = "jpeg";

constexpr size_t kReserveBytes = 1536 * 1024;  // PSRAM left alone for everything else
constexpr size_t kHeadBytes = 4096;            // enough to reach the frame header

// 16x16 solid red, baseline 4:4:4 (made with Pillow: Image.new("RGB",(16,16),(255,0,0)).save(..., quality=90,
// subsampling=0)). Decoded at start-up to see whether the engine's red/blue order matches LVGL's RGB565.
const uint8_t kRedJpeg[] = {
0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03,
0x03, 0x03, 0x03, 0x04, 0x03, 0x03, 0x04, 0x05, 0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0A, 0x07,
0x07, 0x06, 0x08, 0x0C, 0x0A, 0x0C, 0x0C, 0x0B, 0x0A, 0x0B, 0x0B, 0x0D, 0x0E, 0x12, 0x10, 0x0D,
0x0E, 0x11, 0x0E, 0x0B, 0x0B, 0x10, 0x16, 0x10, 0x11, 0x13, 0x14, 0x15, 0x15, 0x15, 0x0C, 0x0F,
0x17, 0x18, 0x16, 0x14, 0x18, 0x12, 0x14, 0x15, 0x14, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x03, 0x04,
0x04, 0x05, 0x04, 0x05, 0x09, 0x05, 0x05, 0x09, 0x14, 0x0D, 0x0B, 0x0D, 0x14, 0x14, 0x14, 0x14,
0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xFF, 0xC0,
0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x10, 0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11,
0x01, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23,
0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17,
0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5,
0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1,
0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xC4, 0x00, 0x1F, 0x01, 0x00, 0x03,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x11, 0x00,
0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00,
0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13,
0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15,
0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27,
0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4,
0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2,
0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
0xFA, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00, 0xF9,
0xD2, 0xBF, 0x0C, 0x3F, 0xD5, 0x30, 0xA0, 0x02, 0x80, 0x0A, 0x00, 0xFF, 0xD9
};

jpeg_decoder_handle_t g_engine = nullptr;
jpeg_dec_rgb_element_order_t g_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
bool g_usable = false;
lv_draw_buf_handlers_t g_handlers;  // LVGL's image handlers, but memory comes from the JPEG driver (cache-line aligned)

unsigned g_decodes = 0, g_fails = 0, g_last_ms = 0, g_last_w = 0, g_last_h = 0;
char g_last_error[64] = "";

void note_fail(const char *why)
{
    ++g_fails;
    snprintf(g_last_error, sizeof g_last_error, "%s", why);
    ESP_LOGW(TAG, "decode failed: %s", why);
}

void *dec_malloc(size_t size, lv_color_format_t)
{
    size_t got = 0;
    jpeg_decode_memory_alloc_cfg_t cfg = {};
    cfg.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER;
    return jpeg_alloc_decoder_mem(size, &cfg, &got);
}

void dec_free(void *p) { heap_caps_free(p); }

uint8_t *alloc_input(size_t size)
{
    size_t got = 0;
    jpeg_decode_memory_alloc_cfg_t cfg = {};
    cfg.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER;
    return static_cast<uint8_t *>(jpeg_alloc_decoder_mem(size, &cfg, &got));
}

// Decodes one JPEG held in memory into a new LVGL draw buffer (RGB565, visible size = the picture's own size, rows
// padded to a multiple of 16 by the engine). Null with a reason in `why` on failure.
lv_draw_buf_t *decode(const uint8_t *data, size_t size, const char **why)
{
    JpegInfo info;
    std::string reason;
    if (!parse_jpeg_header(data, size, info, &reason)) {
        static char buf[64];
        snprintf(buf, sizeof buf, "%s", reason.c_str());
        *why = buf;
        return nullptr;
    }
    const DecodePlan plan = plan_decode(info, heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), kReserveBytes);
    if (!plan.ok) {
        static char buf[64];
        snprintf(buf, sizeof buf, "%s", plan.why.c_str());
        *why = buf;
        return nullptr;
    }
    lv_draw_buf_t *out = lv_draw_buf_create_ex(&g_handlers, plan.aligned_w, plan.aligned_h, LV_COLOR_FORMAT_RGB565, 0);
    if (!out) {
        *why = "no memory for the picture";
        return nullptr;
    }
    jpeg_decode_cfg_t cfg = {};
    cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    cfg.rgb_order = g_order;
    cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
    uint32_t out_size = 0;
    const esp_err_t e = jpeg_decoder_process(g_engine, &cfg, data, static_cast<uint32_t>(size), out->data,
                                             static_cast<uint32_t>(out->data_size), &out_size);
    if (e != ESP_OK) {
        lv_draw_buf_destroy(out);
        static char buf[64];
        snprintf(buf, sizeof buf, "engine error %s", esp_err_to_name(e));
        *why = buf;
        return nullptr;
    }
    out->header.w = info.width;  // the visible part; the stride still covers the padded rows
    out->header.h = info.height;
    return out;
}

bool is_jpeg_name(const char *src)
{
    const char *ext = lv_fs_get_ext(src);
    return ext && (lv_strcmp(ext, "jpg") == 0 || lv_strcmp(ext, "jpeg") == 0);
}

lv_result_t decoder_info(lv_image_decoder_t *, lv_image_decoder_dsc_t *dsc, lv_image_header_t *header)
{
    if (dsc->src_type != LV_IMAGE_SRC_FILE || !g_usable || !is_jpeg_name(static_cast<const char *>(dsc->src))) return LV_RESULT_INVALID;
    uint8_t head[kHeadBytes];
    uint32_t got = 0;
    if (lv_fs_read(&dsc->file, head, sizeof head, &got) != LV_FS_RES_OK || got < 4) return LV_RESULT_INVALID;
    JpegInfo info;
    if (!parse_jpeg_header(head, got, info)) return LV_RESULT_INVALID;
    header->cf = LV_COLOR_FORMAT_RGB565;
    header->w = info.width;
    header->h = info.height;
    header->stride = round_up(info.width, kJpegAlign) * 2;
    return LV_RESULT_OK;
}

lv_result_t decoder_open(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc)
{
    if (dsc->src_type != LV_IMAGE_SRC_FILE || !g_usable) return LV_RESULT_INVALID;
    const int64_t t0 = esp_timer_get_time();
    // LVGL closes dsc->file after the info step and before open, so the file is opened again here by name.
    lv_fs_file_t f;
    if (lv_fs_open(&f, static_cast<const char *>(dsc->src), LV_FS_MODE_RD) != LV_FS_RES_OK) {
        note_fail("cannot open the file");
        return LV_RESULT_INVALID;
    }
    uint32_t size = 0;
    if (lv_fs_seek(&f, 0, LV_FS_SEEK_END) != LV_FS_RES_OK || lv_fs_tell(&f, &size) != LV_FS_RES_OK || size < 4 ||
        size > 8u * 1024 * 1024 || lv_fs_seek(&f, 0, LV_FS_SEEK_SET) != LV_FS_RES_OK) {
        lv_fs_close(&f);
        note_fail("cannot size the file");
        return LV_RESULT_INVALID;
    }
    uint8_t *in = alloc_input(size);
    if (!in) {
        lv_fs_close(&f);
        note_fail("no memory for the file");
        return LV_RESULT_INVALID;
    }
    uint32_t got = 0;
    const lv_fs_res_t rr = lv_fs_read(&f, in, size, &got);
    lv_fs_close(&f);
    if (rr != LV_FS_RES_OK || got != size) {
        heap_caps_free(in);
        note_fail("file read failed");
        return LV_RESULT_INVALID;
    }
    const char *why = "";
    lv_draw_buf_t *decoded = decode(in, size, &why);
    heap_caps_free(in);
    if (!decoded) {
        note_fail(why);
        return LV_RESULT_INVALID;
    }
    g_last_ms = static_cast<unsigned>((esp_timer_get_time() - t0) / 1000);
    g_last_w = decoded->header.w;
    g_last_h = decoded->header.h;
    ++g_decodes;
    dsc->decoded = decoded;
    if (dsc->args.no_cache || !lv_image_cache_is_enabled()) return LV_RESULT_OK;
    lv_image_cache_data_t key;
    key.src_type = dsc->src_type;
    key.src = dsc->src;
    key.slot.size = decoded->data_size;
    lv_cache_entry_t *entry = lv_image_decoder_add_to_cache(decoder, &key, decoded, nullptr);
    if (!entry) {
        lv_draw_buf_destroy(decoded);
        dsc->decoded = nullptr;
        return LV_RESULT_INVALID;
    }
    dsc->cache_entry = entry;
    return LV_RESULT_OK;
}

void decoder_close(lv_image_decoder_t *, lv_image_decoder_dsc_t *dsc)
{
    if (dsc->args.no_cache || !lv_image_cache_is_enabled()) lv_draw_buf_destroy(const_cast<lv_draw_buf_t *>(dsc->decoded));
}

// Decodes the built-in red picture and returns the first pixel (RGB565), or -1.
int red_pixel()
{
    const uint8_t *in = kRedJpeg;
    const char *why = "";
    lv_draw_buf_t *b = decode(in, sizeof kRedJpeg, &why);
    if (!b) {
        ESP_LOGW(TAG, "self-test decode failed: %s", why);
        return -1;
    }
    const int px = *reinterpret_cast<const uint16_t *>(b->data);
    lv_draw_buf_destroy(b);
    return px;
}

}  // namespace

bool start()
{
    if (g_usable) return true;
    jpeg_decode_engine_cfg_t ecfg = {};
    ecfg.intr_priority = 0;
    ecfg.timeout_ms = 500;
    const esp_err_t e = jpeg_new_decoder_engine(&ecfg, &g_engine);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "engine start failed: %s", esp_err_to_name(e));
        snprintf(g_last_error, sizeof g_last_error, "engine start: %s", esp_err_to_name(e));
        return false;
    }
    g_handlers = *lv_draw_buf_get_image_handlers();
    g_handlers.buf_malloc_cb = dec_malloc;
    g_handlers.buf_free_cb = dec_free;
    g_handlers.buf_copy_cb = nullptr;  // never copied: the cache holds the one buffer
    // Colour order self-test: a red picture must read back as 0xF800 (LVGL RGB565). If it reads as blue, flip.
    int px = red_pixel();
    if (px == 0x001F) {
        g_order = g_order == JPEG_DEC_RGB_ELEMENT_ORDER_BGR ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
        px = red_pixel();
    }
    if (px != 0xF800) {
        ESP_LOGE(TAG, "self-test: red picture read back as 0x%04X, hardware JPEG not used", px < 0 ? 0u : static_cast<unsigned>(px));
        snprintf(g_last_error, sizeof g_last_error, "self-test pixel 0x%04X", px < 0 ? 0u : static_cast<unsigned>(px));
        return false;
    }
    g_usable = true;
    lv_image_decoder_t *dec = lv_image_decoder_create();
    lv_image_decoder_set_info_cb(dec, decoder_info);
    lv_image_decoder_set_open_cb(dec, decoder_open);
    lv_image_decoder_set_close_cb(dec, decoder_close);
    ESP_LOGI(TAG, "hardware JPEG decoder ready (colour order %s)", g_order == JPEG_DEC_RGB_ELEMENT_ORDER_BGR ? "BGR" : "RGB");
    return true;
}

bool usable() { return g_usable; }

std::string diag_lines()
{
    char b[256];
    snprintf(b, sizeof b, "jpeg_state=%s\njpeg_decodes=%u\njpeg_fails=%u\njpeg_last_ms=%u\njpeg_last_size=%ux%u\njpeg_last_error=%s\n",
             g_usable ? "ready" : "off", g_decodes, g_fails, g_last_ms, g_last_w, g_last_h, g_last_error[0] ? g_last_error : "none");
    return b;
}

}  // namespace summit::jpeg_hw
