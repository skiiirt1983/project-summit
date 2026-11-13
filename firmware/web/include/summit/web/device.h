// Starts the device web server (config portal) on port 80. Device only.
// Reachable once the network service brings Wi-Fi up (esp_hosted, Phase 1).
#pragma once
#include "summit/event_bus.h"
#include "summit/modules.h"
#include "summit/settings.h"
#include "summit/state_store.h"
#include "summit/storage.h"
#include "summit/web/api.h"

namespace summit::web {
// extension: extra endpoints (e.g. recipes); storage: for responses that stream a card file.
bool web_server_start(EventBus &bus, StateStore &state, Settings &settings, const ModuleManager *modules = nullptr,
                      ApiExtension *extension = nullptr, Storage *storage = nullptr);
}
