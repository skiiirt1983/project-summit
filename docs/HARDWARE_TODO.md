# Hardware TODO (written without the board; test in Phase 1)

Code here compiles in CI but has never run on the device. Do not treat it as working.

| Item | Where | How to test in Phase 1 |
| --- | --- | --- |
| OTA install, download, rollback, health gate | `firmware/services/update/esp/` | Install a catalog build and an upload; break the UI on purpose and confirm rollback after 40 s |
| Health-gate probes (UI heartbeat, frames, touch, I2C scan, SD present) | `app_main.cpp` (unset) | Wire probes once display/touch BSP exists; unplug accelerometer and SD and confirm warnings only |
| Web server on the device | `firmware/web/esp/http_server.cpp` | Needs Wi-Fi bring-up (esp_hosted, WPA2 setup AP + QR); open the portal from a phone |
| Settings in NVS | `firmware/services/nvs_settings/` | Change a setting, reboot, confirm it persists; export/import/reset |
| SD card mount, hot-plug, free space (`storage/sd_present`, `storage/sd_free_mb`) | `firmware/services/storage/esp/sd_card.cpp` | Assumes SDMMC slot 0 on default P4 pins, 4-bit, on-chip LDO channel 4: check against the schematic. Insert/remove a FAT32 card and watch the Modules screen; confirm a card is never formatted; time reading a 300 KB page image |
| Writes on FAT are not atomic (remove + rename) | `firmware/framework/src/storage.cpp` | Pull power while saving a recipe; the old or new file must survive and `index.json` must rebuild |
| Double-tap on the case → `input/double_tap` | not written (needs the LIS2DW12 driver) | Configure the tap interrupt on INT1; double-tap with messy hands; check no false triggers from chopping on the same counter |
| Screen stays on while cooking (`ui/keep_awake`, 30 min safety timeout) | panel sets the flag; the power service that honours it is not written | Open a recipe, wait past the idle-dim time; confirm it dims after 30 min untouched and returns to normal after leaving the page |
| Recipes panel on the real display (text sizes, touch targets, keyboard) | `firmware/panels/recipes/src/panel.cpp` (simulator only so far) | Use it at arm's length in both orientations |
| Page images on the device (book mode) | not written: needs an LVGL file-system driver for `/sdcard` and a JPEG decoder (ESP32-P4 hardware JPEG) passed to `RecipesPanel` as its image source | Open a 3-page recipe; time page turns (target well under 1 s); check memory with a 905x1280 page decoded in PSRAM |
| Pinch to zoom | not written (multi-touch gestures) | Pinch on a page with two fingers; compare with the Zoom button |
| Timer ringing sound (`timers/ringing`) | service sets the flag; no audio service yet | Let a timer end on battery with the screen dimmed: it must ring through the speaker and wake the screen |
| UI loop on the device (bus dispatch, `TimerService::tick`, shell tick) | not written: starts with the display BSP | Timers count down and fire on the device |
| Recipe uploads/downloads over Wi-Fi (streamed to/from the SD card) | `firmware/web/esp/http_server.cpp`, `firmware/panels/recipes/src/web_api.cpp` | Import a 10-page PDF from a phone; time it; pull the card mid-upload and check nothing half-written remains |
| Web import from a phone browser (pdf.js served from flash, ~510 KB gzipped) | `firmware/web/page/recipes.mjs`, `firmware/web/esp/http_server.cpp` static files | Import a real multi-page cookbook PDF from iOS Safari and Android Chrome; check memory on the phone and time per page |
