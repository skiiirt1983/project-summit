#include "summit/net/wifi_service.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "summit/json_writer.h"

namespace summit::net {

namespace {

const char *TAG = "net";

constexpr uint32_t kNotifyCredentials = 1u << 0;  // web UI saved a network: (re)connect now
constexpr uint32_t kNotifyDisconnected = 1u << 1;
constexpr uint32_t kNotifyScan = 1u << 2;
constexpr int64_t kApAfterMs = 45 * 1000;      // no connection for this long: bring the setup AP up
constexpr int64_t kScanEveryDownMs = 30 * 1000;
constexpr int64_t kScanEveryUpMs = 180 * 1000;

EventBus *g_bus = nullptr;
StateStore *g_state = nullptr;
TaskHandle_t g_task = nullptr;
esp_netif_t *g_sta = nullptr;
esp_netif_t *g_ap = nullptr;
volatile bool g_connected = false;
volatile bool g_ap_on = false;

std::string nvs_read(const char *key, size_t cap)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return "";
    std::string buf(cap, '\0');
    size_t len = cap;
    std::string out;
    if (nvs_get_str(h, key, buf.data(), &len) == ESP_OK) out.assign(buf.c_str());
    nvs_close(h);
    return out;
}

void nvs_write(const char *key, const std::string &value)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_str(h, key, value.c_str()) == ESP_OK) nvs_commit(h);
    nvs_close(h);
}

void set_state(bool connected, const std::string &ssid, const std::string &ip)
{
    g_state->set("net/wifi_connected", connected);
    g_state->set("net/ssid", ssid);
    g_state->set("net/ip", ip);
}

// Clock: the update pre-flight and the recipes' "recently cooked" need real time. SNTP starts once there is an
// address; time/unix is published from the net task (UTC for now: a timezone setting comes later).
bool g_sntp_started = false;

void on_time_synced(struct timeval *)
{
    g_state->set("time/synced", true);
    ESP_LOGI(TAG, "clock synced from the network");
}

void start_sntp()
{
    if (g_sntp_started) return;
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = on_time_synced;
    if (esp_netif_sntp_init(&cfg) == ESP_OK) g_sntp_started = true;
}

void publish_time()
{
    const time_t now = time(nullptr);
    if (now < 1700000000) return;  // not synced yet (before late 2023)
    g_state->set("time/unix", static_cast<int64_t>(now));
    struct tm t;
    gmtime_r(&now, &t);
    char hm[8], date[40];
    strftime(hm, sizeof hm, "%H:%M", &t);
    strftime(date, sizeof date, "%A %-d %B", &t);
    g_state->set("time/now_text", std::string(hm));
    g_state->set("time/date_text", std::string(date));
}

void on_event(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const auto *d = static_cast<wifi_event_sta_disconnected_t *>(data);
        ESP_LOGW(TAG, "disconnected (reason %d)", d ? d->reason : -1);
        g_connected = false;
        set_state(false, "", "");
        if (g_task) xTaskNotify(g_task, kNotifyDisconnected, eSetBits);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "associated, waiting for an address");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto *d = static_cast<ip_event_got_ip_t *>(data);
        char ip[16];
        snprintf(ip, sizeof ip, IPSTR, IP2STR(&d->ip_info.ip));
        wifi_ap_record_t ap = {};
        esp_wifi_sta_get_ap_info(&ap);
        g_connected = true;
        set_state(true, reinterpret_cast<const char *>(ap.ssid), ip);
        ESP_LOGI(TAG, "connected to '%s', address %s, signal %d dBm", ap.ssid, ip, ap.rssi);
        start_sntp();
    }
}

void connect_saved()
{
    const std::string ssid = nvs_read("ssid", 33), pass = nvs_read("pass", 64);
    if (ssid.empty()) return;
    wifi_config_t c = {};
    strlcpy(reinterpret_cast<char *>(c.sta.ssid), ssid.c_str(), sizeof c.sta.ssid);
    strlcpy(reinterpret_cast<char *>(c.sta.password), pass.c_str(), sizeof c.sta.password);
    c.sta.threshold.authmode = pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_PSK;
    c.sta.pmf_cfg.capable = true;
    c.sta.pmf_cfg.required = false;
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &c);
    if (err == ESP_OK) err = esp_wifi_connect();
    ESP_LOGI(TAG, "connecting to '%s': %s", ssid.c_str(), esp_err_to_name(err));
}

