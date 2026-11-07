# firmware

ESP-IDF 5.5.3+ project for the ESP32-P4 (C++17, LVGL 9). Two chip-revision variants:
`rev3` (ESP32-P4 v3.x, default) and `rev1` (v1.x). A build for one will not boot on the other.

    cd firmware && idf.py set-target esp32p4
    bash ../tools/ci_build.sh rev3     # or rev1

The main app goes in `ota_0` with an otadata that selects it; `factory` is for the recovery app only.
`partitions.csv` is fixed from v0.1. Never edit it (guarded by `tests/test_partitions.py`).
