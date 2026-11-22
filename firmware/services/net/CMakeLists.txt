# Wi-Fi through the ESP32-C6 co-processor (esp_hosted over SDIO, via esp_wifi_remote). Device only,
# compiled in CI and UNTESTED on hardware until Paul flashes it (docs/HARDWARE_TODO.md).
idf_component_register(
    SRCS "esp/wifi_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
