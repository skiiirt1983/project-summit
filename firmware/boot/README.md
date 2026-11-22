# firmware/boot

The boot screen from the Slopeform Summit media kit (`04_Boot_Animation/firmware_lvgl`, supplied by Paul):
`boot_images.c` is the kit's file unchanged. `boot_screen.c` is the kit's with the include path changed and the grow-in added (marked "Slopeform Summit addition": the kit's video grows the spark in, its code only spins and pulses it).
`splash.cpp` shows it over the shell and removes it after at least 2 s and once start-up is ready;
`splash_timer.h` is that timing as pure logic.

The images are ARGB8888 arrays (logo 448x348, spark 208x208, rays 128x128), centred, so they work in
both orientations. They are artwork, not layout, so the "size tokens, not pixels" rule does not apply to them.
