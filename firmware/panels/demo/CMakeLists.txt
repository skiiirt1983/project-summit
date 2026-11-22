# The shell's placeholder panels (home, calendar, weather, news, printer) and their fake glance data,
# shared with the PC simulator. They let the first flash show a real-looking UI before the real panels
# exist. Remove from app_main once the real panels land.
idf_component_register(
    SRCS "../../sim/demo_panels.cpp"
    INCLUDE_DIRS "../../sim"
    REQUIRES framework shell recipes)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
