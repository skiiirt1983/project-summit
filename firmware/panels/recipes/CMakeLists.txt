# Recipe reader module (D-008). Pure logic here is host-tested; the LVGL panel arrives in step R4.
idf_component_register(
    SRCS "src/module.cpp" "src/recipe.cpp" "src/quantity.cpp" "src/step_time.cpp" "src/store.cpp" "src/cook.cpp" "src/panel.cpp" "src/web_api.cpp" "src/jsonld.cpp" "esp/url_fetch.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework shell web
    PRIV_REQUIRES json esp_http_client esp-tls mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
