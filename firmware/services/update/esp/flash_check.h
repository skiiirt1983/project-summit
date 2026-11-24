// Used by the OTA writer when an image fails verification (see summit/update/flash_check.h).
#pragma once
#include <cstddef>
#include <cstdint>

#include "esp_partition.h"

namespace summit::update::flashcheck {

// Reads the slot back and compares it with the image still held in memory. Records where and how many bytes differ
// (and how many of them lie at flash addresses of 16 MB or more) for diag_lines().
void locate_mismatch(const esp_partition_t *slot, const uint8_t *image, size_t len);

}  // namespace summit::update::flashcheck
