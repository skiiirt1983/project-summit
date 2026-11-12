// microSD card service (device only). UNTESTED ON HARDWARE: see docs/HARDWARE_TODO.md.
// Mounts the card at /sdcard (never formats it), re-checks every few seconds so a
// card can be removed and inserted, and publishes storage/sd_present and
// storage/sd_free_mb.
#pragma once
#include "summit/state_store.h"
#include "summit/storage.h"

namespace summit {

// Starts the service and returns the storage (valid for the life of the program).
Storage &sd_card_start(StateStore &state);

}  // namespace summit
