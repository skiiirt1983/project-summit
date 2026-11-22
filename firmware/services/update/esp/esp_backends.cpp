#include "esp_backends.h"

#include <cstdio>

#include "esp_err.h"

namespace summit::update {

bool EspOtaWriter::begin(size_t image_size, std::string *error)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    target_ = esp_ota_get_next_update_partition(nullptr);
    if (!target_ || target_ == running || target_->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        *error = "no spare OTA slot";
        return false;
    }
    if (image_size > target_->size) {
        *error = "image larger than the slot";
        return false;
    }
    // Erase sector by sector as the image arrives. With the real size, esp_ota_begin erased the whole image area
    // up front: seconds with the flash cache frozen, which starves the display's frame buffer reads (screen flashing).
    esp_err_t err = esp_ota_begin(target_, OTA_WITH_SEQUENTIAL_WRITES, &handle_);
    if (err != ESP_OK) {
        *error = esp_err_to_name(err);
        return false;
    }
    open_ = true;
    return true;
}

bool EspOtaWriter::write(const uint8_t *data, size_t len, std::string *error)
{
    esp_err_t err = esp_ota_write(handle_, data, len);
    if (err != ESP_OK) *error = esp_err_to_name(err);
    return err == ESP_OK;
}

bool EspOtaWriter::finish(std::string *error)
{
    open_ = false;
    esp_err_t err = esp_ota_end(handle_);  // validates the image (hash, chip, segments)
    if (err == ESP_OK) err = esp_ota_set_boot_partition(target_);
    if (err != ESP_OK) *error = esp_err_to_name(err);
    return err == ESP_OK;
}

void EspOtaWriter::abort()
{
    if (open_) esp_ota_abort(handle_);
    open_ = false;
}

void MbedSha256::reset()
{
    mbedtls_sha256_free(&ctx_);
    mbedtls_sha256_init(&ctx_);
    mbedtls_sha256_starts(&ctx_, 0);
}

void MbedSha256::update(const uint8_t *data, size_t len) { mbedtls_sha256_update(&ctx_, data, len); }

std::string MbedSha256::hex_digest()
{
    uint8_t out[32];
    mbedtls_sha256_finish(&ctx_, out);
    char hex[65];
    for (int i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", out[i]);
    return std::string(hex, 64);
}

}  // namespace summit::update
