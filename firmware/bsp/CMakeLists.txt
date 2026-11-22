# Board support for the Waveshare ESP32-P4-WIFI6-Touch-LCD-5: display, touch, backlight, LVGL port.
# Written from Waveshare's BSP (Apache-2.0) pin and timing values; compiled in CI, UNTESTED on hardware.
idf_component_register(
    SRCS "src/bsp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework
    PRIV_REQUIRES esp_lcd esp_driver_i2c esp_driver_ledc esp_driver_gpio esp_hw_support esp_timer esp_psram heap)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
