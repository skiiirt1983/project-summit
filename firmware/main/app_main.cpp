// Project Summit firmware entry point.
// Phase 0: framework wiring (bus, state, settings, panel registry) and the update
// engine with its post-boot health gate. Shell and panels start once the display BSP lands.
#include <atomic>
#include <memory>
#include <string>
#include <variant>

#include "esp_app_desc.h"
#include "esp_core_dump.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "board_pins.h"
#include "demo_panels.h"
#include "hardware_id.h"
#include "nvs_settings_storage.h"
#include "summit/boot/splash.h"
#include "summit/bsp/bsp.h"
#include "summit/event_bus.h"
#include "summit/modules.h"
#include "summit/net/wifi_service.h"
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
#include "summit/web/log_capture.h"

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
    summit::web::log_capture_start();  // the web UI's Log tab shows everything printed from here on
    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "Project Summit %s (%s) booting", desc->version, SUMMIT_HARDWARE_ID);
    static const char *reset_name = "?";
    {
        const esp_reset_reason_t rr = esp_reset_reason();
        const char *why = rr == ESP_RST_POWERON ? "power on" : rr == ESP_RST_SW ? "software restart" :
                          rr == ESP_RST_PANIC ? "CRASH (panic)" : rr == ESP_RST_INT_WDT ? "interrupt watchdog" :
                          rr == ESP_RST_TASK_WDT ? "task watchdog" : rr == ESP_RST_WDT ? "other watchdog" :
                          rr == ESP_RST_BROWNOUT ? "brown-out (power dip)" : rr == ESP_RST_DEEPSLEEP ? "deep sleep wake" :
                          rr == ESP_RST_EXT ? "reset pin" : "other";
        reset_name = why;
        if (rr == ESP_RST_POWERON || rr == ESP_RST_SW) ESP_LOGI(TAG, "reset reason: %s (%d)", why, static_cast<int>(rr));
        else ESP_LOGW(TAG, "RESTARTED after: %s (%d)", why, static_cast<int>(rr));
        // After a crash the core dump in flash names the task and the code address that failed: paste these lines.
        if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT) {
            if (esp_core_dump_image_check() == ESP_OK) {
                auto *s = static_cast<esp_core_dump_summary_t *>(calloc(1, sizeof(esp_core_dump_summary_t)));
                if (s && esp_core_dump_get_summary(s) == ESP_OK) {
                    // RISC-V has no on-device backtrace: the task, the failing address and the registers are enough to
                    // find the cause with the build's ELF file (mcause 2 = illegal instruction, 5 = bad load,
                    // 7 = bad store; mtval is the bad address).
                    ESP_LOGW(TAG, "CRASH DETAILS: task '%s', pc=0x%08x, ra=0x%08x, sp=0x%08x", s->exc_task,
                             static_cast<unsigned>(s->exc_pc), static_cast<unsigned>(s->ex_info.ra),
                             static_cast<unsigned>(s->ex_info.sp));
                    ESP_LOGW(TAG, "CRASH DETAILS: mcause=%u mtval=0x%08x", static_cast<unsigned>(s->ex_info.mcause),
                             static_cast<unsigned>(s->ex_info.mtval));
                } else {
                    ESP_LOGW(TAG, "CRASH DETAILS: the saved crash dump could not be read");
                }
                free(s);
            } else {
                ESP_LOGW(TAG, "CRASH DETAILS: no crash dump saved");
            }
        }
    }

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
    // The gate samples every 2.5 s and needs 3 healthy samples in a row within 40 s of boot, so it starts
    // now and reads values the UI side fills in later (before the UI is up the samples simply count as unhealthy).
    static std::atomic<uint32_t> ui_last_tick_ms{0};
    static std::atomic<bool> ui_touch_ok{false};
    summit::update::HealthProbes probes;
    probes.ui_tick_age_ms = [] {
        const uint32_t last = ui_last_tick_ms.load();
        return last == 0 ? UINT32_MAX : static_cast<uint32_t>(esp_timer_get_time() / 1000) - last;
    };
    probes.frames_rendered = [] { return summit::bsp::frames_rendered(); };
    probes.touch_ok = [] { return ui_touch_ok.load(); };
    probes.i2c_scan = [] {
        auto devs = summit::update::board_i2c_devices();
        const uint8_t addr[4] = {0x5d, 0x18, 0x40, 0x19};  // same order as board_i2c_devices()
        for (size_t i = 0; i < devs.size() && i < 4; ++i) devs[i].present = summit::bsp::i2c_present(addr[i]);
        return devs;
    };
    probes.settings_ok = r != summit::LoadResult::InvalidSchema;
    summit::update::ota_guard_start(probes, bus, state);
    summit::update::update_service_start({SUMMIT_HARDWARE_ID, SUMMIT_CHIP_REV_FAMILY}, bus, state, settings);
    // Wi-Fi through the C6 (esp_hosted): runs in its own task and publishes net/* for the web UI and the strip.
    summit::net::wifi_service_start(bus, state);
    // Config portal; reachable once Wi-Fi is up.
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
    summit::bsp::set_orientation(hw.display, summit::kLandscape);  // the shell starts landscape, until the accelerometer service exists
    // The display slider (quick settings) sets the backlight; the saved value applies from the start.
    auto brightness_of = [](const summit::Value &v, int fallback) {
        const int64_t *i = std::get_if<int64_t>(&v);
        return i ? static_cast<int>(*i) : fallback;
    };
    const int start_brightness = brightness_of(settings.get("display/brightness"), 80);
    auto apply_brightness = [brightness_of](const std::string &, const summit::Value &v) {
        summit::bsp::set_brightness(brightness_of(v, 80));
    };
    bus.subscribe("settings/display/brightness", apply_brightness);  // saved value (web UI, release)
    bus.subscribe("ui/brightness_live", apply_brightness);           // while the slider is dragged
    sim::publish_placeholders(state, false);                              // fake time, weather, battery... (placeholders)
    static summit::shell::Shell shell(
        summit::shell::Shell::Deps{bus, state, settings, registry, host, &modules},
        [](summit::Orientation o) { summit::bsp::set_orientation(hw.display, o); });
    auto now_ms = [] { return static_cast<uint32_t>(esp_timer_get_time() / 1000); };
    std::string ui_error;
    if (!shell.start(now_ms(), &ui_error)) {
        ESP_LOGE(TAG, "shell start failed: %s", ui_error.c_str());
        return;
    }
    ui_touch_ok = hw.touch != nullptr;
    if (hw.touch) shell.attach_input(hw.touch);
    else ESP_LOGW(TAG, "no touch controller: the UI is display-only");
    // Boot screen over the finished shell: at least 2 s, and until start-up is ready (nothing else to
    // wait for yet; later, Wi-Fi and the first data fetch can set_status() and delay set_ready()).
    static summit::boot::Splash splash;
    // The boot screen is portrait (the panel's native orientation) unless the accelerometer reads landscape.
    const summit::Value sensor = state.get("sensor/orientation");
    const std::string *sensor_o = std::get_if<std::string>(&sensor);
    const bool boot_portrait = !(sensor_o && *sensor_o == "landscape");
    if (hw.touch) lv_indev_enable(hw.touch, false);    // no touch (or phantom touch) during the boot screen
    splash.show(now_ms(), hw.display, boot_portrait);
    splash.set_ready();
    lv_timer_handler();                                // draw the first frame, then light the panel
    summit::bsp::set_brightness(start_brightness);
    ESP_LOGI(TAG, "UI running");
    // PowerService::tick() is deliberately not called yet: deep sleep is untested and the device would
    // sleep after 5 idle minutes. Timers and the shell run.
    // The housekeeping (event bus, timers, shell, splash) runs every 4 ms while something moves (a finger down or
    // an animation) and every 20 ms otherwise; it used to run every millisecond, which cost a lot of CPU at idle.
    // LVGL's own timers still run every pass, and the loop sleeps until the next one is due.
    // Diagnostics (web UI Diagnostics tab + one "diag:" log line every 30 s), so nobody has to paste long logs.
    static std::atomic<uint32_t> redraws{0};
    lv_display_add_event_cb(hw.display, [](lv_event_t *) { redraws.fetch_add(1, std::memory_order_relaxed); },
                            LV_EVENT_REFR_START, nullptr);
    uint32_t next_services = 0, next_report = now_ms() + 30000, next_metrics = now_ms() + 5000;
    uint32_t sec_start = now_ms(), sec_redraws = 0, sec_strips = 0, fps_peak = 0, strips_peak = 0, worst_ms = 0;
    for (;;) {
        const uint32_t now = now_ms();
        ui_last_tick_ms = now ? now : 1;
        if (static_cast<int32_t>(now - next_services) >= 0) {
            bus.dispatch();
            timers.tick(now);
            shell.tick(now);
            if (splash.tick(now) && hw.touch) lv_indev_enable(hw.touch, true);
            const bool moving = lv_anim_count_running() > 0 || (hw.touch && lv_indev_get_state(hw.touch) == LV_INDEV_STATE_PRESSED);
            next_services = now + (moving ? 4 : 20);
        }
        const uint32_t t0 = now_ms();
        uint32_t wait = lv_timer_handler();
        const uint32_t took = now_ms() - t0;
        if (took > worst_ms) worst_ms = took;
        if (now - sec_start >= 1000) {  // peak redraws/s and panel strips/s over the window = frame rate while moving
            const uint32_t r = redraws.load(), f = summit::bsp::frames_rendered();
            const uint32_t per_s = (r - sec_redraws) * 1000 / (now - sec_start);
            if (per_s > fps_peak) fps_peak = per_s;
            const uint32_t strips = (f - sec_strips) * 1000 / (now - sec_start);
            if (strips > strips_peak) strips_peak = strips;
            sec_redraws = r;
            sec_strips = f;
            sec_start = now;
        }
        if (static_cast<int32_t>(now - next_metrics) >= 0) {
            next_metrics = now + 5000;
            char m[640];
            snprintf(m, sizeof m,
                     "lvgl_busy_pct=%u\nfps_peak_5s=%u\npanel_strips_per_s_peak=%u\nworst_lvgl_pass_ms=%u\n"
                     "heap_internal_kb=%u\nheap_internal_lowest_kb=%u\npsram_free_kb=%u\nui_pixel_clock_mhz=40\n"
                     "uptime_s=%u\npending_verify=%s\nreset=%s\n",
                     100u - lv_timer_get_idle(), fps_peak, strips_peak, worst_ms,
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                     static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), now / 1000,
                     summit::update::ota_pending_verify() ? "yes" : "no", reset_name);
            state.set("diag/metrics", std::string(m));
            fps_peak = strips_peak = worst_ms = 0;
        }
        if (wait < 1) wait = 1;
        if (wait > 10) wait = 10;
        vTaskDelay(pdMS_TO_TICKS(wait));  // at least one tick: lets the idle task run and feed the watchdog
        if (static_cast<int32_t>(now - next_report) >= 0) {
            next_report = now + 30000;
            char m[640];
            const summit::Value mv = state.get("diag/metrics");
            snprintf(m, sizeof m, "%s", std::get_if<std::string>(&mv) ? std::get_if<std::string>(&mv)->c_str() : "");
            std::string md(m);
            for (char &c : md) if (c == '\n') c = ' ';
            ESP_LOGI(TAG, "diag: %s", md.c_str());
        }
    }
}
