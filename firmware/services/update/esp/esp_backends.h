#pragma once
#include "esp_ota_ops.h"
#include "mbedtls/sha256.h"
#include "summit/update/engine.h"

namespace summit::update {

// Writes the spare app slot via esp_ota_*. Never the running slot or factory.
class EspOtaWriter : public OtaWriter {
public:
    bool begin(size_t image_size, std::string *error) override;
    bool write(const uint8_t *data, size_t len, std::string *error) override;
    bool finish(std::string *error) override;
    void abort() override;

private:
    const esp_partition_t *target_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    bool open_ = false, flash_open_ = false;
    // The whole image is collected in PSRAM while it downloads and written to flash in one burst at the end, so the
    // display only suffers flash stalls for those few seconds. Null = streaming fallback (no buffer available).
    uint8_t *buf_ = nullptr;
    size_t buf_len_ = 0, buf_size_ = 0;
    bool open_flash(std::string *error);
};

class MbedSha256 : public Hasher {
public:
    MbedSha256() { mbedtls_sha256_init(&ctx_); }
    ~MbedSha256() override { mbedtls_sha256_free(&ctx_); }
    void reset() override;
    void update(const uint8_t *data, size_t len) override;
    std::string hex_digest() override;

private:
    mbedtls_sha256_context ctx_;
};

}  // namespace summit::update
