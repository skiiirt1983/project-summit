// Project Summit firmware entry point.
// Phase 0: framework wiring (bus, state, settings, panel registry) and the update
// engine with its post-boot health gate. Shell and panels start once the display BSP lands.
#include <memory>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "board_pins.h"
#include "demo_panels.h"
#include "hardware_id.h"
#include "nvs_settings_storage.h"
#include "summit/bsp/bsp.h"
#include "summit/event_bus.h"
#include "summit/modules.h"
#include "summit/recipes/module.h"
#include "summit/recipes/panel.h"
#include "summit/recipes/store.h"
#include "summit/recipes/url_fetch.h"
#include "summit/recipes/web_api.h"
#include "summit/registry.h"
#include "summit/settings.h"
#include "summit/shell/shell.h"
#include "summit/sd_card.h"
#include "summit/state_store.h"
#include "summit/system_settings.h"
#include "summit/deep_sleep.h"
#include "summit/power_service.h"
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
    recipe_api.set_bus(&bus);
    // First-light placeholder panels (home, calendar, weather, news, printer), shared with the simulator.
    sim::register_demo_panels(registry);
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

    // Sleep (D-012). The rules are host-tested; deep sleep itself is untested on hardware.
    // PowerService::tick() runs from the UI loop (Phase 1), so nothing sleeps before then.
    static summit::DeepSleepPort sleep_port(summit::board::kAccelInt1Gpio);
    static summit::PowerService power(bus, state, settings, timers, sleep_port);
    {
        const summit::BootWake wake = summit::read_boot_wake();
        power.on_wake(wake.cause, wake.blob.data(), wake.blob.size(), wake.slept_ms, 0);
    }

    static summit::PanelHost host(registry, bus, state, settings);
    state.set("system/hardware_id", std::string(SUMMIT_HARDWARE_ID));
    state.set("system/firmware_version", std::string(desc->version));
    bus.dispatch();

    // ---- Display, touch and the UI loop (Phase 1, first light; UNTESTED on hardware) ----
    // The loop runs here, so the main task stack is raised in sdkconfig.defaults. Nothing but this task
    // touches LVGL; other tasks only use the (thread-safe) event bus.
    static summit::bsp::Handles hw;
    if (!summit::bsp::start(hw)) {
        ESP_LOGE(TAG, "display/touch failed to start: the update engine and web server keep running without a UI");
        return;
    }
    summit::bsp::set_orientation(hw.display, summit::kLandscape);  // until the accelerometer service exists
    sim::publish_placeholders(state);                              // fake time, weather, battery... (placeholders)
    static summit::shell::Shell shell(
        summit::shell::Shell::Deps{bus, state, settings, registry, host, &modules},
        [](summit::Orientation o) { summit::bsp::set_orientation(hw.display, o); });
    auto now_ms = [] { return static_cast<uint32_t>(esp_timer_get_time() / 1000); };
    std::string ui_error;
    if (!shell.start(now_ms(), &ui_error)) {
        ESP_LOGE(TAG, "shell start failed: %s", ui_error.c_str());
        return;
    }
    if (hw.touch) shell.attach_input(hw.touch);
    else ESP_LOGW(TAG, "no touch controller: the UI is display-only");
    lv_timer_handler();                                // draw the first frame, then light the panel
    summit::bsp::set_brightness(80);
    ESP_LOGI(TAG, "UI running");
    // PowerService::tick() is deliberately not called yet: deep sleep is untested and the device would
    // sleep after 5 idle minutes. Timers and the shell run.
    for (;;) {
        const uint32_t now = now_ms();
        bus.dispatch();
        timers.tick(now);
        shell.tick(now);
        lv_timer_handler();
        vTaskDelay(1);  // one tick (1 ms at 1000 Hz): lets the idle task run and feed the watchdog
    }
}
