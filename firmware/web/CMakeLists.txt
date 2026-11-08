# Web UI (config portal). src/api.cpp is pure (host-tested in tests/host/test_web_api.cpp,
# also served by the PC simulator); esp/ maps esp_http_server onto it; page/index.html is
# gzipped at build time and embedded in flash.
idf_component_register(
    SRCS "src/api.cpp" "esp/http_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework
    PRIV_REQUIRES json esp_http_server esp_netif nvs_flash esp_timer esp_system app_update esp_app_format update)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)

set(PAGE ${CMAKE_CURRENT_LIST_DIR}/page/index.html)
set(PAGE_GZ ${CMAKE_CURRENT_BINARY_DIR}/index.html.gz)
add_custom_command(OUTPUT ${PAGE_GZ}
    COMMAND gzip -9 -n -c ${PAGE} > ${PAGE_GZ}
    DEPENDS ${PAGE}
    VERBATIM)
add_custom_target(web_page_gz DEPENDS ${PAGE_GZ})
add_dependencies(${COMPONENT_LIB} web_page_gz)
target_add_binary_data(${COMPONENT_LIB} ${PAGE_GZ} BINARY)
