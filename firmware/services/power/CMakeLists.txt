# Low-power sleep (D-012): pure scheduler + snapshot (host-tested in tests/host/test_sleep.cpp).
idf_component_register(
    SRCS "src/sleep.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework timers)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
