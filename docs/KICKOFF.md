# Kickoff — Phase 0 (before hardware arrives)

Paste this into Claude Code from the repo root:

```
Read CLAUDE.md and docs/ARCHITECTURE.md. Also read github.com/skiiirt1983/LyricFlash for the update/rollback pattern, release workflow and contract tests to reuse.

Phase 0, before hardware arrives:
1. Scaffold the monorepo layout and the 32 MB partition table from the architecture doc.
2. Set up GitHub Actions: host tests, ESP-IDF firmware build for ESP32-P4, OTA + full image packaging.
3. Build the framework layer (event bus, state store, settings with schema version, panel API) with host tests.
4. Set up an LVGL PC simulator at 1280x720 and 720x1280 and build the shell: pager, glance strip, rotation re-layout.
5. Build the update engine shared by the on-device Updates screen and the web UI, including the post-boot health gate and rollback.
6. Build the basic web UI: status, Wi-Fi, firmware (catalog + upload), settings export/import, diagnostics.

Stop after each step and show me what changed.
```

## Phase 1 (when parts arrive)
- Flash a Waveshare demo; confirm display, touch, speaker, mics, Wi-Fi.
- I2C scan with accelerometer unplugged; set LIS2DW12 SA0 to the free address; read WHO_AM_I = 0x44.
- Rotation spike on real hardware: landscape 1280x720, animated swipe, record fps.
- Measure battery charge current and every TBD in the dimensions table.
