// esp_http_server adapter for WebApi: the gzipped page, JSON API routes and the
// streaming firmware upload into the update engine.
#include <cstring>
#include <memory>
#include <string>

#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "summit/update/device.h"
#include "summit/web/api.h"
#include "summit/web/device.h"

extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");

namespace summit::web {

namespace {

const char *TAG = "web";

struct EngineControl : UpdateControl {
    bool upload_begin(int64_t size, std::string *e) override { return update::upload_begin(size, e); }
    bool upload_data(const uint8_t *d, size_t n) override { return update::upload_data(d, n); }
    bool upload_complete(std::string *e) override { return update::upload_complete(e); }
    void upload_abort(const char *r) override { update::upload_abort(r); }
};

// Wi-Fi credentials in their own NVS namespace: never part of a settings export.
struct NvsWifi : WifiStore {
    std::string ssid() override
    {
        nvs_handle_t h;
        if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return "";
        char buf[33] = {};
        size_t len = sizeof buf;
        std::string out = nvs_get_str(h, "ssid", buf, &len) == ESP_OK ? buf : "";
        nvs_close(h);
        return out;
    }
    bool save(const std::string &ssid, const std::string &password, std::string *error) override
    {
        nvs_handle_t h;
        esp_err_t err = nvs_open("wifi", NVS_READWRITE, &h);
        if (err != ESP_OK) {
            *error = esp_err_to_name(err);
            return false;
        }
        err = nvs_set_str(h, "ssid", ssid.c_str());
        if (err == ESP_OK) err = nvs_set_str(h, "pass", password.c_str());
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
        if (err != ESP_OK) *error = esp_err_to_name(err);
        return err == ESP_OK;
    }
};

struct EspFacts : DeviceFacts {
    std::vector<std::pair<std::string, std::string>> facts() override
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *boot = esp_ota_get_boot_partition();
        return {
            {"uptime_s", std::to_string(esp_timer_get_time() / 1000000)},
            {"free_heap", std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL))},
            {"min_free_heap", std::to_string(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL))},
            {"free_psram", std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))},
            {"reset_reason", std::to_string(static_cast<int>(esp_reset_reason()))},
            {"running_partition", running ? running->label : "?"},
            {"boot_partition", boot ? boot->label : "?"},
            {"pending_verify", update::ota_pending_verify() ? "yes" : "no"},
            {"idf_version", esp_get_idf_version()},
        };
    }
};

struct Server {
    EngineControl control;
    NvsWifi wifi;
    EspFacts facts;
    std::unique_ptr<WebApi> api;
};
Server *g = nullptr;

const char *status_line(int code)
{
    switch (code) {
    case 200: return "200 OK";
    case 202: return "202 Accepted";
    case 403: return "403 Forbidden";
    case 400: return "400 Bad Request";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 409: return "409 Conflict";
    case 411: return "411 Length Required";
    case 413: return "413 Payload Too Large";
    case 415: return "415 Unsupported Media Type";
    case 422: return "422 Unprocessable Entity";
    default: return "500 Internal Server Error";
    }
}

esp_err_t send(httpd_req_t *req, const Response &r)
{
    httpd_resp_set_status(req, status_line(r.status));
    httpd_resp_set_type(req, r.content_type.c_str());
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    std::string disposition;
    if (!r.filename.empty()) {
        disposition = "attachment; filename=\"" + r.filename + "\"";
        httpd_resp_set_hdr(req, "Content-Disposition", disposition.c_str());
    }
    return httpd_resp_send(req, r.body.data(), static_cast<ssize_t>(r.body.size()));
}

std::string header(httpd_req_t *req, const char *name)
{
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0 || len > 256) return "";
    std::string v(len + 1, '\0');
    if (httpd_req_get_hdr_value_str(req, name, v.data(), v.size()) != ESP_OK) return "";
    v.resize(len);
    return v;
}

esp_err_t page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, reinterpret_cast<const char *>(index_html_gz_start),
                           index_html_gz_end - index_html_gz_start);
}

esp_err_t api_handler(httpd_req_t *req)
{
    Request r;
    r.method = req->method == HTTP_POST ? "POST" : "GET";
    r.path = req->uri;
    size_t q = r.path.find('?');
    if (q != std::string::npos) r.path.resize(q);
    r.content_type = header(req, "Content-Type");
    if (req->method == HTTP_POST) {
        if (req->content_len > static_cast<size_t>(WebApi::kMaxJsonBody)) {
            return send(req, g->api->handle({"POST", r.path, r.content_type, std::string(WebApi::kMaxJsonBody + 1, ' ')}));
        }
        r.body.resize(req->content_len);
        size_t got = 0;
        while (got < req->content_len) {
            int n = httpd_req_recv(req, r.body.data() + got, req->content_len - got);
            if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (n <= 0) return ESP_FAIL;
            got += static_cast<size_t>(n);
        }
    }
    return send(req, g->api->handle(r));
}

esp_err_t upload_handler(httpd_req_t *req)
{
    // A custom header forces a CORS preflight, so other web pages can't post firmware.
    if (header(req, "X-Summit-Upload") != "1") {
        return send(req, WebApi::upload_refused());
    }
    Response err;
    if (!g->api->upload_begin(static_cast<int64_t>(req->content_len), &err)) return send(req, err);
    static char buf[4096];
    size_t left = req->content_len;
    while (left > 0) {
        int n = httpd_req_recv(req, buf, left < sizeof buf ? left : sizeof buf);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) {
            g->api->upload_abort("upload connection lost");
            return ESP_FAIL;
        }
        left -= static_cast<size_t>(n);
        if (!g->api->upload_chunk(reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(n))) break;
    }
    return send(req, g->api->upload_finish());
}

}  // namespace

bool web_server_start(EventBus &bus, StateStore &state, Settings &settings)
{
    if (g) return true;
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif init: %s", esp_err_to_name(err));
        return false;
    }
    g = new Server;
    g->api = std::make_unique<WebApi>(bus, state, settings, g->control, g->wifi, g->facts);
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return false;
    }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = page, .user_ctx = nullptr},
        {.uri = "/api/firmware/upload", .method = HTTP_POST, .handler = upload_handler, .user_ctx = nullptr},
        {.uri = "/api/*", .method = HTTP_GET, .handler = api_handler, .user_ctx = nullptr},
        {.uri = "/api/*", .method = HTTP_POST, .handler = api_handler, .user_ctx = nullptr},
    };
    for (const auto &r : routes) httpd_register_uri_handler(server, &r);
    ESP_LOGI(TAG, "web UI on port %d", cfg.server_port);
    return true;
}

}  // namespace summit::web
