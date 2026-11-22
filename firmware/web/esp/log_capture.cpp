#include <cstdarg>
#include <cstdio>
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

int hook(const char *fmt, va_list args)
{
    if (g_line_mu.try_lock()) {
        va_list copy;
        va_copy(copy, args);
        const int n = vsnprintf(g_line, sizeof g_line, fmt, copy);
        va_end(copy);
        if (n > 0) device_log().append(g_line, static_cast<size_t>(n) < sizeof g_line ? static_cast<size_t>(n) : sizeof g_line - 1);
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
    if (!g_previous) g_previous = esp_log_set_vprintf(hook);
}

}  // namespace summit::web
