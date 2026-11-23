#include "esp_backends.h"

#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"

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
    // Collect the image in PSRAM first: flash erase/write freezes the cache and starves the display (blue flashing),
    // so all of it is done in one short burst in finish(). Falls back to streaming if there is no room.
    buf_len_ = 0;
    buf_size_ = image_size;
    buf_ = image_size ? static_cast<uint8_t *>(heap_caps_malloc(image_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) : nullptr;
    open_ = true;
    if (buf_) return true;
    return open_flash(error);
}

bool EspOtaWriter::open_flash(std::string *error, size_t erase_bytes)
{
    // erase_bytes = the image size: the whole image area is erased up front in big blocks, much faster than sector by
    // sector (alpha.18 measured about 21 s for 3.6 MB that way). Streaming fallback: sector by sector as data arrives.
    esp_err_t err = esp_ota_begin(target_, erase_bytes, &handle_);
    if (err != ESP_OK) {
        *error = esp_err_to_name(err);
        open_ = false;
        return false;
    }
    flash_open_ = true;
    return true;
}

bool EspOtaWriter::write(const uint8_t *data, size_t len, std::string *error)
{
    if (buf_) {
        if (buf_len_ + len > buf_size_) {
            // More data than announced: switch to streaming for the rest.
            if (!open_flash(error)) return false;
            esp_err_t e = esp_ota_write(handle_, buf_, buf_len_);
            heap_caps_free(buf_);
            buf_ = nullptr;
            if (e != ESP_OK) { *error = esp_err_to_name(e); return false; }
        } else {
            memcpy(buf_ + buf_len_, data, len);
            buf_len_ += len;
            return true;
        }
    }
    esp_err_t err = esp_ota_write(handle_, data, len);
    if (err != ESP_OK) *error = esp_err_to_name(err);
    return err == ESP_OK;
}

bool EspOtaWriter::finish(std::string *error)
{
    open_ = false;
    if (buf_) {  // the one burst
        bool ok = open_flash(error, buf_len_);
        if (ok) {
            const size_t chunk = 64 * 1024;
            for (size_t off = 0; off < buf_len_ && ok; off += chunk) {
                size_t n = buf_len_ - off < chunk ? buf_len_ - off : chunk;
                esp_err_t e = esp_ota_write(handle_, buf_ + off, n);
                if (e != ESP_OK) { *error = esp_err_to_name(e); ok = false; }
            }
        }
        heap_caps_free(buf_);
        buf_ = nullptr;
        if (!ok) { if (flash_open_) esp_ota_abort(handle_); flash_open_ = false; return false; }
    }
    flash_open_ = false;
    esp_err_t err = esp_ota_end(handle_);  // validates the image (hash, chip, segments)
    if (err == ESP_OK) err = esp_ota_set_boot_partition(target_);
    if (err != ESP_OK) *error = esp_err_to_name(err);
    return err == ESP_OK;
}

void EspOtaWriter::abort()
{
    if (flash_open_) esp_ota_abort(handle_);
    if (buf_) heap_caps_free(buf_);
    buf_ = nullptr;
    flash_open_ = false;
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
