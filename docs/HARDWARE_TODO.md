# Hardware TODO (written without the board; test in Phase 1)

Code here compiles in CI but has never run on the device. Do not treat it as working.

| Item | Where | How to test in Phase 1 |
| --- | --- | --- |
| OTA install, download, rollback, health gate | `firmware/services/update/esp/` | Install a catalog build and an upload; break the UI on purpose and confirm rollback after 40 s |
| Health-gate probes (UI heartbeat, frames, touch, I2C scan, SD present) | `app_main.cpp` (unset) | Wire probes once display/touch BSP exists; unplug accelerometer and SD and confirm warnings only |
| Web server on the device | `firmware/web/esp/http_server.cpp` | Needs Wi-Fi bring-up (esp_hosted, WPA2 setup AP + QR); open the portal from a phone |
| Settings in NVS | `firmware/services/nvs_settings/` | Change a setting, reboot, confirm it persists; export/import/reset |
| SD card presence / free space (`storage/sd_present`, `storage/sd_free_mb`) | not written yet (R3) | Insert/remove card; Modules screen requirement text changes |
