// UNTESTED ON HARDWARE. Assumptions to verify in Phase 1 (docs/HARDWARE_TODO.md):
// SDMMC slot 0 on its default ESP32-P4 pins, 4-bit, powered from on-chip LDO channel 4.
#include "summit/sd_card.h"

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

namespace summit {

namespace {

const char *TAG = "sd_card";
const char *kMount = "/sdcard";
constexpr int kLdoChannel = 4;
constexpr uint32_t kPollMs = 3000;

sdmmc_card_t *g_card = nullptr;
sd_pwr_ctrl_handle_t g_pwr = nullptr;
PosixStorage *g_storage = nullptr;
StateStore *g_state = nullptr;

int64_t free_mb()
{
    uint64_t total = 0, free_bytes = 0;
    if (esp_vfs_fat_info(kMount, &total, &free_bytes) != ESP_OK) return 0;
    return static_cast<int64_t>(free_bytes / (1024 * 1024));
}

bool mount()
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.pwr_ctrl_handle = g_pwr;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    esp_vfs_fat_sdmmc_mount_config_t cfg = {};
    cfg.format_if_mount_failed = false;  // never format the user's card
    cfg.max_files = 6;
    cfg.allocation_unit_size = 16 * 1024;
    return esp_vfs_fat_sdmmc_mount(kMount, &host, &slot, &cfg, &g_card) == ESP_OK;
}

void task(void *)
{
    bool mounted = false;
    int failures = 0;  // consecutive failed mounts. No card in the slot is normal, so the retries back off:
                       // every failed attempt touches the SDMMC controller that the Wi-Fi link to the C6 also uses.
    for (;;) {
        if (mounted && sdmmc_get_status(g_card) != ESP_OK) {
            ESP_LOGW(TAG, "card removed");
            esp_vfs_fat_sdcard_unmount(kMount, g_card);
            g_card = nullptr;
            mounted = false;
        } else if (!mounted && mount()) {
            ESP_LOGI(TAG, "card mounted");
            mounted = true;
            failures = 0;
        } else if (!mounted) {
            if (failures == 0) ESP_LOGW(TAG, "no SD card (checking again every few seconds, then every minute)");
            if (failures < 100) ++failures;
        }
        g_storage->set_present(mounted);
        publish_storage_state(*g_storage, *g_state);
        const uint32_t wait_ms = mounted || failures < 3 ? kPollMs : (failures < 6 ? 15000u : 60000u);
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

}  // namespace

Storage &sd_card_start(StateStore &state)
{
    if (g_storage) return *g_storage;
    g_state = &state;
    g_storage = new PosixStorage(kMount, free_mb);
    g_storage->set_present(false);
    sd_pwr_ctrl_ldo_config_t ldo = {};
    ldo.ldo_chan_id = kLdoChannel;
    if (sd_pwr_ctrl_new_on_chip_ldo(&ldo, &g_pwr) != ESP_OK) ESP_LOGW(TAG, "SD power LDO not available");
    publish_storage_state(*g_storage, state);
    xTaskCreate(task, "sd_card", 8192, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    return *g_storage;
}

}  // namespace summit
