#include <cstdarg>
#include <cstdio>

#include "esp_log.h"
#include "summit/web/log_capture.h"

namespace summit::web {

namespace {
vprintf_like_t g_previous = nullptr;

int hook(const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    char line[320];
    const int n = vsnprintf(line, sizeof line, fmt, copy);
    va_end(copy);
    if (n > 0) device_log().append(line, static_cast<size_t>(n) < sizeof line ? static_cast<size_t>(n) : sizeof line - 1);
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
