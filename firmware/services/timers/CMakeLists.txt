# Kitchen timers: pure logic + bus/state service (host-tested in tests/host/test_timers.cpp).
idf_component_register(
    SRCS "src/timers.cpp" "src/stopwatch.cpp" "src/alarms.cpp" "src/clock_tools.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
