# Low-power sleep (D-012): pure scheduler, snapshot and service (host-tested in
# tests/host/test_sleep.cpp) + the ESP32-P4 deep-sleep port (untested on hardware).
idf_component_register(
    SRCS "src/sleep.cpp" "src/power_service.cpp" "esp/deep_sleep.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework timers
    PRIV_REQUIRES esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
