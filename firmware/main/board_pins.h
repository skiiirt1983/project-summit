// Board pin choices that are ours (add-on wiring), not fixed by the Waveshare board.
// ARCHITECTURE.md "Accelerometer wiring". Not yet wired or tested on hardware.
#pragma once

namespace summit::board {

// LIS2DW12 interrupts. INT1 wakes the chip from deep sleep, so it must be an
// LP GPIO (GPIO0-15 on the ESP32-P4). D-012: GPIO4 / GPIO5, to be confirmed
// against the datasheet and the silkscreen before wiring.
constexpr int kAccelInt1Gpio = 4;
constexpr int kAccelInt2Gpio = 5;  // optional
constexpr int kLpGpioMax = 15;
static_assert(kAccelInt1Gpio >= 0 && kAccelInt1Gpio <= kLpGpioMax, "INT1 must be an LP GPIO to wake from deep sleep");

}  // namespace summit::board
