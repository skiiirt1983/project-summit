# Working on Project Summit locally

For picking the project up on your own computer (written 2026-11-24, when Paul moved from the cloud sessions to a local clone). Everything you need to build, test and release is in the repo; the design is in `docs/ARCHITECTURE.md` and the running state of the project is in the "Current state and handoff" section of `CLAUDE.md` (Claude Code reads that file by itself when you start it in the repo).

## 1. Get the code

    git clone https://github.com/skiiirt1983/project-summit.git
    cd project-summit

`main` is the only long-lived branch and always builds. The way work has been done: make a branch, push it, open a pull request, wait for the four CI checks (host-tests, simulator, firmware rev1, firmware rev3), merge, and dispatch the release workflow when a build should reach the device. CI skips changes that only touch `CLAUDE.md` or the status docs.

## 2. Host tests (no board, no ESP-IDF; under a minute)

Needs `python3`, `node`, `g++` and `gcc` (Linux, macOS or WSL on Windows). Nothing else.

    bash tests/run_tests.sh

This runs the Python contract tests, the recipe parser test and about 20 C++ test programs (event bus, settings, shell logic, update engine, web API, recipes, timers, alarms and stopwatch, audio, JPEG header reader, ...). Run it before every push. A new test file must be added to the `for t in ...` loop in `tests/run_tests.sh`; a new library source file must be added to `srcs_all` in the same file.

## 3. Simulator (optional: the shell and placeholder panels on your PC)

See `firmware/sim/README.md`. Needs cmake, ninja, `zlib1g-dev`, `libjpeg-turbo8-dev` (and `libsdl2-dev` for a window).

    cmake -S firmware/sim -B build/sim -G Ninja && cmake --build build/sim
    build/sim/summit_sim --selftest --out sim-out      # what CI runs
    build/sim/summit_sim --sdl                         # a window

## 4. Firmware build (needs ESP-IDF 5.5.4)

CI builds with ESP-IDF **v5.5.4** (`espressif/esp-idf-ci-action`). Install it (Espressif's installer or the VS Code extension), open an ESP-IDF shell, then:

    cd firmware
    idf.py set-target esp32p4
    bash ../tools/ci_build.sh rev3          # rev3 = ESP32-P4 chip revision 3.x (Paul's board); rev1 is for v1.x chips

Output: `firmware/build/summit-full.bin` (the full USB image: bootloader, partition table, app in `ota_0`) and the app image for OTA. A local build is version 0.0.1. Components (LVGL, esp_codec_dev, esp_hosted, ...) are downloaded on the first build.

## 5. Flashing the board by USB

    python -m esptool --chip esp32p4 -p <your serial port> write_flash 0x0 firmware/build/summit-full.bin

(The USB-to-UART port of the Waveshare board, not the power-only port.) A USB flash is needed whenever the **bootloader** changes. Since alpha.40 it has a special setting (see section 7), so a device flashed before that must be USB-flashed once before it can take images larger than 4,063,232 bytes over the air. Everything else can go over the air.

## 6. Releases (reach the device over the air)

On GitHub: Actions, "Release (alpha)" (`release.yml`), "Run workflow" on `main`, and give a **new** version such as `0.0.2-alpha.42` (releases are immutable: never reuse one). It builds rev1 and rev3, publishes the OTA images and `catalog.json` to the `gh-pages` branch, and uploads the full USB images as the workflow artifacts (`release-rev3`). The device checks `https://skiiirt1983.github.io/project-summit/catalog.json`; Settings > Updates on the device installs it.

## 7. Things that bite (all learned on the real board)

- **Never edit `firmware/partitions.csv`.** It is fixed from the first build; changing it breaks over-the-air updates (`tests/test_partitions.py` guards it).
- **Images over 4,063,232 bytes in `ota_1`** reach past 16 MB of the 32 MB flash. ESP-IDF keeps CPU cache reads above 16 MB off by default; `sdkconfig.defaults` turns them on (`CONFIG_IDF_EXPERIMENTAL_FEATURES` and `CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH`). Do not remove them. The Diagnostics tab has a "Flash self-test" that checks it.
- **Never enable `CONFIG_SPI_FLASH_AUTO_SUSPEND`** (the board panics at boot).
- The firmware compiler is strict (`-Werror`, including `format-truncation`): code that compiles on the host can fail in CI. `g++ -O2 -Wall -Wextra -Werror=format-truncation -c file.cpp` finds most of it before pushing.
- Device-only code (`firmware/**/esp/`, `firmware/bsp`, `firmware/main`) is compiled by CI only. Say what was and was not tested on the board.
- Sounds are generated: `python3 tools/make_sounds.py` (needs `ffmpeg`) rebuilds `firmware/services/audio/src/sounds_data.cpp` from `firmware/services/audio/assets/`; never edit that file by hand.
- `docs/ARCHITECTURE.md` "State topics" must list every state topic, bus event and setting key used (checked by `tests/test_state_topics_contract.py`).

## 8. How the board is checked

Paul flashes a build and reports what he sees. Prefer the web UI **Diagnostics** tab (open the device's address in a browser) to pasting logs: it shows load, heap, reset reason, the JPEG and flash self-test lines and has buttons for sound, Wi-Fi chip probe and flash tests. `docs/HARDWARE_TODO.md` lists everything that is written but not yet confirmed on hardware, with how to test each.

## 9. What is queued next

The system Timer / Stopwatch / Alarm (Paul's design: `docs/TIMER_BRIEF.md`). Done: the logic (`firmware/services/timers`: numbered timers, stopwatch, alarms), the service on the event bus and state store (`ClockToolsService`), alarm sounds through the audio service. Still to build, in this order:
1. **Clock page**: tapping the clock on Home opens a page with four areas (Clock top left, Alarm top right, Timer bottom left, Stopwatch bottom right), each tappable to start or change. The Home panel is the placeholder in `firmware/sim/demo_panels.cpp` (shared by the simulator and the device); the page is a new panel or screen using only the state topics and events listed in `docs/ARCHITECTURE.md` (`stopwatch/*`, `alarm/*`, `timers/*`). Size tokens only, both orientations.
2. **Tray items**: a running timer and a set alarm each get an item in the top strip (a glance chip; see `GlanceChipDef` in `firmware/framework/include/summit/panel_api.h` and how `timers/chip_text` is shown). Tapping a timer item opens the timer page, an alarm item the alarm page.
3. **Countdown overlay**: for the last 10 seconds of a timer or alarm, a semi-transparent frosted box over most of the screen (a darkened, shrunken snapshot of the page behind it) counts down; a touch opens that timer's page to stop it; ringing is stopped by touching the screen.
4. Then double-tap, deep sleep (`docs/HARDWARE_TODO.md`), and the parts-dependent work (accelerometer, battery) in `CLAUDE.md` "Where we are going".
