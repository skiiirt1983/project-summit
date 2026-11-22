# firmware/boot

The boot screen from the Slopeform Summit media kit (`04_Boot_Animation/firmware_lvgl`, supplied by Paul):
`boot_screen.c` and `boot_images.c` are the kit's files unchanged apart from the include path.
`splash.cpp` shows it over the shell and removes it after at least 2 s and once start-up is ready;
`splash_timer.h` is that timing as pure logic.

The images are ARGB8888 arrays (logo 448x348, spark 208x208, rays 128x128), centred, so they work in
both orientations. They are artwork, not layout, so the "size tokens, not pixels" rule does not apply to them.
