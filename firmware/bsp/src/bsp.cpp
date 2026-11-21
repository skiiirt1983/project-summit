// See bsp.h. Pins and timings come from Waveshare's BSP for this board (waveshare/esp32_p4_wifi6_touch_lcd_5
// v1.0.4, Apache-2.0): backlight GPIO26 (plain PWM, not an I2C controller as on the DB board), panel reset
// GPIO27 (active high), touch reset/interrupt not connected, shared I2C SDA 7 / SCL 8, MIPI DPHY on LDO
// channel 3 at 2.5 V, 2 lanes at 700 Mbit/s. Not yet run on hardware.
#include "summit/bsp/bsp.h"

#include <cstdlib>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_hx8394.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace summit::bsp {

namespace {

const char *TAG = "bsp";

constexpr int kHRes = 720, kVRes = 1280;
constexpr gpio_num_t kSda = GPIO_NUM_7, kScl = GPIO_NUM_8;
constexpr gpio_num_t kBacklight = GPIO_NUM_26, kLcdReset = GPIO_NUM_27;
constexpr int kDphyLdoChan = 3, kDphyLdoMv = 2500;
constexpr int kLaneMbps = 700, kLanes = 2;
constexpr int kBufLines = 80;  // partial render buffer height; two buffers in PSRAM
constexpr ledc_channel_t kLedcChannel = LEDC_CHANNEL_1;
constexpr ledc_timer_t kLedcTimer = LEDC_TIMER_1;

esp_lcd_panel_handle_t g_panel = nullptr;
esp_lcd_touch_handle_t g_touch = nullptr;
void *g_rotate_buf = nullptr;
bool g_backlight_ready = false;

bool backlight_init()
{
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = kLedcTimer;
    timer.freq_hz = 5000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_channel_config_t ch = {};
    ch.gpio_num = kBacklight;
    ch.speed_mode = LEDC_LOW_SPEED_MODE;
    ch.channel = kLedcChannel;
    ch.intr_type = LEDC_INTR_DISABLE;
    ch.timer_sel = kLedcTimer;
    ch.duty = 0;
    ch.hpoint = 0;
    if (ledc_timer_config(&timer) != ESP_OK || ledc_channel_config(&ch) != ESP_OK) return false;
    g_backlight_ready = true;
    return true;
}

bool panel_init()
{
    // The MIPI DPHY is powered from on-chip LDO channel 3.
    static esp_ldo_channel_handle_t phy_ldo = nullptr;
    esp_ldo_channel_config_t ldo = {};
    ldo.chan_id = kDphyLdoChan;
    ldo.voltage_mv = kDphyLdoMv;
    if (esp_ldo_acquire_channel(&ldo, &phy_ldo) != ESP_OK) {
        ESP_LOGE(TAG, "DSI PHY LDO failed");
        return false;
    }
    esp_lcd_dsi_bus_handle_t dsi = nullptr;
    esp_lcd_dsi_bus_config_t bus = {};
    bus.bus_id = 0;
    bus.num_data_lanes = kLanes;
    bus.phy_clk_src = static_cast<mipi_dsi_phy_clock_source_t>(0);
    bus.lane_bit_rate_mbps = kLaneMbps;
    if (esp_lcd_new_dsi_bus(&bus, &dsi) != ESP_OK) {
        ESP_LOGE(TAG, "DSI bus failed");
        return false;
    }
    esp_lcd_panel_io_handle_t io = nullptr;
    esp_lcd_dbi_io_config_t dbi = {};
    dbi.virtual_channel = 0;
    dbi.lcd_cmd_bits = 8;
    dbi.lcd_param_bits = 8;
    if (esp_lcd_new_panel_io_dbi(dsi, &dbi, &io) != ESP_OK) {
        ESP_LOGE(TAG, "DBI io failed");
        return false;
    }
    // The values of HX8394_720_1280_PANEL_30HZ_DPI_CONFIG() (waveshare/esp_lcd_hx8394 2.1.0, ESP-IDF 5.x
    // field names), set by name: the vendor macro is C-only and does not compile as C++.
    esp_lcd_dpi_panel_config_t dpi = {};
    dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi.dpi_clock_freq_mhz = 58;
    dpi.virtual_channel = 0;
    dpi.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
    dpi.num_fbs = 1;
    dpi.video_timing.h_size = kHRes;
    dpi.video_timing.v_size = kVRes;
    dpi.video_timing.hsync_back_porch = 20;
    dpi.video_timing.hsync_pulse_width = 20;
    dpi.video_timing.hsync_front_porch = 40;
    dpi.video_timing.vsync_back_porch = 10;
    dpi.video_timing.vsync_pulse_width = 4;
    dpi.video_timing.vsync_front_porch = 24;
    dpi.flags.use_dma2d = true;
    hx8394_vendor_config_t vendor = {};
    vendor.mipi_config.dsi_bus = dsi;
    vendor.mipi_config.dpi_config = &dpi;
    vendor.mipi_config.lane_num = kLanes;
    esp_lcd_panel_dev_config_t dev = {};
    dev.bits_per_pixel = 16;
    dev.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    dev.reset_gpio_num = kLcdReset;
    dev.vendor_config = &vendor;
    dev.flags.reset_active_high = 1;
    if (esp_lcd_new_panel_hx8394(io, &dev, &g_panel) != ESP_OK || esp_lcd_panel_reset(g_panel) != ESP_OK ||
        esp_lcd_panel_init(g_panel) != ESP_OK) {
        ESP_LOGE(TAG, "HX8394 panel failed");
        return false;
    }
    ESP_LOGI(TAG, "display up: %dx%d RGB565, %d lanes @ %d Mbit/s", kHRes, kVRes, kLanes, kLaneMbps);
    return true;
}

bool touch_init(i2c_master_bus_handle_t i2c)
{
    esp_lcd_touch_config_t cfg = {};
    cfg.x_max = kHRes;
    cfg.y_max = kVRes;
    cfg.rst_gpio_num = GPIO_NUM_NC;  // not connected on this board
    cfg.int_gpio_num = GPIO_NUM_NC;
    // The LCD reset pin is shared with the touch chip, so the GT911 restarts when the panel is reset and is
    // not answering for a while (first run: found by the bus scan before the panel reset, gone right after).
    // Two possible I2C addresses depending on how it sampled its reset pin; retry for about 3 s.
    for (int attempt = 0; attempt < 15; ++attempt) {
        if (attempt) vTaskDelay(pdMS_TO_TICKS(200));
        uint8_t addr = 0;
        for (uint8_t a : {static_cast<uint8_t>(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS),
                          static_cast<uint8_t>(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP)})
            if (addr == 0 && i2c_master_probe(i2c, a, 50) == ESP_OK) addr = a;
        if (addr == 0) continue;
        // The values of ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(), set by name: that macro's initialiser order is
        // not valid C++ (it failed to compile).
        esp_lcd_panel_io_i2c_config_t io_cfg = {};
        io_cfg.dev_addr = addr;
        io_cfg.control_phase_bytes = 1;
        io_cfg.dc_bit_offset = 0;
        io_cfg.lcd_cmd_bits = 16;
        io_cfg.flags.disable_control_phase = 1;
        io_cfg.scl_speed_hz = 400000;
        esp_lcd_panel_io_handle_t io = nullptr;
        if (esp_lcd_new_panel_io_i2c(i2c, &io_cfg, &io) != ESP_OK) return false;
        if (esp_lcd_touch_new_i2c_gt911(io, &cfg, &g_touch) != ESP_OK) return false;
        ESP_LOGI(TAG, "GT911 touch found at 0x%02x (attempt %d)", addr, attempt + 1);
        return true;
    }
    ESP_LOGE(TAG, "GT911 touch not found on I2C (0x5d / 0x14) after 15 tries");
    return false;
}

uint32_t tick_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    // LVGL renders in the rotated (logical) orientation; the panel is always portrait. Rotate the
    // finished strip into the panel's orientation first (LVGL's documented software-rotation pattern).
    const lv_display_rotation_t rot = lv_display_get_rotation(disp);
    lv_area_t out = *area;
    if (rot != LV_DISPLAY_ROTATION_0 && g_rotate_buf) {
        const lv_color_format_t cf = lv_display_get_color_format(disp);
        lv_display_rotate_area(disp, &out);
        const int32_t src_w = lv_area_get_width(area), src_h = lv_area_get_height(area);
        const uint32_t src_stride = lv_draw_buf_width_to_stride(src_w, cf);
        const uint32_t dst_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&out), cf);
        lv_draw_sw_rotate(px, g_rotate_buf, src_w, src_h, src_stride, dst_stride, rot, cf);
        px = static_cast<uint8_t *>(g_rotate_buf);
    }
    // The copy into the frame buffer runs by DMA2D; the panel driver calls trans_done_cb when it has finished
    // and only then may LVGL reuse the buffer (flush_ready). Calling it here made the next strip start too
    // early ("previous draw operation is not finished", flicker).
    if (esp_lcd_panel_draw_bitmap(g_panel, out.x1, out.y1, out.x2 + 1, out.y2 + 1, px) != ESP_OK)
        lv_display_flush_ready(disp);  // not started: do not hang LVGL
}

