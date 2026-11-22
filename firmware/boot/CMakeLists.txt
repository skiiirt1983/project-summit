# Boot screen from the Slopeform Summit media kit (LVGL 9 animation) and its timing.
# boot_images.c holds the logo, spark and ray images as ARGB8888 arrays (about 0.9 MB of flash).
idf_component_register(
    SRCS "src/boot_screen.c" "src/boot_images.c" "src/splash.cpp"
    INCLUDE_DIRS "include")
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
