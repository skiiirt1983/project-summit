#include "summit/audio_esp.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace summit {

namespace {

const char *TAG = "audio";

constexpr gpio_num_t kMclk = GPIO_NUM_13, kBclk = GPIO_NUM_12, kWs = GPIO_NUM_10, kDout = GPIO_NUM_9;
constexpr int kAmpEnable = 53;

// Codec + I2S are created once, on the first sound; the codec is opened and closed around each burst of sounds so the
// amplifier is off (no hiss, no idle current) the rest of the time.
class Es8311Port : public AudioPort {
public:
    explicit Es8311Port(void *i2c_bus) : i2c_(i2c_bus) {}

    bool open(int sample_rate) override
    {
        if (!ready_ && !init()) return false;
        esp_codec_dev_sample_info_t fs = {};
        fs.bits_per_sample = 16;
        fs.channel = 1;
        fs.channel_mask = 0;
        fs.sample_rate = static_cast<uint32_t>(sample_rate);
        const int rc = esp_codec_dev_open(dev_, &fs);
        if (rc != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "codec open failed: %d", rc);
            return false;
        }
        ESP_LOGI(TAG, "speaker on (%d Hz)", sample_rate);
        return true;
    }

    void set_volume(int percent) override
    {
        const int rc = esp_codec_dev_set_out_vol(dev_, percent);
        if (rc != ESP_CODEC_DEV_OK) ESP_LOGW(TAG, "set volume %d%%: %d", percent, rc);
    }

    bool write(const int16_t *samples, size_t count) override
    {
        const int rc = esp_codec_dev_write(dev_, const_cast<int16_t *>(samples), static_cast<int>(count * sizeof(int16_t)));
        if (rc != ESP_CODEC_DEV_OK) ESP_LOGW(TAG, "write: %d", rc);
        return rc == ESP_CODEC_DEV_OK;
    }

    void close() override
    {
        esp_codec_dev_close(dev_);
        ESP_LOGI(TAG, "speaker off");
    }

private:
    bool init()
    {
        // I2S transmit channel; esp_codec_dev reconfigures the clock and slot for the format it opens with.
        i2s_chan_config_t cc = {};
        cc.id = I2S_NUM_0;
        cc.role = I2S_ROLE_MASTER;
        cc.dma_desc_num = 6;
        cc.dma_frame_num = 240;
        cc.auto_clear = true;
        i2s_chan_handle_t tx = nullptr;
        esp_err_t err = i2s_new_channel(&cc, &tx, nullptr);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2s channel: %s", esp_err_to_name(err));
            return false;
        }
        i2s_std_config_t sc = {};
        sc.clk_cfg.sample_rate_hz = kAudioSampleRate;
        sc.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
        sc.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        sc.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
        sc.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
        sc.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
        sc.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
        sc.slot_cfg.ws_width = 16;
        sc.slot_cfg.ws_pol = false;
        sc.slot_cfg.bit_shift = true;
        sc.slot_cfg.left_align = true;
        sc.slot_cfg.big_endian = false;
        sc.slot_cfg.bit_order_lsb = false;
        sc.gpio_cfg.mclk = kMclk;
        sc.gpio_cfg.bclk = kBclk;
        sc.gpio_cfg.ws = kWs;
        sc.gpio_cfg.dout = kDout;
        sc.gpio_cfg.din = I2S_GPIO_UNUSED;
        err = i2s_channel_init_std_mode(tx, &sc);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2s init: %s", esp_err_to_name(err));
            return false;
        }

        audio_codec_i2s_cfg_t i2s_cfg = {};
        i2s_cfg.port = I2S_NUM_0;
        i2s_cfg.tx_handle = tx;
        const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

        audio_codec_i2c_cfg_t i2c_cfg = {};
        i2c_cfg.port = 0;
        i2c_cfg.addr = ES8311_CODEC_DEFAULT_ADDR;  // 0x30 = 7-bit 0x18 shifted, as the library wants it
        i2c_cfg.bus_handle = i2c_;
        const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

        es8311_codec_cfg_t cfg = {};
        cfg.ctrl_if = ctrl_if;
        cfg.gpio_if = gpio_if;
        cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
        cfg.pa_pin = kAmpEnable;
        cfg.pa_reverted = false;
        cfg.use_mclk = true;
        cfg.hw_gain.pa_voltage = 5.0f;
        cfg.hw_gain.codec_dac_voltage = 3.3f;
        const audio_codec_if_t *codec_if = es8311_codec_new(&cfg);

        if (!data_if || !ctrl_if || !gpio_if || !codec_if) {
            ESP_LOGE(TAG, "codec interfaces failed (data %p, ctrl %p, gpio %p, codec %p)", (const void *)data_if,
                     (const void *)ctrl_if, (const void *)gpio_if, (const void *)codec_if);
            return false;
        }
        esp_codec_dev_cfg_t dc = {};
        dc.dev_type = ESP_CODEC_DEV_TYPE_OUT;
        dc.codec_if = codec_if;
        dc.data_if = data_if;
        dev_ = esp_codec_dev_new(&dc);
        if (!dev_) {
            ESP_LOGE(TAG, "esp_codec_dev_new failed");
            return false;
        }
        ready_ = true;
        ESP_LOGI(TAG, "ES8311 ready (I2C 0x18, amplifier GPIO%d)", kAmpEnable);
        return true;
    }

    void *i2c_;
    bool ready_ = false;
    esp_codec_dev_handle_t dev_ = nullptr;
};

}  // namespace

bool audio_service_start(EventBus &bus, StateStore &state, Settings &settings, void *i2c_bus)
{
    if (!i2c_bus) {
        ESP_LOGW(TAG, "no I2C bus: audio stays off");
        return false;
    }
    static Es8311Port port(i2c_bus);
    static AudioService service(bus, state, settings, port);
    const BaseType_t ok = xTaskCreate(
        [](void *) {
            for (;;) {
                service.tick(static_cast<uint64_t>(esp_timer_get_time() / 1000));
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        },
        "audio", 6144, nullptr, 4, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "audio task failed to start");
        return false;
    }
    ESP_LOGI(TAG, "audio service started (the codec is brought up on the first sound)");
    return true;
}

}  // namespace summit
