#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>

#include <atomic>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "summit/web/log_capture.h"
#include "summit/web/log_mirror.h"

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

// Text not yet written to the SD card (see log_mirror_start). Guarded by g_line_mu; capped so a missing card cannot eat RAM.
std::string g_pending;
std::atomic<size_t> g_pending_bytes{0};
std::atomic<bool> g_important{false};  // a warning or error line is waiting: write it soon
bool g_had_ram_log = false;            // the log from before the last restart was still in RAM
constexpr size_t kPendingMax = 24 * 1024;

void pending_cap()
{
    if (g_pending.size() <= kPendingMax) return;
    size_t cut = g_pending.size() - kPendingMax / 2;
    const size_t nl = g_pending.find('\n', cut);
    if (nl != std::string::npos) cut = nl + 1;
    g_pending.erase(0, cut);
    g_pending.insert(0, "[earlier lines dropped: no SD card to write them to]\n");
}

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
            g_pending.append(g_line, len);
            pending_cap();
            g_pending_bytes = g_pending.size();
            if (log_line_is_important(g_line, len)) g_important = true;
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
        g_had_ram_log = true;
    }
    g_ring.magic = kMagic;
    g_ring.len = 0;
    if (!g_previous) g_previous = esp_log_set_vprintf(hook);
}

namespace {
void mirror_task(void *arg)
{
    Storage &storage = *static_cast<Storage *>(arg);
    while (!storage.present()) vTaskDelay(pdMS_TO_TICKS(1000));  // the card mounts a moment after boot
    LogMirror mirror(storage);
    const std::string previous = mirror.open(16 * 1024);
    if (!g_had_ram_log && !previous.empty()) {  // a power cut or a reset lost the copy in RAM: the card has it
        std::string block = "===== log from before the last restart (read from the SD card) =====\n" + previous;
        if (block.back() != '\n') block += '\n';
        device_log().prepend(block + "===== this boot =====\n");
    }
    LogFlushPolicy policy;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        const int64_t now = esp_timer_get_time() / 1000;
        if (!storage.present()) continue;  // card pulled: the text waits in g_pending (capped)
        if (!policy.due(now, g_pending_bytes.load() > 0, g_important.load())) continue;
        std::string chunk;
        {
            std::lock_guard<std::mutex> lock(g_line_mu);
            chunk.swap(g_pending);
            g_pending_bytes = 0;
            g_important = false;
        }
        policy.flushed(now);
        if (!mirror.append(chunk)) {  // write failed (card full or pulled): keep the text for the next try
            std::lock_guard<std::mutex> lock(g_line_mu);
            g_pending.insert(0, chunk);
            pending_cap();
            g_pending_bytes = g_pending.size();
        }
    }
}
}  // namespace

void log_mirror_start(Storage &storage)
{
    static bool started = false;
    if (started) return;
    started = true;
    xTaskCreate(mirror_task, "logsd", 6144, &storage, tskIDLE_PRIORITY + 1, nullptr);
}

}  // namespace summit::web
