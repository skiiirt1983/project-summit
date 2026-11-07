// Project Summit firmware entry point.
// Phase 0 step 3: framework wiring (bus, state, settings, panel registry).
// Shell, services and panels arrive in later steps.
#include "esp_app_desc.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hardware_id.h"
#include "nvs_settings_storage.h"
#include "summit/event_bus.h"
#include "summit/registry.h"
#include "summit/settings.h"
#include "summit/state_store.h"
#include "summit/system_settings.h"

static const char *TAG = "summit";

static const char *load_result_name(summit::LoadResult r)
{
    switch (r) {
    case summit::LoadResult::Fresh: return "fresh";
    case summit::LoadResult::Current: return "current";
    case summit::LoadResult::Migrated: return "migrated";
    case summit::LoadResult::NewerSchema: return "newer schema (not written)";
    case summit::LoadResult::MigrationMissing: return "no migration (not written)";
    case summit::LoadResult::Unreadable: return "unreadable (defaults in memory)";
    case summit::LoadResult::InvalidSchema: return "invalid schema (firmware bug)";
    }
    return "?";
}

extern "C" void app_main(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "Project Summit %s (%s) booting", desc->version, SUMMIT_HARDWARE_ID);

    // NVS is never erased automatically: on failure settings run read-only on defaults.
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "nvs_flash_init: %s", esp_err_to_name(err));

    static summit::EventBus bus;
    static summit::StateStore state(bus);
    static summit::PanelRegistry registry;  // panels register here (later steps)

    summit::SettingsSchema schema = summit::system_settings_schema();
    for (const auto &d : registry.setting_defs()) schema.defs.push_back(d);

    static NvsSettingsStorage storage;
    static summit::Settings settings(schema, storage, &bus);
    summit::LoadResult r = settings.load();
    ESP_LOGI(TAG, "settings schema v%u, stored v%u: %s", (unsigned)settings.schema_version(),
             (unsigned)settings.stored_version(), load_result_name(r));

    // The shell (step 4) and web UI (step 6) show this until the user updates or resets.
    state.set("system/settings_notice", std::string(summit::notice_message(settings.notice())));
    if (settings.notice() != summit::SettingsNotice::None)
        ESP_LOGW(TAG, "%s", summit::notice_message(settings.notice()));

    static summit::PanelHost host(registry, bus, state, settings);
    state.set("system/hardware_id", std::string(SUMMIT_HARDWARE_ID));
    bus.dispatch();
}
