# Shell: glance strip, pager, quick settings, toasts, rotation.
# src/{layout,glance,strip_state,rotation,pager}.cpp are pure logic (host-tested);
# shell_ui.cpp is the LVGL front end (also built by firmware/sim).
idf_component_register(
    SRCS "src/layout.cpp" "src/glance.cpp" "src/strip_state.cpp" "src/rotation.cpp"
         "src/pager.cpp" "src/shell_ui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
