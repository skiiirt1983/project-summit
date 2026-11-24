#include "flash_check.h"

#include "summit/update/flash_check.h"

#include <cstdio>
#include <cstring>
#include <mutex>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace summit::update::flashcheck {

namespace {

const char *TAG = "flashcheck";
constexpr uint32_t k16MB = 0x1000000;
constexpr size_t kBlock = 64 * 1024;

std::mutex g_mu;
std::string g_selftest = "not run";
std::string g_verify = "none";
bool g_running = false;

// xorshift32: the pattern is recomputed to compare, so nothing has to be stored.
struct Pattern {
    uint32_t s;
    explicit Pattern(uint32_t seed) : s(seed ? seed : 1) {}
    uint8_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<uint8_t>(s >> 11);
    }
};

struct Compare {
    size_t bad = 0;
    long first = -1;
};

Compare compare_pattern(const uint8_t *data, size_t len, uint32_t seed)
{
    Compare c;
    Pattern p(seed);
    for (size_t i = 0; i < len; ++i) {
        if (data[i] != p.next()) {
            if (c.first < 0) c.first = static_cast<long>(i);
            ++c.bad;
        }
    }
    return c;
}

std::string verdict(const char *what, const Compare &c)
{
    char b[80];
    if (c.bad == 0) snprintf(b, sizeof b, "%s ok", what);
    else snprintf(b, sizeof b, "%s %u bytes differ (first at +%ld)", what, static_cast<unsigned>(c.bad), c.first);
    return b;
}

// Erase one 64 KB block at `off`, check it reads as erased, write a pattern, read it back (esp_flash path) and through
// the cache mapping (the path the image check uses). Returns a one-line summary.
std::string test_block(const esp_partition_t *part, size_t off, uint32_t seed)
{
    char head[40];
    snprintf(head, sizeof head, "@0x%X:", static_cast<unsigned>(part->address + off));
    std::string out = head;
    uint8_t *buf = static_cast<uint8_t *>(heap_caps_malloc(kBlock, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buf) return out + " no memory";
    esp_err_t e = esp_partition_erase_range(part, off, kBlock);
    if (e != ESP_OK) {
        heap_caps_free(buf);
        return out + " erase failed " + esp_err_to_name(e);
    }
    e = esp_partition_read(part, off, buf, kBlock);
    size_t not_erased = 0;
    for (size_t i = 0; e == ESP_OK && i < kBlock; ++i) not_erased += buf[i] != 0xFF;
    out += e != ESP_OK ? std::string(" erase-read ") + esp_err_to_name(e)
                       : (not_erased ? " erase left " + std::to_string(not_erased) + " bytes set," : " erased,");
    Pattern p(seed);
    for (size_t i = 0; i < kBlock; ++i) buf[i] = p.next();
    for (size_t at = 0; at < kBlock && e == ESP_OK; at += 4096) e = esp_partition_write(part, off + at, buf + at, 4096);
    if (e != ESP_OK) {
        heap_caps_free(buf);
        return out + " write failed " + esp_err_to_name(e);
    }
    memset(buf, 0, kBlock);
    e = esp_partition_read(part, off, buf, kBlock);
    out += e == ESP_OK ? " " + verdict("read", compare_pattern(buf, kBlock, seed)) : std::string(" read ") + esp_err_to_name(e);
    heap_caps_free(buf);
    const void *map = nullptr;
    esp_partition_mmap_handle_t h = 0;
    e = esp_partition_mmap(part, off, kBlock, ESP_PARTITION_MMAP_DATA, &map, &h);
    if (e != ESP_OK) return out + std::string(", mmap ") + esp_err_to_name(e);
    out += ", " + verdict("mmap", compare_pattern(static_cast<const uint8_t *>(map), kBlock, seed));
    esp_partition_munmap(h);
    return out;
}

std::string test_psram()
{
    constexpr size_t kSize = 5u * 1024 * 1024;  // above the 4 MB mark the failing images crossed
    uint8_t *m = static_cast<uint8_t *>(heap_caps_malloc(kSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!m) return "psram 5 MB: no memory";
    Pattern p(0xC0FFEE);
    for (size_t i = 0; i < kSize; ++i) m[i] = p.next();
    const Compare c = compare_pattern(m, kSize, 0xC0FFEE);
    heap_caps_free(m);
    return verdict("psram 5 MB", c);
}

void run()
{
    const int64_t t0 = esp_timer_get_time();
    std::string result;
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, "assets");
    if (!part || part->address < k16MB || part->size < 2 * kBlock) {
        result = "assets partition not usable";
    } else {
        result = test_block(part, 0, 0x1234567) + "; " + test_block(part, part->size - kBlock, 0x7654321) + "; " + test_psram();
    }
    char tail[24];
    snprintf(tail, sizeof tail, " (%u ms)", static_cast<unsigned>((esp_timer_get_time() - t0) / 1000));
    result += tail;
    ESP_LOGI(TAG, "self-test: %s", result.c_str());
    std::lock_guard<std::mutex> lock(g_mu);
    g_selftest = result;
    g_running = false;
}

}  // namespace

void start(EventBus &bus)
{
    bus.subscribe("diag/flash_test", [](const std::string &, const Value &) {
        {
            std::lock_guard<std::mutex> lock(g_mu);
            if (g_running) return;
            g_running = true;
            g_selftest = "running";
        }
        if (xTaskCreate([](void *) { run(); vTaskDelete(nullptr); }, "flashcheck", 6144, nullptr, 3, nullptr) != pdPASS) {
            std::lock_guard<std::mutex> lock(g_mu);
            g_running = false;
            g_selftest = "could not start";
        }
    });
}

void locate_mismatch(const esp_partition_t *slot, const uint8_t *image, size_t len)
{
    std::string r;
    uint8_t *chunk = static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!slot || !image || !chunk) {
        if (chunk) heap_caps_free(chunk);
        std::lock_guard<std::mutex> lock(g_mu);
        g_verify = "could not compare";
        return;
    }
    size_t bad = 0, bad_high = 0;
    long first = -1, last = -1;
    esp_err_t e = ESP_OK;
    for (size_t off = 0; off < len && e == ESP_OK; off += 4096) {
        const size_t n = len - off < 4096 ? len - off : 4096;
        e = esp_partition_read(slot, off, chunk, n);
        if (e != ESP_OK) break;
        for (size_t i = 0; i < n; ++i) {
            if (chunk[i] != image[off + i]) {
                if (first < 0) first = static_cast<long>(off + i);
                last = static_cast<long>(off + i);
                ++bad;
                if (slot->address + off + i >= k16MB) ++bad_high;
            }
        }
    }
    heap_caps_free(chunk);
    char b[200];
    if (e != ESP_OK) snprintf(b, sizeof b, "read-back failed: %s", esp_err_to_name(e));
    else if (bad == 0) snprintf(b, sizeof b, "flash matches the image (%u bytes) but esp_ota_end failed", static_cast<unsigned>(len));
    else
        snprintf(b, sizeof b, "%u of %u bytes differ, first +%ld last +%ld (flash 0x%X..), %u of them at 16 MB or above",
                 static_cast<unsigned>(bad), static_cast<unsigned>(len), first, last,
                 static_cast<unsigned>(slot->address + (first < 0 ? 0 : first)), static_cast<unsigned>(bad_high));
    ESP_LOGE(TAG, "install verify: %s", b);
    std::lock_guard<std::mutex> lock(g_mu);
    g_verify = b;
}

std::string diag_lines()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return "flash_selftest=" + g_selftest + "\nflash_last_verify=" + g_verify + "\n";
}

}  // namespace summit::update::flashcheck
