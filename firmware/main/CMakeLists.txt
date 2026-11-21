idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES framework shell bsp demo update web recipes storage timers power nvs_settings nvs_flash esp_app_format esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
