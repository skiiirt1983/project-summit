idf_component_register(
    SRCS "nvs_settings_storage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework nvs_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
