// Project Summit firmware entry point.
// Phase 0 step 1: scaffold only. Boot sequence arrives with the framework (step 3).
#include "esp_log.h"
#include "esp_app_desc.h"

static const char *TAG = "summit";

extern "C" void app_main(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "Project Summit %s booting", desc->version);
}
