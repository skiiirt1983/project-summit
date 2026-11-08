// Publishes the engine's status to the state store (update/* topics, see
// docs/ARCHITECTURE.md "State topics"). Shared by the device service and the simulator.
#pragma once
#include "summit/state_store.h"
#include "summit/update/engine.h"

namespace summit::update {

void publish_status(const UpdateEngine &engine, StateStore &state);

}  // namespace summit::update