bool trans_done_cb(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *user)
{
    lv_display_flush_ready(static_cast<lv_display_t *>(user));
    return false;  // no higher-priority task woken
}

void touch_read_cb(lv_indev_t *, lv_indev_data_t *data)
{
    uint16_t x[1], y[1];
    uint8_t n = 0;
    esp_lcd_touch_read_data(g_touch);
    if (esp_lcd_touch_get_coordinates(g_touch, x, y, nullptr, &n, 1) && n > 0) {
        data->point.x = x[0];
        data->point.y = y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

}  // namespace

bool start(Handles &out)
{
    if (!backlight_init()) {
        ESP_LOGE(TAG, "backlight PWM failed");
        return false;
    }
    set_brightness(0);

    i2c_master_bus_handle_t i2c = nullptr;
    i2c_master_bus_config_t i2c_cfg = {};
    i2c_cfg.i2c_port = I2C_NUM_0;
    i2c_cfg.sda_io_num = kSda;
    i2c_cfg.scl_io_num = kScl;
    i2c_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_cfg.glitch_ignore_cnt = 7;
    i2c_cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&i2c_cfg, &i2c) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus failed");
        return false;
    }
    // Everything on the shared bus, as a quick wiring check in the log.
    for (uint8_t a = 0x08; a < 0x78; ++a)
        if (i2c_master_probe(i2c, a, 20) == ESP_OK) ESP_LOGI(TAG, "I2C device at 0x%02x", a);

    if (!panel_init()) return false;

    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *disp = lv_display_create(kHRes, kVRes);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    const size_t buf_bytes = static_cast<size_t>(kHRes) * kBufLines * 2;
    void *b1 = heap_caps_aligned_alloc(64, buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *b2 = heap_caps_aligned_alloc(64, buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    g_rotate_buf = heap_caps_aligned_alloc(64, buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!b1 || !b2 || !g_rotate_buf) {
        ESP_LOGE(TAG, "no memory for the draw buffers (%u bytes each)", static_cast<unsigned>(buf_bytes));
        return false;
    }
    lv_display_set_buffers(disp, b1, b2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    out.display = disp;
    esp_lcd_dpi_panel_event_callbacks_t cbs = {};
    cbs.on_color_trans_done = trans_done_cb;
    if (esp_lcd_dpi_panel_register_event_callbacks(g_panel, &cbs, disp) != ESP_OK) {
        ESP_LOGE(TAG, "could not register the draw-done callback");
        return false;
    }

    if (touch_init(i2c)) {
        out.touch = lv_indev_create();
        lv_indev_set_type(out.touch, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(out.touch, touch_read_cb);
        lv_indev_set_display(out.touch, disp);
    }
    return true;
}

void set_brightness(int percent)
{
    if (!g_backlight_ready) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, kLedcChannel, static_cast<uint32_t>(1023 * percent / 100));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, kLedcChannel);
}

void set_orientation(lv_display_t *display, Orientation o)
{
    lv_display_set_rotation(display, o == kLandscape ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_0);
}

}  // namespace summit::bsp
