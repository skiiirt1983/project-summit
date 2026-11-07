#include "nvs_settings_storage.h"

#include <string>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "nvs_settings";
static const char *kBlobKey = "v";

bool NvsSettingsStorage::load(summit::SettingsMap &out)
{
    out.clear();
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return true;  // never written
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open: %s", esp_err_to_name(err));
        return false;
    }
    size_t len = 0;
    err = nvs_get_blob(h, kBlobKey, nullptr, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return true;
    }
    std::string blob(len, '\0');
    if (err == ESP_OK) err = nvs_get_blob(h, kBlobKey, blob.data(), &len);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read: %s", esp_err_to_name(err));
        return false;
    }
    if (!summit::decode_settings(blob, out)) {
        ESP_LOGE(TAG, "stored settings blob is malformed (%u bytes)", (unsigned)len);
        return false;
    }
    return true;
}

bool NvsSettingsStorage::save(const summit::SettingsMap &values)
{
    std::string blob;
    if (!summit::encode_settings(values, blob)) return false;
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open for write: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_blob(h, kBlobKey, blob.data(), blob.size());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "save: %s", esp_err_to_name(err));
    return err == ESP_OK;
}
