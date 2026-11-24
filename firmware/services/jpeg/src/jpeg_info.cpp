#include "summit/jpeg_info.h"

namespace summit {
namespace {
void fail(std::string *why, const char *text)
{
    if (why) *why = text;
}
}  // namespace

bool parse_jpeg_header(const uint8_t *d, size_t n, JpegInfo &out, std::string *why)
{
    out = JpegInfo{};
    if (!d || n < 4 || d[0] != 0xFF || d[1] != 0xD8) {
        fail(why, "not a JPEG");
        return false;
    }
    size_t i = 2;
    while (i + 4 <= n) {
        if (d[i] != 0xFF) {  // lost the marker structure
            fail(why, "damaged JPEG header");
            return false;
        }
        const uint8_t m = d[i + 1];
        if (m == 0xFF) {  // fill byte
            ++i;
            continue;
        }
        if (m == 0x01 || (m >= 0xD0 && m <= 0xD8)) {  // markers without a length
            i += 2;
            continue;
        }
        if (m == 0xD9 || m == 0xDA) {  // end of image / start of scan before any frame header
            fail(why, "no frame header");
            return false;
        }
        const size_t len = (static_cast<size_t>(d[i + 2]) << 8) | d[i + 3];
        if (len < 2) {
            fail(why, "damaged JPEG header");
            return false;
        }
        const bool sof = m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC;
        if (sof) {
            if (len < 8 || i + 4 + 6 > n) {
                fail(why, "cut-off JPEG header");
                return false;
            }
            const uint8_t *f = d + i + 4;
            out.precision = f[0];
            out.height = (static_cast<uint32_t>(f[1]) << 8) | f[2];
            out.width = (static_cast<uint32_t>(f[3]) << 8) | f[4];
            out.components = f[5];
            out.progressive = (m == 0xC2 || m == 0xC6 || m == 0xCA || m == 0xCE);
            out.arithmetic = m >= 0xC9 && m != 0xCC;
            if (out.width == 0 || out.height == 0) {
                fail(why, "JPEG has no size");
                return false;
            }
            if (out.components == 1) {
                out.sampling = JpegSampling::Gray;
            } else if (out.components == 3) {
                if (len < 8u + 9u || i + 4 + 6 + 9 > n) {
                    fail(why, "cut-off JPEG header");
                    return false;
                }
                const uint8_t s = f[7];  // component 1 (Y): horizontal factor in the high nibble, vertical in the low
                const int h = s >> 4, v = s & 15;
                const uint8_t s2 = f[10], s3 = f[13];
                if (s2 != 0x11 || s3 != 0x11) out.sampling = JpegSampling::Unknown;
                else if (h == 1 && v == 1) out.sampling = JpegSampling::Yuv444;
                else if (h == 2 && v == 1) out.sampling = JpegSampling::Yuv422;
                else if (h == 2 && v == 2) out.sampling = JpegSampling::Yuv420;
                else if (h == 1 && v == 2) out.sampling = JpegSampling::Yuv440;
                else out.sampling = JpegSampling::Unknown;
            }
            return true;
        }
        i += 2 + len;
    }
    fail(why, "no frame header in the first bytes");
    return false;
}

DecodePlan plan_decode(const JpegInfo &info, size_t free_bytes, size_t reserve_bytes)
{
    DecodePlan p;
    if (info.width == 0 || info.height == 0) p.why = "no picture size";
    else if (info.arithmetic) p.why = "arithmetic coding is not supported";
    else if (info.progressive) p.why = "progressive JPEG (the hardware decodes baseline only)";
    else if (info.precision != 8) p.why = "not an 8-bit JPEG";
    else if (info.components != 1 && info.components != 3) p.why = "needs 1 or 3 colour components";
    else if (info.sampling == JpegSampling::Unknown) p.why = "unsupported colour sampling";
    else {
        p.aligned_w = round_up(info.width, kJpegAlign);
        p.aligned_h = round_up(info.height, kJpegAlign);
        p.stride_bytes = p.aligned_w * 2;
        p.out_bytes = static_cast<size_t>(p.stride_bytes) * p.aligned_h;
        if (static_cast<size_t>(info.width) * info.height > kJpegMaxPixels) p.why = "picture too large";
        else if (free_bytes < reserve_bytes || p.out_bytes > free_bytes - reserve_bytes) p.why = "not enough free memory";
        else p.ok = true;
    }
    if (!p.ok && p.why.empty()) p.why = "cannot decode";
    return p;
}

}  // namespace summit
