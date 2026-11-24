# Hardware JPEG decoding for LVGL (ESP32-P4 JPEG engine). jpeg_info.cpp is pure logic (host-tested in
# tests/host/test_jpeg_info.cpp); esp/jpeg_decoder_esp.cpp is compiled in CI only and has never run on the board.
idf_component_register(
    SRCS "src/jpeg_info.cpp" "esp/jpeg_decoder_esp.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_jpeg esp_timer heap)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
