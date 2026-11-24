# CLAUDE.md — Project Summit (SLOPEFORM Home)

Read `docs/ARCHITECTURE.md` before any work. It is the design source of truth.
Start with `docs/KICKOFF.md` (Phase 0 tasks).

## What this is
Firmware for the Waveshare ESP32-P4-WIFI6-Touch-LCD-5: a battery-portable, auto-rotating 5" home dashboard.
Private alpha repo `project-summit`; public release at 1.0 as `slopeform-home`. Licence: GPL-3.0.

## Stack
- C++17, ESP-IDF 5.5.x, LVGL 9.x, FreeRTOS. No MicroPython, no Rust.
- Single self-contained device. No companion server. The device calls cloud/LAN services directly.
- Wi-Fi is via the ESP32-C6 co-processor (esp_hosted over SDIO).

## Hard rules
1. The partition table in `docs/ARCHITECTURE.md` is fixed from v0.1. Never change it after the first build: it breaks OTA for every device.
2. Panels talk only to the framework (state store, event bus, settings). Never to drivers or the network directly.
3. Panels lay out with size tokens, never hard-coded pixels; every panel must work at 1280x720 and 720x1280.
4. Settings carry a schema version. Adding a key with a default needs no version bump: older firmware ignores unknown keys and preserves them on write. Renames, removals, type or meaning changes bump the version and need a migration + host test.
5. Every update path (on-device, web UI) goes through the one update engine. Keep the post-boot health gate and rollback (pattern: LyricFlash `main/ota_guard.c`).
6. Never commit secrets. Use `secrets.example.json` only.
7. Releases are immutable. Never overwrite a published tag or OTA image.
8. A green CI build is not hardware proof. Say clearly what was tested on hardware and what was not.
9. No code copied from PrintSphere until its licence is confirmed GPL-3.0 compatible.
10. Design/architecture decisions are made in the Claude design doc, not in code. If a task needs a design change, stop and ask Paul.

## Reference projects
- LyricFlash (github.com/skiiirt1983/LyricFlash): OTA, rollback, web UI firmware page, catalog, USB preserve-settings flasher, contract tests, release workflow. Reuse the patterns, adapt for ESP32-P4.
- PrintSphere (github.com/cptkirki/PrintSphere): Bambu printer client. Licence check pending.

## Working style
- Small steps. Stop after each step and show what changed.
- Host-side tests for all pure logic (as LyricFlash does in `tests/`).
- Paul prefers concise, direct updates.

## Current state and handoff (updated 2026-11-24)
Read this section first when taking over. Phase 1 (real hardware) is well under way; Paul flashes builds and pastes results. The repo is public. Latest release: `0.0.2-alpha.35` (dispatched 2026-11-24; check the catalog). Merged so far this stretch: PRs 50-53 (Wi-Fi fixes, audio).

