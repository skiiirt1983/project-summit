# microSD card service (device only, untested on hardware: docs/HARDWARE_TODO.md).
# The Storage interface and PosixStorage live in framework and are host-tested.
idf_component_register(
    SRCS "esp/sd_card.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework
    PRIV_REQUIRES fatfs sdmmc esp_driver_sdmmc)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
