# firmware

ESP-IDF 5.5.x project for the ESP32-P4 (C++17, LVGL 9).

    cd firmware && idf.py set-target esp32p4 && idf.py build

`partitions.csv` is fixed from v0.1. Never edit it (guarded by `tests/test_partitions.py`).
