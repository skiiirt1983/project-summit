// Update service: one worker task runs checks and downloads through the shared
// UpdateEngine; the web UI upload path (step 6) uses the same engine under the
// same lock. Status is published to the state store (update/* topics).
#include <memory>
#include <mutex>
#include <string>

#include "catalog_json.h"
#include "esp_app_desc.h"
#include "esp_backends.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "summit/update/device.h"
#include "summit/update/status_topics.h"

namespace summit::update {

namespace {

const char *TAG = "update";

// Catalog locations per channel (ARCHITECTURE.md "Update channels"). Downloads are
// only allowed from these hosts, over HTTPS, without redirects.
const char *kCatalogStableBeta = "https://skiiirt1983.github.io/slopeform-home/catalog.json";
// Alpha builds are published to this (public) repository's GitHub Pages by .github/workflows/release.yml.
const char *kCatalogAlpha = "https://skiiirt1983.github.io/project-summit/catalog.json";
const char *kAllowedHost = "skiiirt1983.github.io";
constexpr size_t kMaxCatalogBytes = 64 * 1024;
constexpr int kHttpTimeoutMs = 15000;

struct Command {
    enum Kind : uint8_t { Check, Install, Previous, Reboot } kind;
    char version[32];
};

struct Service {
    EventBus *bus = nullptr;
    StateStore *state = nullptr;
    Settings *settings = nullptr;
    EspOtaWriter writer;
    MbedSha256 hasher;
    std::unique_ptr<UpdateEngine> engine;
    std::mutex mu;  // guards engine (worker task, bus handler, web upload)
    QueueHandle_t queue = nullptr;
};

Service *g = nullptr;

bool state_bool(const char *key)
{
    Value v = g->state->get(key);
    const bool *b = std::get_if<bool>(&v);
    return b && *b;
}

int64_t state_int(const char *key, int64_t fallback)
{
    Value v = g->state->get(key);
    const int64_t *i = std::get_if<int64_t>(&v);
    return i ? *i : fallback;
}

PreflightInput preflight_now()
{
    PreflightInput in;
    in.on_usb_power = state_bool("power/usb_power");
    in.battery_pct = static_cast<int>(state_int("power/battery_pct", 0));
    in.next_alarm_in_s = state_int("alarm/next_in_s", -1);
    in.wifi_connected = state_bool("net/wifi_connected");
    in.time_synced = state_bool("time/synced");
    Value timers = g->state->get("timers/list");
    const std::string *list = std::get_if<std::string>(&timers);
    in.timer_running = list && !list->empty();
    return in;
}

Channel channel_setting()
{
    Value v = g->settings->get("update/channel");
    const std::string *s = std::get_if<std::string>(&v);
    Channel c = Channel::Stable;
    if (s) parse_channel(*s, c);
    return c;
}

// mu held.
void publish_locked() { publish_status(*g->engine, *g->state); }

void publish()
{
    std::lock_guard<std::mutex> lock(g->mu);
    publish_locked();
}

// Streams an HTTPS GET. on_data returns false to stop. Returns HTTP status or -1.
template <typename F>
int http_get(const char *url, int64_t max_bytes, F on_data, std::string *error)
{
    if (url_host(url) != kAllowedHost) {
        *error = "download host not allowed";
        return -1;
    }
    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.timeout_ms = kHttpTimeoutMs;
    cfg.disable_auto_redirect = true;  // the allow-list applies to the final host too
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = 4096;
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        *error = "HTTP client init failed";
        return -1;
    }
    int status = -1;
    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
        *error = std::string("connect: ") + esp_err_to_name(err);  // DNS / TLS / socket
    } else {
        int64_t length = esp_http_client_fetch_headers(c);
        status = esp_http_client_get_status_code(c);
        if (status != 200) {
            *error = "HTTP " + std::to_string(status);
        } else if (length > max_bytes) {
            *error = "response too large";
            status = -1;
        } else {
            static uint8_t buf[4096];
            int64_t total = 0;
            for (;;) {
                int n = esp_http_client_read(c, reinterpret_cast<char *>(buf), sizeof buf);
                if (n < 0) {
                    *error = "read failed";
                    status = -1;
                    break;
                }
                if (n == 0) {
                    if (!esp_http_client_is_complete_data_received(c)) {
                        *error = "connection closed early";
                        status = -1;
                    }
                    break;
                }
                total += n;
                if (total > max_bytes) {
                    *error = "response too large";
                    status = -1;
                    break;
                }
                if (!on_data(buf, static_cast<size_t>(n))) {
                    status = -1;
                    break;
                }
            }
        }
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return status;
}

void do_check()
{
    {
        std::lock_guard<std::mutex> lock(g->mu);
        g->engine->set_channel(channel_setting());
        if (!g->engine->begin_check()) return;
        publish_locked();
    }
    std::string body, error;
    const char *url = channel_setting() == Channel::Alpha ? kCatalogAlpha : kCatalogStableBeta;
    int status = http_get(url, kMaxCatalogBytes,
                          [&](const uint8_t *p, size_t n) {
                              body.append(reinterpret_cast<const char *>(p), n);
                              return true;
                          },
                          &error);
    std::lock_guard<std::mutex> lock(g->mu);
    Catalog catalog;
    if (status != 200) {
        g->engine->check_failed(error, status > 0 ? status : 0);
    } else if (!parse_catalog_json(body, catalog, &error)) {
        g->engine->check_failed(error);
    } else {
        g->engine->catalog_received(catalog);
    }
    publish_locked();
    ESP_LOGI(TAG, "check: %s %s", stage_name(g->engine->status().stage), g->engine->status().error.c_str());
}

void do_install(const std::string &version)
{
    std::string url;
    {
        std::lock_guard<std::mutex> lock(g->mu);
        // The same entry the engine will install (first live bug: the last entry with this version won here, the
        // rev1 file, and was rejected by the rev3 image check: "different chip revision").
        if (const Candidate *c = g->engine->installable(version)) url = c->build.url;
        bool ok = g->engine->start_install(version, preflight_now());
        publish_locked();
        if (!ok) return;
    }
    std::string error;
    int64_t last_pct = -1;
    int status = http_get(url.c_str(), INT64_MAX,
                          [&](const uint8_t *p, size_t n) {
                              std::lock_guard<std::mutex> lock(g->mu);
                              bool ok = g->engine->data(p, n);
                              const Status &s = g->engine->status();
                              int64_t pct = s.total ? s.bytes * 100 / s.total : 0;
                              if (pct != last_pct || !ok) {
                                  last_pct = pct;
                                  publish_locked();
                              }
                              return ok;
                          },
                          &error);
    std::lock_guard<std::mutex> lock(g->mu);
    if (status == 200) {
        g->engine->complete();
    } else {
        g->engine->fail(error.empty() ? "download failed" : error, status > 0 ? status : 0);
    }
    publish_locked();
    if (g->engine->status().stage == Stage::ReadyToReboot) {
        g->bus->publish("notify/toast", std::string("Update ready: restart to finish"));
    }
}

void do_previous()
{
    const std::string blocker = previous_version_blocker(preflight_now());
    if (!blocker.empty()) {
        g->bus->publish("notify/toast", blocker);
        return;
    }
    const esp_partition_t *spare = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t desc;
    esp_ota_img_states_t st;
    if (!spare || spare == esp_ota_get_running_partition() ||
        esp_ota_get_partition_description(spare, &desc) != ESP_OK ||
        std::string(desc.project_name) != "summit" ||
        (esp_ota_get_state_partition(spare, &st) == ESP_OK &&
         (st == ESP_OTA_IMG_INVALID || st == ESP_OTA_IMG_ABORTED))) {
        g->bus->publish("notify/toast", std::string("No previous version available"));
        return;
    }
    // The previous image boots as a pending image and goes through the health gate too.
    if (esp_ota_set_boot_partition(spare) == ESP_OK) {
        ESP_LOGW(TAG, "switching to previous version %s", desc.version);
        esp_restart();
    }
}

void worker(void *)
{
    Command cmd;
    for (;;) {
        if (xQueueReceive(g->queue, &cmd, portMAX_DELAY) != pdTRUE) continue;
        switch (cmd.kind) {
        case Command::Check: do_check(); break;
        case Command::Install: do_install(cmd.version); break;
        case Command::Previous: do_previous(); break;
        case Command::Reboot: {
            std::lock_guard<std::mutex> lock(g->mu);
            if (g->engine->status().stage == Stage::ReadyToReboot) esp_restart();
            break;
        }
        }
    }
}

void publish_previous_version()
{
    const esp_partition_t *spare = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t desc;
    std::string version;
    if (spare && spare != esp_ota_get_running_partition() && esp_ota_get_partition_description(spare, &desc) == ESP_OK &&
        std::string(desc.project_name) == "summit")
        version = desc.version;
    g->state->set("update/previous_version", version);
}

}  // namespace

