#include "nvs_settings_storage.h"

#include <string>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "nvs_settings";

summit::ReadStatus NvsSettingsStorage::read_key(const char *key, std::string &blob)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return summit::ReadStatus::Empty;  // never written
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open: %s", esp_err_to_name(err));
        return summit::ReadStatus::Error;
    }
    size_t len = 0;
    err = nvs_get_blob(h, key, nullptr, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return summit::ReadStatus::Empty;
    }
    std::string data(len, '\0');
    if (err == ESP_OK) err = nvs_get_blob(h, key, data.data(), &len);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read %s: %s", key, esp_err_to_name(err));
        return summit::ReadStatus::Error;
    }
    blob = std::move(data);
    return summit::ReadStatus::Ok;
}

bool NvsSettingsStorage::write_key(const char *key, const std::string &blob)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open for write: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_blob(h, key, blob.data(), blob.size());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "write %s: %s", key, esp_err_to_name(err));
    return err == ESP_OK;
}
