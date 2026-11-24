// JPEG header reading and the decode plan (pure logic, host-tested in tests/host/test_jpeg_info.cpp).
//
// The ESP32-P4 hardware JPEG engine decodes baseline (sequential, Huffman, 8-bit) JPEGs only, and always writes
// pictures whose width and height are rounded up to a multiple of 16. This file decides, before the engine is touched,
// whether a file can be decoded in hardware and how much memory the result needs, so a progressive or huge file
// fails with a readable reason instead of a crash or an out-of-memory abort.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace summit {

enum class JpegSampling { Unknown, Gray, Yuv444, Yuv422, Yuv420, Yuv440 };

struct JpegInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    int components = 0;     // 1 = gray, 3 = colour
    int precision = 0;      // bits per sample (the hardware does 8)
    bool progressive = false;
    bool arithmetic = false;  // arithmetic coding (never supported)
    JpegSampling sampling = JpegSampling::Unknown;
};

// Reads the markers up to the first start-of-frame. `size` may be just the beginning of the file (a few KB is enough
// for the pictures the web UI makes). Returns false with a short reason when it is not a readable JPEG.
bool parse_jpeg_header(const uint8_t *data, size_t size, JpegInfo &out, std::string *why = nullptr);

constexpr uint32_t kJpegAlign = 16;        // the engine's output size is a multiple of this
constexpr size_t kJpegMaxPixels = 4u << 20;  // 4 Mpixel: a 905x1280 page is 1.2 Mpixel (anything larger is refused)

struct DecodePlan {
    bool ok = false;
    std::string why;           // set when !ok: plain words for the log
    uint32_t aligned_w = 0;    // decoded picture size in pixels (multiples of 16); the visible part is width x height
    uint32_t aligned_h = 0;
    uint32_t stride_bytes = 0; // bytes per decoded row (RGB565, 2 bytes per pixel)
    size_t out_bytes = 0;      // decoded picture, RGB565
};

// Can this picture be decoded in hardware, and what does it need? `free_bytes` is the largest block that can be
// allocated now (PSRAM); `reserve_bytes` is left untouched for everything else.
DecodePlan plan_decode(const JpegInfo &info, size_t free_bytes, size_t reserve_bytes);

constexpr uint32_t round_up(uint32_t v, uint32_t to) { return (v + to - 1) / to * to; }

}  // namespace summit
