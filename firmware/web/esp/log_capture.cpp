#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>

#include "esp_log.h"
#include "summit/web/log_capture.h"

namespace summit::web {

namespace {
vprintf_like_t g_previous = nullptr;

// The line buffer is static, not on the caller's stack: some tasks (SD card, network) log from deep inside
// calls and have small stacks. If another task is already in here the line is skipped for the web log (it
// still goes to the serial console), so logging never waits on this.
std::mutex g_line_mu;
char g_line[320];

// The last 16 KB of log text lives in .noinit RAM, which a restart (crash, rollback, update, "Restart") does not
// clear. The next boot puts it in front of the web log. A power cut still loses it.
constexpr uint32_t kMagic = 0x5355474C;  // "SUGL"
constexpr size_t kRingSize = 16 * 1024;
struct Ring {
    uint32_t magic;
    uint32_t len;
    char data[kRingSize];
};
__attribute__((section(".noinit"))) Ring g_ring;

void ring_append(const char *p, size_t n)
{
    if (n > kRingSize / 2) n = kRingSize / 2;
    if (g_ring.len + n > kRingSize) {  // full: keep the newest half, from a line start
        size_t keep = kRingSize / 2;
        const char *from = g_ring.data + g_ring.len - keep;
        const char *nl = static_cast<const char *>(memchr(from, '\n', keep));
        if (nl) {
            from = nl + 1;
            keep = static_cast<size_t>(g_ring.data + g_ring.len - from);
        }
        memmove(g_ring.data, from, keep);
        g_ring.len = static_cast<uint32_t>(keep);
    }
    memcpy(g_ring.data + g_ring.len, p, n);
    g_ring.len += static_cast<uint32_t>(n);
}

int hook(const char *fmt, va_list args)
{
    if (g_line_mu.try_lock()) {
        va_list copy;
        va_copy(copy, args);
        const int n = vsnprintf(g_line, sizeof g_line, fmt, copy);
        va_end(copy);
        if (n > 0) {
            const size_t len = static_cast<size_t>(n) < sizeof g_line ? static_cast<size_t>(n) : sizeof g_line - 1;
            device_log().append(g_line, len);
            ring_append(g_line, len);
        }
        g_line_mu.unlock();
    }
    return g_previous ? g_previous(fmt, args) : vprintf(fmt, args);
}
}  // namespace

LogBuffer &device_log()
{
    static LogBuffer log(48 * 1024);
    return log;
}

void log_capture_start()
{
    if (g_ring.magic == kMagic && g_ring.len > 0 && g_ring.len <= kRingSize) {
        std::string prev(g_ring.data, g_ring.len);
        device_log().append("===== log from before the last restart =====\n");
        device_log().append(prev);
        if (prev.back() != '\n') device_log().append("\n");
        device_log().append("===== this boot =====\n");
    }
    g_ring.magic = kMagic;
    g_ring.len = 0;
    if (!g_previous) g_previous = esp_log_set_vprintf(hook);
}

}  // namespace summit::web
