#pragma once
#include <cstdint>
#include <string>

// Writes a 32-bit XRGB8888 framebuffer (LVGL LV_COLOR_DEPTH 32) as an RGB PNG.
bool write_png(const std::string &path, const uint8_t *xrgb, int width, int height, int stride_bytes);
