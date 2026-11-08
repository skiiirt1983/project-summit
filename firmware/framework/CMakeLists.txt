# App framework: event bus, state store, settings, panel API, registry.
# Plain C++17 with no ESP-IDF or LVGL dependency, so tests/host builds it too.
idf_component_register(
    SRCS "src/value.cpp" "src/event_bus.cpp" "src/state_store.cpp"
         "src/settings.cpp" "src/registry.cpp" "src/system_settings.cpp" "src/json_writer.cpp"
    INCLUDE_DIRS "include")
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
