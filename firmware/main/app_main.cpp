// Project Summit firmware entry point.
// Phase 0: framework wiring (bus, state, settings, panel registry) and the update
// engine with its post-boot health gate. Shell and panels start once the display BSP lands.
#include <memory>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board_pins.h"
#include "hardware_id.h"
#include "nvs_settings_storage.h"
#include "summit/event_bus.h"
#include "summit/modules.h"
#include "summit/recipes/module.h"
#include "summit/recipes/panel.h"
#include "summit/recipes/store.h"
#include "summit/recipes/url_fetch.h"
#include "summit/recipes/web_api.h"
#include "summit/registry.h"
#include "summit/settings.h"
#include "summit/sd_card.h"
#include "summit/state_store.h"
#include "summit/system_settings.h"
#include "summit/timers.h"
#include "summit/update/device.h"
#include "summit/web/device.h"

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
    // microSD card (untested on hardware): publishes storage/sd_present, storage/sd_free_mb.
    static summit::Storage &sd = summit::sd_card_start(state);
    static summit::recipes::RecipeStore recipe_store(sd);
    static summit::recipes::RecipeWebApi recipe_api(recipe_store, sd);
    static summit::recipes::HttpsPageFetcher page_fetcher;  // untested on hardware
    recipe_api.set_fetcher(&page_fetcher);
    registry.add(std::make_unique<summit::recipes::RecipesPanel>(recipe_store));

    // Modules ship in the firmware but start uninstalled (ARCHITECTURE.md "Modules").
    static summit::ModuleManager modules;
    modules.add(summit::recipes::module_manifest());
    static summit::recipes::RecipeModuleData recipe_data(recipe_store, state);
    modules.set_data("recipes", &recipe_data);
    for (const auto &d : registry.setting_defs()) schema.defs.push_back(d);
    for (const auto &d : modules.setting_defs()) schema.defs.push_back(d);

    static NvsSettingsStorage storage;
    static summit::Settings settings(schema, storage, &bus);
    summit::LoadResult r = settings.load();
    ESP_LOGI(TAG, "settings schema v%u, stored v%u: %s", (unsigned)settings.schema_version(),
             (unsigned)settings.stored_version(), load_result_name(r));

    // The shell (step 4) and web UI (step 6) show this until the user updates or resets.
    state.set("system/settings_notice", std::string(summit::notice_message(settings.notice())));
    if (settings.notice() != summit::SettingsNotice::None)
        ESP_LOGW(TAG, "%s", summit::notice_message(settings.notice()));

    // Post-boot health gate (only runs on a newly installed, pending image) and the
    // update service. UI, touch and I2C probes arrive with the display/touch BSP
    // (Phase 1); until then a pending OTA image fails the gate and rolls back.
    // Fail-fast: a firmware bug on a new image rolls back at once instead of waiting 40 s.
    if (r == summit::LoadResult::InvalidSchema) summit::update::ota_init_failed("settings schema invalid");
    summit::update::HealthProbes probes;
    probes.settings_ok = r != summit::LoadResult::InvalidSchema;
    summit::update::ota_guard_start(probes, bus, state);
    summit::update::update_service_start({SUMMIT_HARDWARE_ID, SUMMIT_CHIP_REV_FAMILY}, bus, state, settings);
    // Config portal; reachable once Wi-Fi is up (network service, Phase 1).
    summit::web::web_server_start(bus, state, settings, &modules, &recipe_api, &sd);

    // Kitchen timers. They are ticked from the UI loop, which starts with the display BSP (Phase 1).
    static summit::TimerService timers(bus, state);

    static summit::PanelHost host(registry, bus, state, settings);
    state.set("system/hardware_id", std::string(SUMMIT_HARDWARE_ID));
    state.set("system/firmware_version", std::string(desc->version));
    bus.dispatch();
}