### Working agreements with Paul
- Paul has a limited daily allowance and dislikes circles: commit and push after every small step, keep `docs/KICKOFF.md`, `docs/OPEN_QUESTIONS.md` and `docs/HARDWARE_TODO.md` current, never leave the tree red or uncommitted. Run the full host suite (`bash tests/run_tests.sh`, about 8 minutes: run it in the background and wait for it) before pushing, so CI is not the first place a test fails. Do not poll CI: subscribe to the PR and wait for the event.
- Standing permission: do the GitHub work yourself (open PR, subscribe, merge when all four checks are green, dispatch the release workflow). After each merged PR restart the dev branch from main: `git fetch && git checkout -B <your dev branch> origin/main && git push --force-with-lease`. The dev branch is whatever the session designates (it was `claude/upbeat-hamilton-7h6lnw` for the last stretch).
- Paul flashes and reports back (the LyricFlash way); he has no git. USB flash: CI artifact `summit-firmware-rev3-<sha>` contains `summit-<ver>-rev3-full.bin` (write at 0x0). Over-the-air: `-ota.bin` via the catalog. CI builds are version 0.0.1; only the release workflow (Actions > "Release (alpha)", `release.yml`, input a new SemVer, run on `main`) builds the real version and publishes to GitHub Pages (`gh-pages`, catalog at https://skiiirt1983.github.io/project-summit/catalog.json). Releases are immutable: always use a new version (next: `0.0.2-alpha.36`).
- Say plainly what was and was not tested on hardware. Do not change two things at once when chasing a hardware bug.
- Paul wants minimal log pasting: report with the web UI Diagnostics tab or one `diag:` log line (see below). The log from before a restart is kept at the top of the web Log tab (RAM, or the SD card after a power cut).
- Never put Paul's email or personal details into outside requests.
- Paul's product rules: the device is quiet and carefree (only critical things and alarms make a sound); no customer soldering or special tools for any recovery or update path; the product must not depend on an SD card.

### What works on the board (confirmed by Paul)
Display (HX8394 720x1280), GT911 touch (polled), boot/loading screen, shell with swipe/pull-down motion, brightness and orientation, Wi-Fi through the ESP32-C6 (esp_hosted over SDIO), SNTP clock, web UI (Wi-Fi, Log, Diagnostics, Recipes, settings), setup access point `Summit-Setup` / `summit123`, OTA update check/download/install with health gate and rollback, install screen hidden during flash writes (no flashing), auto-restart after install, software version in quick settings, silent/do-not-disturb, SD card mount and hot-plug, settings persistence, recipe import from a website and from PDF, SD rolling log that survives a power cut.
**Wi-Fi after an outage (alpha.33, confirmed):** a 2-minute outage rejoins by itself; after 5 minutes without the saved network the setup network appears, a phone can join it and open `http://192.168.4.1`, and the device rejoins almost at once after the phone leaves. Design (do not undo): the C6 has one radio and its old firmware cannot serve the setup network and a router join together (every join failed with reason 15 and phones could not join either, alpha.32). So they take turns in `firmware/services/net/esp/wifi_service.cpp`: no join attempts while the setup network is up; with no phone on it, it steps aside for a 20 s router-only window every 60 s; with a phone on it nothing is attempted; with no saved network it never steps aside.

### Waiting for Paul (hardware tests of alpha.35, the first build with sound)
Audio is `firmware/services/audio` (rules host-tested, 895 checks; the ES8311 port `esp/audio_esp.cpp` is compiled in CI only and has NEVER run on the board). Ask Paul to report on these four things:
1. Web UI Diagnostics tab: Test tick, Test warning, Test USB plug-in and Test alarm (cowbell) buttons each play through the speaker (loud enough, clean, no crackle or hiss when idle).
2. Quick settings volume slider: release it and a short tick plays at that volume; volume 0 is silent for everything except the alarm.
3. A recipe step timer rings with the cowbell and stops within a moment when dismissed.
4. Nothing else regressed (Wi-Fi, touch, display, OTA) and the `audio:` log lines show `ES8311 ready` / `speaker on` / `speaker off`. If a sound is silent, ask for those log lines.
Also waiting on: the Waveshare tech-support reply (flash auto-suspend crash, C6 recovery); parts on order (accelerometer, battery).
Sound rules (Paul, 2026-11-24): warning hit plays on critical toasts only (not while a timer alarm rings), USB plug-in sound when `power/usb_power` goes off to on (cannot fire yet: nothing sets that topic until the battery/USB readings exist), cowbell is the default timer alarm (never below 40% volume, ignores do-not-disturb, stops when dismissed); ordinary toasts are silent. Sounds are built in as 8-bit mu-law from `firmware/services/audio/assets/` by `tools/make_sounds.py` (regenerates `src/sounds_data.cpp`: never hand-edit it). Licences: cowbell free (no attribution), the other two made by Paul with AI tools.

### Open problems and parked items
1. **Screen flashing during an install: solved by hiding it** (panel display-off during the write, lit progress blips). Never enable `CONFIG_SPI_FLASH_AUTO_SUSPEND` (panics at boot on this board). Remaining polish is on the end-list below.
2. **C6 co-processor firmware is old** (reports 0.0.0, esp_hosted "version mismatch" warning; streaming SDIO mode in use). Paul, 2026-11-24: ignore it until it presents a problem. Background in `docs/WIFI_CHIP_UPDATE.md` (probe result: begin ok, end ERR_INVALID_ARG; recovery header J7, no soldering for customers). The Waveshare repo has no slave source or pin map.
3. **SD card:** works; the SD log mirror is a development aid and is to be removed before release.
4. Decisions waiting in `docs/OPEN_QUESTIONS.md`: show the neighbouring page while dragging a swipe; the 40% alarm minimum (Paul can say if a timer should be silent at volume 0); the remaining defaults.
5. Minor: scan right after connect gives ESP_ERR_WIFI_STATE (harmless); a 404 on the stable channel should read friendlier; battery/USB values are placeholders; timezone is UTC only; PowerService::tick is not called (no sleep yet). A stray remote branch `claude/upbeat-hamilton-7h6lnw-tests` exists (Paul can delete).

### Diagnostics (so Paul does not paste logs)
`diag/metrics` (state topic, documented in ARCHITECTURE.md) is refreshed every 5 s by the UI loop in `firmware/main/app_main.cpp`: LVGL busy %, peak fps, panel strips/s, worst LVGL pass ms, internal heap and lowest ever, PSRAM free, uptime, pending_verify, reset reason. It shows on the web Diagnostics tab and as one `diag:` log line every 30 s. When you want a new measurement, add a key there and document it rather than asking for full logs. On boot the log also prints "RESTARTED after: ..." and `CRASH DETAILS` after a crash; `tools/ci_build.sh` prints addr2line for `tools/crash_addrs.txt`.

### Hardware and build gotchas learned
- ESP-IDF 5.5.4, LVGL 9.4, esp_hosted 2.13.x, esp_wifi_remote 1.6.x (pinned in `firmware/services/net/esp/idf_component.yml`; a Kconfig shim defines the esp_hosted 3.x manifest symbols). Audio uses `espressif/esp_codec_dev` ~1.6.2 (component manifest in `firmware/services/audio/idf_component.yml`); component registry downloads work from this container (`components.espressif.com`), handy for reading headers before writing driver code.
- Vendor C macros and designated-initialiser structs (`HX8394_*_DPI_CONFIG`, `ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG`, I2S/codec configs) do not compile in C++: zero-init and set fields by hand. The HX8394 driver must skip its own I2C init (`CONFIG_ESP_LCD_HX8394_SKIP_I2C_INIT=y`) or it steals GPIO7/8 from the touch bus.
- Board pins (Waveshare repo `docs/IO.md`): I2C SDA 7 / SCL 8 shared by ES8311 (0x18; esp_codec_dev wants the 8-bit 0x30), ES7210 mic ADC (0x40) and GT911 (0x5D); I2S MCLK 13, BCLK 12, WS 10, DOUT 9, DIN 11; speaker amplifier enable GPIO53 (active high); LCD reset 27, backlight 26; SD on GPIO39-44. No IMU, RTC or battery gauge is documented anywhere in that repo (parts are on order: use their datasheets).
- `uint32_t` is `long` on RISC-V: cast to `unsigned` for `%u` (CI uses -Werror=format). RISC-V core dumps have no backtrace array. Beware unsigned underflow in time arithmetic (a "future" timestamp subtracted from now): a host test caught one in the audio idle timer.
- FreeRTOS tick is 1 ms (`CONFIG_FREERTOS_HZ=1000`); `vTaskDelay(pdMS_TO_TICKS(x))` can round to 0 at coarser ticks and starve the idle task.
- LVGL: `lv_obj_add_flag/remove_flag(HIDDEN)` and similar invalidate on every call (guard with `ui_motion::set_hidden`); `lv_layer_top()` cannot be un-hidden (use opacity); the DMA2D done callback calls `lv_display_flush_ready`.
- Internal RAM is tight: LVGL draw buffers are 20 lines in internal DMA RAM, small allocations go to PSRAM (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=2048`); low internal heap made the esp_hosted `sdio_read` task abort.
- The update engine must pick the catalog entry for this chip revision (`Engine::installable`); a version lists both rev1 and rev3 builds.
- `docs/ARCHITECTURE.md` "State topics" must list every state topic, bus event and setting key used (contract test `tests/test_state_topics_contract.py`); a new web API route also needs a row in the `known[]` list in `firmware/web/src/api.cpp`.
- Host tests: `bash tests/run_tests.sh` (about 8 minutes; run in the background). Simulator: `firmware/sim/README.md`; self-test `summit_sim --selftest`, web e2e `node tests/web/e2e.mjs`. New host test files must be added to the loop in `tests/run_tests.sh`.

### Where we are going
Direction: a battery-portable, auto-rotating 5" home dashboard that is quiet, carefree and self-contained. Order of work (Paul, 2026-11-24: "lots of stuff to build"; the recipe card redesign waits for the end):
1. Verify audio on hardware (the four tests above), fix what they show.
2. Build the rest of Phase 1 that needs no parts: hardware JPEG decoder (P4 has one; needed for recipe photos and page thumbnails), double-tap, deep sleep and `PowerService::tick` (`docs/HARDWARE_TODO.md`).
3. When the parts arrive: accelerometer rotation (LIS2DW12; I2C 0x18 clashes with the ES8311, scan before wiring SA0), battery/charge and USB readings (this also makes the USB plug-in sound work), INT1 pin check.
4. Real data panels: weather, calendar, printer client (PrintSphere code only after its licence is confirmed GPL-3.0 compatible).
5. At the end, before 1.0 and the public release as `slopeform-home`: the **recipe card redesign** (`docs/RECIPE_CARD.md`: bigger centred title, card photo chosen from images pulled out of the recipe, description, page-count badge, title-only fallback; same card size), then the **firmware polish** list (`docs/KICKOFF.md`, item 8: confirm-blink, pulsing write screen vs steady lit screen, reboot flicker, rotation toast, remove the `debug/display_flush` and `debug/backlight` settings), remove the SD log mirror, and the optional C6 firmware update if it has become a problem.
Day-to-day the alpha path is: PR, CI green, merge, dispatch the release workflow with a new version.
