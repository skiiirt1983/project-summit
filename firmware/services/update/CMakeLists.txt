# Update engine (hard rule 5): pure logic in src/ (host-tested in tests/host/test_update.cpp),
# ESP-IDF adapters in esp/ (OTA writer, SHA-256, HTTPS, catalog JSON, health gate task).
idf_component_register(
    SRCS "src/version.cpp" "src/catalog.cpp" "src/preflight.cpp" "src/image_check.cpp"
         "src/engine.cpp" "src/health_gate.cpp"
         "esp/esp_backends.cpp" "esp/catalog_json.cpp" "esp/update_service.cpp" "esp/ota_guard.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "esp"
    REQUIRES framework
    PRIV_REQUIRES app_update esp_http_client esp-tls mbedtls json esp_timer esp_partition esp_app_format esp_system)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
