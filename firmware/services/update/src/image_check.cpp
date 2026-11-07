#include "summit/update/image_check.h"

#include <cstring>

#include "summit/update/version.h"

namespace summit::update {

bool expect_for_family(const std::string &family, ImageExpect &out)
{
    ImageExpect e;
    if (family == "rev3") {
        e.min_rev_lo = 300;
        e.min_rev_hi = 399;
        e.max_rev = 399;
    } else if (family == "rev1") {
        e.min_rev_lo = 100;
        e.min_rev_hi = 199;
        e.max_rev = 199;
    } else {
        return false;
    }
    out = e;
    return true;
}

const char *image_error_text(ImageError e)
{
    switch (e) {
    case ImageError::None: return "ok";
    case ImageError::TooShort: return "image too short";
    case ImageError::NotAnImage: return "not an ESP firmware image";
    case ImageError::WrongChip: return "not built for ESP32-P4";
    case ImageError::WrongRevision: return "built for a different chip revision";
    case ImageError::NoAppDescriptor: return "missing app descriptor (bootloader or full image?)";
    case ImageError::WrongProject: return "not Project Summit firmware";
    case ImageError::WrongVersion: return "version does not match the catalog";
    }
    return "?";
}

static uint16_t u16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
static uint32_t u32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
static std::string cstr(const uint8_t *p, size_t n)
{
    size_t len = 0;
    while (len < n && p[len] != 0) ++len;
    return std::string(reinterpret_cast<const char *>(p), len);
}

ImageError check_image_header(const uint8_t *d, size_t len, const ImageExpect &e, std::string *version_out)
{
    if (!d || len < kImageHeaderBytes) return ImageError::TooShort;
    if (d[0] != 0xE9) return ImageError::NotAnImage;
    if (u16(d + 12) != e.chip_id) return ImageError::WrongChip;
    const uint16_t min_full = u16(d + 15), max_full = u16(d + 17);
    if (min_full < e.min_rev_lo || min_full > e.min_rev_hi || max_full != e.max_rev) return ImageError::WrongRevision;
    if (u32(d + 0x20) != 0xABCD5432u) return ImageError::NoAppDescriptor;
    if (cstr(d + 0x50, 32) != e.project) return ImageError::WrongProject;
    const std::string version = cstr(d + 0x30, 32);
    Version parsed;
    if (!parse_version(version, parsed)) return ImageError::WrongVersion;
    if (!e.version.empty() && version != e.version) return ImageError::WrongVersion;
    if (version_out) *version_out = version;
    return ImageError::None;
}

}  // namespace summit::update
