idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES framework shell update web recipes nvs_settings nvs_flash esp_app_format)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
