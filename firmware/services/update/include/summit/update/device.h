// ESP-IDF side of the update engine: the update service task, the post-boot
// health gate task and the previous-version switch. Device only (not host-built).
//
// Bus commands (event "update/command", string):
//   "check" | "install:<version>" | "previous" | "reboot"
// Published state: see docs/ARCHITECTURE.md "State topics" (update/*).
#pragma once
#include <cstdint>
#include <functional>
#include <string>

#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/state_store.h"
#include "summit/update/engine.h"

namespace summit::update {

// Probes for the health gate. Missing probes count as unhealthy.
struct HealthProbes {
    std::function<uint32_t()> ui_tick_age_ms;
    std::function<uint32_t()> frames_rendered;
    std::function<bool()> touch_ok;
    std::function<bool()> i2c_ok;
    bool settings_ok = false;
};

// True if the running image is new and not yet confirmed (PENDING_VERIFY).
bool ota_pending_verify();
// Fail-fast: an init failure on a pending image marks it invalid and reboots into
// the previous slot. No effect on a confirmed image.
void ota_init_failed(const char *reason);
// Starts the 2.5 s / 3 samples / 40 s gate if this boot is pending. On confirm it
// publishes a "Updated to <version>" toast.
bool ota_guard_start(HealthProbes probes, EventBus &bus, StateStore &state);

struct ServiceConfig {
    std::string hardware_id;     // e.g. waveshare-p4-wifi6-lcd5-rev3
    std::string chip_family;     // "rev3" | "rev1"
};

// Starts the update service (worker task). Reads update/channel from settings.
bool update_service_start(const ServiceConfig &config, EventBus &bus, StateStore &state, Settings &settings);

// Web UI upload path (step 6) goes through the same engine: begin, stream, complete.
bool upload_begin(int64_t size, std::string *error);
bool upload_data(const uint8_t *data, size_t len);
bool upload_complete(std::string *error);
void upload_abort(const char *reason);

}  // namespace summit::update
