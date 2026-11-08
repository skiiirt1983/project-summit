// Starts the device web server (config portal) on port 80. Device only.
// Reachable once the network service brings Wi-Fi up (esp_hosted, Phase 1).
#pragma once
#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/state_store.h"

namespace summit::web {
bool web_server_start(EventBus &bus, StateStore &state, Settings &settings);
}
