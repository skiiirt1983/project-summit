# Recipe reader module (D-008). Pure logic here is host-tested; the LVGL panel arrives in step R4.
idf_component_register(
    SRCS "src/module.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
