# Audio: which sound plays when (pure logic + bus service, host-tested in tests/host/test_audio.cpp) and the ES8311
# codec port for the Waveshare board (esp/audio_esp.cpp, compiled in CI, untested on hardware).
idf_component_register(
    SRCS "src/audio.cpp" "esp/audio_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES framework
    PRIV_REQUIRES esp_driver_i2s esp_driver_gpio esp_timer esp_codec_dev)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
