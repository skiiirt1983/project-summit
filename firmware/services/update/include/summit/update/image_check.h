// Checks the first bytes of an OTA (app-only) image before anything is written:
// Espressif image, ESP32-P4, the right chip-revision family, project "summit",
// and (for catalog installs) the expected version. Pure logic.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace summit::update {

constexpr size_t kImageHeaderBytes = 0x70;  // image header + esp_app_desc_t up to project_name
constexpr uint16_t kChipIdEsp32P4 = 0x0012;

struct ImageExpect {
    std::string project = "summit";
    uint16_t chip_id = kChipIdEsp32P4;
    uint16_t min_rev_lo = 0, min_rev_hi = 0;  // allowed min_chip_rev_full range
    uint16_t max_rev = 0;                     // required max_chip_rev_full
    std::string version;                      // empty = any valid version
};

// "rev3" (ESP32-P4 v3.x) or "rev1" (v1.x); same ranges as tools/summit_image.py.
bool expect_for_family(const std::string &family, ImageExpect &out);

enum class ImageError { None, TooShort, NotAnImage, WrongChip, WrongRevision, NoAppDescriptor, WrongProject, WrongVersion };
const char *image_error_text(ImageError e);

ImageError check_image_header(const uint8_t *data, size_t len, const ImageExpect &expect,
                              std::string *version_out = nullptr);

}  // namespace summit::update
