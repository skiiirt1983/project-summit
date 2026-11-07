#include "png.h"

#include <cstdio>
#include <vector>

#include <zlib.h>

static void put32(std::vector<uint8_t> &v, uint32_t x)
{
    for (int i = 3; i >= 0; --i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
}

static void chunk(FILE *f, const char *type, const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> out;
    put32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uLong crc = crc32(0L, out.data() + 4, static_cast<uInt>(data.size() + 4));
    put32(out, static_cast<uint32_t>(crc));
    fwrite(out.data(), 1, out.size(), f);
}

bool write_png(const std::string &path, const uint8_t *xrgb, int width, int height, int stride_bytes)
{
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (1 + 3 * width));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);  // filter: none
        const uint8_t *row = xrgb + static_cast<size_t>(y) * stride_bytes;
        for (int x = 0; x < width; ++x) {
            raw.push_back(row[4 * x + 2]);  // LVGL 32-bit is B, G, R, A in memory
            raw.push_back(row[4 * x + 1]);
            raw.push_back(row[4 * x + 0]);
        }
    }
    uLongf zlen = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> z(zlen);
    if (compress2(z.data(), &zlen, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) return false;
    z.resize(zlen);

    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put32(ihdr, static_cast<uint32_t>(width));
    put32(ihdr, static_cast<uint32_t>(height));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8-bit RGB
    chunk(f, "IHDR", ihdr);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    return fclose(f) == 0;
}