bool update_service_start(const ServiceConfig &config, EventBus &bus, StateStore &state, Settings &settings)
{
    if (g) return true;
    g = new Service;
    g->bus = &bus;
    g->state = &state;
    g->settings = &settings;
    const esp_partition_t *slot = esp_ota_get_next_update_partition(nullptr);
    DeviceInfo d;
    d.hardware_id = config.hardware_id;
    d.version = esp_app_get_description()->version;
    d.slot_size = slot ? static_cast<int64_t>(slot->size) : 0;
    d.allowed_hosts = {kAllowedHost};
    ImageExpect expect;
    if (!expect_for_family(config.chip_family, expect)) {
        ESP_LOGE(TAG, "unknown chip family %s", config.chip_family.c_str());
        return false;
    }
    g->engine = std::make_unique<UpdateEngine>(d, expect, g->writer, g->hasher);
    g->queue = xQueueCreate(4, sizeof(Command));
    if (!g->queue || xTaskCreate(worker, "update", 8192, nullptr, tskIDLE_PRIORITY + 2, nullptr) != pdPASS) return false;

    bus.subscribe("update/command", [](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);
        if (!s) return;
        Command c{};
        if (*s == "check") c.kind = Command::Check;
        else if (*s == "previous") c.kind = Command::Previous;
        else if (*s == "reboot") c.kind = Command::Reboot;
        else if (s->rfind("install:", 0) == 0 && s->size() - 8 < sizeof c.version) {
            c.kind = Command::Install;
            s->copy(c.version, s->size() - 8, 8);
        } else return;
        xQueueSend(g->queue, &c, 0);
    });
    // What's available depends on the channel: re-check whenever it changes (screen or web UI).
    bus.subscribe("settings/update/channel", [](const std::string &, const Value &) {
        Command c{};
        c.kind = Command::Check;
        xQueueSend(g->queue, &c, 0);
    });
    publish();
    publish_previous_version();
    return true;
}

bool upload_begin(int64_t size, std::string *error)
{
    if (!g) return false;
    std::lock_guard<std::mutex> lock(g->mu);
    bool ok = g->engine->start_upload(size, preflight_now());
    if (!ok && error) *error = g->engine->status().error;
    publish_locked();
    return ok;
}

bool upload_data(const uint8_t *data, size_t len)
{
    std::lock_guard<std::mutex> lock(g->mu);
    bool ok = g->engine->data(data, len);
    publish_locked();
    return ok;
}

bool upload_complete(std::string *error)
{
    std::lock_guard<std::mutex> lock(g->mu);
    bool ok = g->engine->complete();
    if (!ok && error) *error = g->engine->status().error;
    publish_locked();
    return ok;
}

void upload_abort(const char *reason)
{
    std::lock_guard<std::mutex> lock(g->mu);
    g->engine->fail(reason);
    publish_locked();
}

}  // namespace summit::update
