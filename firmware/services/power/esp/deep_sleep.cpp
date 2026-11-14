// UNTESTED ON HARDWARE (docs/HARDWARE_TODO.md "Deep sleep and wake").
#include "summit/deep_sleep.h"

#include <sys/time.h>

#include <cstring>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "soc/soc_caps.h"

namespace summit {

namespace {

const char *TAG = "sleep";

// RTC slow memory: zero after a cold start, kept across deep sleep.
RTC_DATA_ATTR uint32_t s_blob_len;
RTC_DATA_ATTR uint8_t s_blob[kSnapshotMaxBytes];
RTC_DATA_ATTR int64_t s_sleep_start_us;

int64_t wall_us()
{
    // The system time keeps running through deep sleep (RTC timer).
    timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000000LL + tv.tv_usec;
}

}  // namespace

BootWake read_boot_wake()
{
    BootWake w;
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: w.cause = WakeCause::Timer; break;
        case ESP_SLEEP_WAKEUP_EXT1: w.cause = WakeCause::Motion; break;
        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_GPIO: w.cause = WakeCause::Button; break;
        default: return w;  // power-on or reset: nothing was kept on purpose
    }
    if (s_blob_len > 0 && s_blob_len <= sizeof s_blob) w.blob.assign(s_blob, s_blob + s_blob_len);
    const int64_t slept = wall_us() - s_sleep_start_us;
    w.slept_ms = slept > 0 ? static_cast<uint64_t>(slept / 1000) : 0;
    s_blob_len = 0;  // used once
    return w;
}

void DeepSleepPort::enter(const std::vector<uint8_t> &snapshot, int64_t wake_in_ms)
{
    s_blob_len = snapshot.size() <= sizeof s_blob ? static_cast<uint32_t>(snapshot.size()) : 0;
    if (s_blob_len) std::memcpy(s_blob, snapshot.data(), s_blob_len);
    s_sleep_start_us = wall_us();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (wake_in_ms >= 0) esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(wake_in_ms) * 1000ULL);
#if SOC_PM_SUPPORT_EXT1_WAKEUP
    if (motion_gpio_ >= 0) esp_sleep_enable_ext1_wakeup_io(1ULL << motion_gpio_, ESP_EXT1_WAKEUP_ANY_HIGH);
#endif
    ESP_LOGI(TAG, "deep sleep; timed wake in %lld ms", static_cast<long long>(wake_in_ms));
    esp_deep_sleep_start();
}

}  // namespace summit
