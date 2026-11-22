# Web UI (config portal). src/api.cpp is pure (host-tested in tests/host/test_web_api.cpp,
# also served by the PC simulator); esp/ maps esp_http_server onto it; page/index.html is
# gzipped at build time and embedded in flash.
idf_component_register(
    SRCS "src/api.cpp" "src/log_buffer.cpp" "esp/http_server.cpp" "esp/log_capture.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework
    PRIV_REQUIRES json log esp_http_server esp_netif nvs_flash esp_timer esp_system app_update esp_app_format update)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)

# Static web files, gzipped into flash. Served by esp/http_server.cpp (table kStatic).
set(WEB_FILES
    page/index.html
    page/recipes.mjs
    page/recipe_import.mjs
    page/vendor/pdfjs/pdf.min.mjs
    page/vendor/pdfjs/pdf.worker.min.mjs)
set(WEB_GZ "")
foreach(f ${WEB_FILES})
    get_filename_component(name ${f} NAME)
    set(gz ${CMAKE_CURRENT_BINARY_DIR}/${name}.gz)
    add_custom_command(OUTPUT ${gz}
        COMMAND gzip -9 -n -c ${CMAKE_CURRENT_LIST_DIR}/${f} > ${gz}
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/${f}
        VERBATIM)
    list(APPEND WEB_GZ ${gz})
endforeach()
add_custom_target(web_files_gz DEPENDS ${WEB_GZ})
add_dependencies(${COMPONENT_LIB} web_files_gz)
foreach(gz ${WEB_GZ})
    target_add_binary_data(${COMPONENT_LIB} ${gz} BINARY)
endforeach()
