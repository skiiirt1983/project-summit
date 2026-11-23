// ESP32-P4 side of the audio service: ES8311 codec + NS4150B amplifier on the Waveshare board
// (I2S MCLK 13, BCLK 12, WS 10, DOUT 9; amplifier enable GPIO53; codec on the shared I2C bus at 0x18).
// Compiled in CI; never run on the board until Paul flashes it (HARDWARE_TODO.md).
#pragma once
#include "summit/audio.h"

namespace summit {

// Starts the audio task. i2c_bus = the shared i2c_master_bus_handle_t from the BSP. Returns false (and logs why) if
// the codec could not be brought up; the device then simply stays silent.
bool audio_service_start(EventBus &bus, StateStore &state, Settings &settings, void *i2c_bus);

}  // namespace summit
