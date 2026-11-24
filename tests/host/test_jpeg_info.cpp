#include <vector>

#include "check.h"
#include "summit/jpeg_info.h"

using namespace summit;

namespace {
// SOI + optional APP0 + SOF with the given marker and size; `samp` = sampling byte of component 1 (0x22 = 4:2:0).
std::vector<uint8_t> make(uint8_t sof, int w, int h, int comps, uint8_t samp, bool app0 = true, int precision = 8)
{
    std::vector<uint8_t> v = {0xFF, 0xD8};
    if (app0) {
        const uint8_t a[] = {0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0};
        v.insert(v.end(), a, a + sizeof a);
    }
    const int len = 8 + comps * 3;
    v.insert(v.end(), {0xFF, sof, static_cast<uint8_t>(len >> 8), static_cast<uint8_t>(len), static_cast<uint8_t>(precision),
                       static_cast<uint8_t>(h >> 8), static_cast<uint8_t>(h), static_cast<uint8_t>(w >> 8), static_cast<uint8_t>(w),
                       static_cast<uint8_t>(comps)});
    for (int c = 0; c < comps; ++c) v.insert(v.end(), {static_cast<uint8_t>(c + 1), static_cast<uint8_t>(c == 0 ? samp : 0x11), 0});
    return v;
}
}  // namespace

TEST(reads_baseline_headers)
{
    JpegInfo i;
    std::string why;
    auto v = make(0xC0, 905, 1280, 3, 0x22);
    CHECK(parse_jpeg_header(v.data(), v.size(), i, &why));
    CHECK(i.width == 905 && i.height == 1280 && i.components == 3 && i.precision == 8);
    CHECK(!i.progressive && !i.arithmetic && i.sampling == JpegSampling::Yuv420);
    v = make(0xC0, 320, 240, 3, 0x21, false);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.sampling == JpegSampling::Yuv422 && i.width == 320 && i.height == 240);
    v = make(0xC0, 64, 64, 3, 0x11);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.sampling == JpegSampling::Yuv444);
    v = make(0xC0, 64, 64, 3, 0x12);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.sampling == JpegSampling::Yuv440);
    v = make(0xC0, 64, 64, 1, 0x11);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.sampling == JpegSampling::Gray && i.components == 1);
    v = make(0xC1, 64, 64, 3, 0x22);  // extended sequential: still a plain Huffman frame
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && !i.progressive);
}

TEST(flags_what_the_hardware_cannot_do)
{
    JpegInfo i;
    auto v = make(0xC2, 800, 600, 3, 0x22);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.progressive);
    CHECK(!plan_decode(i, 1u << 26, 0).ok);
    v = make(0xC9, 800, 600, 3, 0x22);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.arithmetic);
    CHECK(!plan_decode(i, 1u << 26, 0).ok);
    v = make(0xC0, 800, 600, 3, 0x22, true, 12);
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && !plan_decode(i, 1u << 26, 0).ok);
    v = make(0xC0, 800, 600, 3, 0x41);  // 4:1:1 style factors are not supported
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && i.sampling == JpegSampling::Unknown);
    CHECK(plan_decode(i, 1u << 26, 0).why == "unsupported colour sampling");
    v = make(0xC0, 800, 600, 4, 0x22);  // CMYK
    CHECK(parse_jpeg_header(v.data(), v.size(), i) && !plan_decode(i, 1u << 26, 0).ok);
}

TEST(rejects_bad_files)
{
    JpegInfo i;
    std::string why;
    const uint8_t png[] = {0x89, 'P', 'N', 'G', 0, 0, 0, 0};
    CHECK(!parse_jpeg_header(png, sizeof png, i, &why) && why == "not a JPEG");
    CHECK(!parse_jpeg_header(nullptr, 0, i, &why));
    auto v = make(0xC0, 100, 100, 3, 0x22);
    CHECK(!parse_jpeg_header(v.data(), 12, i, &why));      // cut off inside the APP0 segment
    CHECK(!parse_jpeg_header(v.data(), v.size() - 5, i, &why));  // cut off inside the frame header
    v = make(0xC0, 0, 100, 3, 0x22);
    CHECK(!parse_jpeg_header(v.data(), v.size(), i, &why) && why == "JPEG has no size");
    const uint8_t sos[] = {0xFF, 0xD8, 0xFF, 0xDA, 0x00, 0x02, 0, 0};
    CHECK(!parse_jpeg_header(sos, sizeof sos, i, &why) && why == "no frame header");
    const uint8_t junk[] = {0xFF, 0xD8, 0x12, 0x34, 0x56, 0x78};
    CHECK(!parse_jpeg_header(junk, sizeof junk, i, &why) && why == "damaged JPEG header");
    const uint8_t shortlen[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x01, 0, 0};
    CHECK(!parse_jpeg_header(shortlen, sizeof shortlen, i, &why));
    const uint8_t fill[] = {0xFF, 0xD8, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x02, 0xFF, 0xD9};  // fill bytes then end: no frame
    CHECK(!parse_jpeg_header(fill, sizeof fill, i, &why));
}

TEST(plan_sizes_and_memory)
{
    JpegInfo i;
    auto v = make(0xC0, 905, 1280, 3, 0x22);
    parse_jpeg_header(v.data(), v.size(), i);
    DecodePlan p = plan_decode(i, 8u << 20, 1u << 20);
    CHECK(p.ok && p.aligned_w == 912 && p.aligned_h == 1280);  // 905 rounds up to 912
    CHECK(p.stride_bytes == 1824 && p.out_bytes == 1824u * 1280);
    CHECK(plan_decode(i, p.out_bytes + (1u << 20), 1u << 20).ok);               // exactly enough
    CHECK(!plan_decode(i, p.out_bytes + (1u << 20) - 1, 1u << 20).ok);         // one byte short
    CHECK(plan_decode(i, p.out_bytes + (1u << 20) - 1, 1u << 20).why == "not enough free memory");
    CHECK(!plan_decode(i, 100, 1u << 20).ok);                                  // reserve larger than what is free: no underflow
    v = make(0xC0, 320, 160, 3, 0x22);  // a thumbnail
    parse_jpeg_header(v.data(), v.size(), i);
    p = plan_decode(i, 1u << 20, 0);
    CHECK(p.ok && p.aligned_w == 320 && p.aligned_h == 160 && p.out_bytes == 320u * 160 * 2);
    v = make(0xC0, 5000, 4000, 3, 0x22);
    parse_jpeg_header(v.data(), v.size(), i);
    CHECK(plan_decode(i, 1u << 30, 0).why == "picture too large");
    v = make(0xC0, 1, 1, 3, 0x22);  // tiny: still a whole 16x16 block
    parse_jpeg_header(v.data(), v.size(), i);
    p = plan_decode(i, 1u << 20, 0);
    CHECK(p.ok && p.aligned_w == 16 && p.aligned_h == 16 && p.out_bytes == 512);
    CHECK(round_up(0, 16) == 0 && round_up(16, 16) == 16 && round_up(17, 16) == 32);
}

int main()
{
    RUN(reads_baseline_headers);
    RUN(flags_what_the_hardware_cannot_do);
    RUN(rejects_bad_files);
    RUN(plan_sizes_and_memory);
    return report("jpeg info");
}