bool scan()
{
    wifi_scan_config_t cfg = {};
    cfg.show_hidden = false;
    esp_err_t err = esp_wifi_scan_start(&cfg, true);  // blocks this task, not the UI
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return false;
    }
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    n = std::min<uint16_t>(n, 24);
    std::vector<wifi_ap_record_t> recs(n);
    esp_wifi_scan_get_ap_records(&n, recs.data());
    std::sort(recs.begin(), recs.begin() + n, [](const wifi_ap_record_t &a, const wifi_ap_record_t &b) { return a.rssi > b.rssi; });
    JsonWriter w;
    w.begin_array();
    std::string last;
    for (uint16_t i = 0; i < n; ++i) {
        const std::string ssid(reinterpret_cast<const char *>(recs[i].ssid));
        if (ssid.empty() || ssid == last) continue;
        last = ssid;
        w.begin_object().key("ssid").value(ssid).key("rssi").value(static_cast<int>(recs[i].rssi));
        w.key("secure").value(recs[i].authmode != WIFI_AUTH_OPEN).end_object();
    }
    w.end_array();
    g_state->set("net/scan", w.str());
    ESP_LOGI(TAG, "scan: %u networks", static_cast<unsigned>(n));
    return true;
}

// Setup access point. Paul's choice for now (easy set-up, no blocks): a fixed name and a short password.
// WPA2 needs at least 8 characters, so "summit123". Safety that matters: it is its own subnet and the device
// never forwards traffic between it and the home network (no NAPT / IP forwarding, see sdkconfig.defaults),
// and it is switched off as soon as the device is on the home Wi-Fi.
constexpr const char *kApSsid = "Summit-Setup";
constexpr const char *kApPassword = "summit123";

void ap_start()
{
    if (g_ap_on) return;
    wifi_config_t c = {};
    strlcpy(reinterpret_cast<char *>(c.ap.ssid), kApSsid, sizeof c.ap.ssid);
    c.ap.ssid_len = static_cast<uint8_t>(strlen(kApSsid));
    strlcpy(reinterpret_cast<char *>(c.ap.password), kApPassword, sizeof c.ap.password);
    c.ap.channel = 1;
    c.ap.max_connection = 3;
    c.ap.authmode = WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &c);
    g_ap_on = err == ESP_OK;
    g_state->set("net/ap_active", g_ap_on);
    ESP_LOGW(TAG, "SETUP WI-FI: join '%s' (password '%s'), then open http://192.168.4.1  (%s)", kApSsid, kApPassword,
             esp_err_to_name(err));
}

void ap_stop()
{
    if (!g_ap_on) return;
    esp_wifi_set_mode(WIFI_MODE_STA);
    g_ap_on = false;
    g_state->set("net/ap_active", false);
    ESP_LOGI(TAG, "setup access point off");
}

void task(void *)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init);  // starts the hosted link to the C6; can take a few seconds
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s (is the C6 running esp_hosted slave firmware?)", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi started");
    const bool have_network = !nvs_read("ssid", 33).empty();
    int64_t down_since = esp_timer_get_time() / 1000;
    int64_t last_scan = -kScanEveryDownMs;
    // Scan first: a scan cannot run while a connection attempt is in progress (it fails with ESP_ERR_WIFI_STATE).
    if (scan()) last_scan = esp_timer_get_time() / 1000;
    connect_saved();
    if (!have_network) ap_start();
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, pdMS_TO_TICKS(2000));  // also the clock tick for time/*
        const int64_t now = esp_timer_get_time() / 1000;
        publish_time();
        if (bits & kNotifyCredentials) {
            down_since = now;
            connect_saved();
            last_scan = now - kScanEveryDownMs;  // look again soon
        }
        if (bits & kNotifyDisconnected) {
            vTaskDelay(pdMS_TO_TICKS(2000));  // not a tight loop
            if (!g_connected) connect_saved();
        }
        if (g_connected) {
            down_since = now;
            if (g_ap_on) ap_stop();
        } else if (now - down_since > kApAfterMs) {
            ap_start();  // nothing reachable: let the web UI be reached directly
        }
        if (bits & kNotifyScan || now - last_scan > (g_connected ? kScanEveryUpMs : kScanEveryDownMs)) {
            last_scan = scan() ? now : now - (g_connected ? kScanEveryUpMs : kScanEveryDownMs) + 8000;  // failed: again in 8 s
        }
    }
}

}  // namespace

bool wifi_service_start(EventBus &bus, StateStore &state)
{
    if (g_task) return true;
    g_bus = &bus;
    g_state = &state;
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif init: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        return false;
    }
    g_sta = esp_netif_create_default_wifi_sta();
    g_ap = esp_netif_create_default_wifi_ap();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, nullptr);
    set_state(false, "", "");
    state.set("net/scan", std::string("[]"));
    state.set("net/ap_active", false);
    // The web UI publishes net/connect when it has saved a network (runs on the UI loop's bus dispatch).
    bus.subscribe("net/connect", [](const std::string &, const Value &) {
        if (g_task) xTaskNotify(g_task, kNotifyCredentials | kNotifyScan, eSetBits);
    });
    xTaskCreate(task, "net", 8192, nullptr, 5, &g_task);
    return true;
}

}  // namespace summit::net
