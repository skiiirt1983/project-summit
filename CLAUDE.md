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

## Current state and handoff (updated 2026-11-22)
Read this section first when taking over. Phase 1 (real hardware) is under way; Paul flashes builds and pastes results. The repo is public.

### Working agreements with Paul
- Paul has a limited daily allowance: commit and push after every small step, keep `docs/KICKOFF.md`, `docs/OPEN_QUESTIONS.md` and `docs/HARDWARE_TODO.md` current, never leave the tree red or uncommitted.
- Standing permission: do the GitHub work yourself (open PR, subscribe, merge when CI is green, dispatch the release workflow). After each merged PR restart the dev branch: `git fetch && git checkout -B claude/admiring-tesla-1y0clg origin/main`.
- Paul flashes and reports back (the LyricFlash way); he has no git. USB flash: CI artifact `summit-firmware-rev3-<sha>` contains `summit-<ver>-rev3-full.bin` (write at 0x0). Over-the-air: `-ota.bin` via the catalog. CI builds are version 0.0.1; only the release workflow (Actions > "Release (alpha)", input a new SemVer) builds the real version and publishes to GitHub Pages (`gh-pages`, catalog at https://skiiirt1983.github.io/project-summit/catalog.json). Releases are immutable: always use a new version.
- Say plainly what was and was not tested on hardware. Do not change two things at once when chasing a hardware bug.
- Paul wants minimal log pasting: report with the web UI Diagnostics tab or one `diag:` log line (see below). The log from before a restart is kept at the top of the web Log tab.
- Never put Paul's email or personal details into outside requests.

### What works on the board (confirmed by Paul)
Display (HX8394 720x1280, 40 MHz pixel clock), GT911 touch (polled, INT/RST not connected), boot screen (media kit, portrait by default), shell with swipe/pull-down motion (25-45 fps), live brightness slider, orientation buttons, Wi-Fi through the ESP32-C6 (esp_hosted over SDIO), SNTP clock, web UI (Wi-Fi settings, Log tab with copy/download, Diagnostics), setup access point `Summit-Setup` / `summit123` (isolated: no IP forwarding/NAPT), update check over TLS from GitHub Pages, update download and install (no more crash), idle CPU low (LVGL ~0-3%).

### Open problems, in priority order
1. **OTA update and health gate: FIXED and confirmed on hardware** (alpha.7 flashed by USB, alpha.8 installed over the air, "updated", `pending_verify no`, booted from `ota_1`). The gate probes were wired in PR #28.
2. **Screen flashing during an install: SOLVED by hiding it (alpha.23, confirmed on hardware).** Cause: flash erase/write freezes the cache and starves the DSI panel's PSRAM reads. The write takes about 21 s (chip-bound; up-front block erase did not help). Fix: the panel gets its display-off command during the write (`display/panel_off`) and the write runs in 2 s slices with a lit progress blip between them (about 44 s total, `update/write_pct`). Tried and rejected: backlight-only blanking (faint glitching still visible), PSRAM buffering (kept: no glitching during the download), `CONFIG_SPI_FLASH_AUTO_SUSPEND` (alpha.24/25 panicked at boot on this board and rolled back: never enable it). The health-gate confirm blanks the light for about 0.3 s on the first boot of a new image (a flash write); the boot screen waits for it. Remaining: the blue/dim flash while the chip restarts (no DSI signal, not fixable in software), and the `debug/display_flush` and `debug/backlight` settings can be removed in a cleanup PR. Hardware test loop without new builds: Previous version, then Install the newer build again.
3. **C6 co-processor firmware is old** (reports 0.0.0, esp_hosted "version mismatch" warning; streaming SDIO mode is in use, plain mode broke HTTPS). Future: update it over the existing link from the host (esp_hosted slave OTA), no soldering or jumpers (Paul's hard requirement). Needs CI to build the esp_hosted slave for esp32c6, a web UI "Update Wi-Fi chip" button, and Paul's go-ahead. Then restore plain/fast link settings.
4. **SD card untested.** After the first clean update Paul powers off, inserts a FAT32 card, and reports the `sd_card` log lines; then verify recipes. SD retries every 5 min after 3 failed mounts.
5. Decisions waiting for Paul (see `docs/OPEN_QUESTIONS.md`): show the neighbouring page while dragging a swipe, plus the remaining defaults listed there.
6. Minor: scan right after connect gives ESP_ERR_WIFI_STATE (harmless); a 404 on the stable channel should read friendlier; volume slider does nothing (no audio driver yet); battery/USB values are placeholders; timezone is UTC only; PowerService::tick is not called (no sleep yet); the log is also kept on the SD card (`/logs/log-NNNNNN.txt`, 256 KB files, a file is deleted once two newer ones exist, written within 2 s of a warning or error and otherwise every minute; the web UI cannot clear it) and shown at the top of the Log tab after a power cut or reset (alpha.31; needs a card, no card = the old RAM-only behaviour).

### Diagnostics (so Paul does not paste logs)
`diag/metrics` (state topic, documented in ARCHITECTURE.md) is refreshed every 5 s by the UI loop in `firmware/main/app_main.cpp`: LVGL busy %, peak fps, panel strips/s, worst LVGL pass ms, internal heap and lowest ever, PSRAM free, uptime, pending_verify, reset reason. It shows on the web Diagnostics tab and as one `diag:` log line every 30 s. When you want a new measurement, add a key there and document it rather than asking for full logs. On boot the log also prints "RESTARTED after: ..." and `CRASH DETAILS` (task, pc, ra, sp, mcause, mtval) after a crash; `tools/ci_build.sh` prints addr2line for `tools/crash_addrs.txt`.

### Hardware and build gotchas learned
- ESP-IDF 5.5.4, LVGL 9.4, esp_hosted 2.13.x, esp_wifi_remote 1.6.x (pinned in `firmware/services/net/esp/idf_component.yml`; a Kconfig shim defines the esp_hosted 3.x manifest symbols).
- Vendor C macros (`HX8394_*_DPI_CONFIG`, `ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG`) do not compile in C++: set struct fields by hand. The HX8394 driver must skip its own I2C init (`CONFIG_ESP_LCD_HX8394_SKIP_I2C_INIT=y`) or it steals GPIO7/8 from the touch bus.
- `uint32_t` is `long` on RISC-V: cast to `unsigned` for `%u` (CI uses -Werror=format). RISC-V core dumps have no backtrace array.
- FreeRTOS tick is 1 ms (`CONFIG_FREERTOS_HZ=1000`); `vTaskDelay(pdMS_TO_TICKS(x))` can round to 0 at coarser ticks and starve the idle task.
- LVGL: `lv_obj_add_flag/remove_flag(HIDDEN)` and similar invalidate on every call (guard with `ui_motion::set_hidden`); `lv_layer_top()` cannot be un-hidden (use opacity); the DMA2D done callback calls `lv_display_flush_ready`.
- Internal RAM is tight: LVGL draw buffers are 20 lines in internal DMA RAM, small allocations go to PSRAM (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=2048`); low internal heap made the esp_hosted `sdio_read` task abort.
- The update engine must pick the catalog entry for this chip revision (`Engine::installable`); a version lists both rev1 and rev3 builds.
- `docs/ARCHITECTURE.md` "State topics" must list every state topic used (contract test `tests/test_state_topics_contract.py`).
- Host tests: `bash tests/run_tests.sh` (takes a few minutes; run in the background). Simulator: `firmware/sim/README.md`; self-test `summit_sim --selftest`, web e2e `node tests/web/e2e.mjs`.

### Where we are going
Finish Phase 1 in this order: confirm a clean OTA update (1) then fix flashing (2) then SD card and recipes (4) then C6 firmware update without soldering (3) then audio, accelerometer rotation, battery/charge readings, JPEG decoder for recipe pages and thumbnails, double-tap, deep sleep, INT1 pin check (`docs/HARDWARE_TODO.md`). Then real data panels (weather, calendar, printer client; PrintSphere code only after its licence is confirmed GPL-3.0 compatible), then 1.0 and public release as `slopeform-home`.
