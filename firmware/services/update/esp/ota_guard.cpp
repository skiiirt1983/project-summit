// Post-boot health gate task (pattern: LyricFlash main/ota_guard.c).
// Runs only on a pending image: 3 healthy samples 2.5 s apart within 40 s
// confirm it; otherwise it is marked invalid and the previous slot boots.
// No internet required. Probes: UI heartbeat, frames drawn, heap, touch, I2C, settings.
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "summit/update/device.h"
#include "summit/update/health_gate.h"

namespace summit::update {

namespace {

const char *TAG = "ota_guard";

struct GuardCtx {
    HealthProbes probes;
    EventBus *bus;
    StateStore *state;
};

void rollback(const char *reason)
{
    ESP_LOGE(TAG, "rejecting new image: %s", reason);
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGE(TAG, "rollback unavailable: %s", esp_err_to_name(err));
}

void guard_task(void *arg)
{
    GuardCtx *ctx = static_cast<GuardCtx *>(arg);
    HealthGate gate;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HealthGate::kSampleMs));
        if (!ota_pending_verify()) break;  // handled elsewhere
        HealthSample s;
        const HealthProbes &p = ctx->probes;
        s.ui_tick_age_ms = p.ui_tick_age_ms ? p.ui_tick_age_ms() : UINT32_MAX;
        s.frames_rendered = p.frames_rendered ? p.frames_rendered() : 0;
        s.free_heap = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        s.touch_ok = p.touch_ok && p.touch_ok();
        s.i2c_ok = p.i2c_ok && p.i2c_ok();
        s.settings_ok = p.settings_ok;
        GateDecision d = gate.feed(s, static_cast<uint32_t>(esp_timer_get_time() / 1000));
        if (d == GateDecision::Confirm) {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                ESP_LOGI(TAG, "new image confirmed");
                ctx->bus->publish("notify/toast",
                                  std::string("Updated to ") + esp_app_get_description()->version);
            }
            break;
        }
        if (d == GateDecision::Rollback) {
            rollback("post-boot health check failed");
            break;
        }
    }
    delete ctx;
    vTaskDelete(nullptr);
}

}  // namespace

bool ota_pending_verify()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    return running && esp_ota_get_state_partition(running, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
}

void ota_init_failed(const char *reason)
{
    if (ota_pending_verify()) rollback(reason);
    else ESP_LOGE(TAG, "init failed on a confirmed image: %s (USB recovery may be needed)", reason);
}

bool ota_guard_start(HealthProbes probes, EventBus &bus, StateStore &state)
{
    if (!ota_pending_verify()) return true;  // known-good boot: no task
    auto *ctx = new GuardCtx{std::move(probes), &bus, &state};
    if (xTaskCreate(guard_task, "ota_guard", 4096, ctx, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
        delete ctx;
        ota_init_failed("cannot start the health gate");
        return false;
    }
    return true;
}

}  // namespace summit::update
