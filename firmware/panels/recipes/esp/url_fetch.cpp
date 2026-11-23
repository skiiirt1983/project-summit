// UNTESTED ON HARDWARE (needs Wi-Fi). Coded against ESP-IDF 5.5's esp_http_client.
#include "summit/recipes/url_fetch.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

namespace summit::recipes {

bool HttpsPageFetcher::get(const std::string &url, size_t max_bytes, std::string &body, std::string *error)
{
    auto fail = [error](const std::string &msg) {
        ESP_LOGW("recipes", "page fetch failed: %s", msg.c_str());  // never the address itself (it can carry personal details)
        if (error) *error = msg;
        return false;
    };
    if (!valid_import_url(url)) return fail("address not allowed");
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.timeout_ms = 15000;
    cfg.max_redirection_count = 3;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = 4096;
    cfg.buffer_size_tx = 2048;
    cfg.user_agent = "SLOPEFORM-Home recipe import";
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return fail("HTTP client init failed");
    bool ok = false;
    std::string data;
    // Follow redirects by hand so every hop is checked against valid_import_url().
    for (int hop = 0; hop < 4; ++hop) {
        esp_err_t err = esp_http_client_open(c, 0);
        if (err != ESP_OK) {
            fail(std::string("connect: ") + esp_err_to_name(err));
            break;
        }
        esp_http_client_fetch_headers(c);
        const int status = esp_http_client_get_status_code(c);
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            esp_http_client_set_redirection(c);
            char next[512] = {};
            if (esp_http_client_get_url(c, next, sizeof next) != ESP_OK || !valid_import_url(next)) {
                fail("redirected to an address that is not allowed");
                break;
            }
            esp_http_client_close(c);
            continue;
        }
        if (status != 200) {
            fail("HTTP " + std::to_string(status));
            break;
        }
        char buf[2048];
        ok = true;
        for (;;) {
            const int n = esp_http_client_read(c, buf, sizeof buf);
            if (n < 0) {
                ok = fail("read failed");
                break;
            }
            if (n == 0) break;
            if (data.size() + static_cast<size_t>(n) > max_bytes) {
                ok = fail("page too large");
                break;
            }
            data.append(buf, static_cast<size_t>(n));
        }
        break;
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    if (ok) body = std::move(data);
    return ok;
}

}  // namespace summit::recipes
