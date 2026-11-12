# Next steps (handoff — read this first)

Updated after every merge so a new session can continue (D-009).

- **Phase 0 (steps 1–6): done and merged.** Phase 1 waits for hardware.
- **Now: Recipe reader module (D-008), built without the board (D-009).** Work mode: merge each step when CI is green, log defaults in `docs/OPEN_QUESTIONS.md`, hardware-only items in `docs/HARDWARE_TODO.md`. Stop only for the items D-009 lists.

| Step | What | Status |
| --- | --- | --- |
| R1 | Modules screen (device + web UI), module manifest + requirements | done (#7) |
| R2 | `recipe.json` format 1: validator, scaling, fractions, step-time detection, index rebuild | done (#8) |
| R3 | Storage interface + local-folder fake "SD card" for host/simulator | next |
| R4 | Recipes panel: library, card mode, book mode, both orientations | todo |
| R5 | Recipe timers, timer chip, "Cooking now" chip | todo |
| R6 | Web UI import: PDF upload, pdf.js conversion, edit screen, inbox | todo |
| R7 | Recipe URL import: JSON-LD extraction (device fetch coded, untested) | todo |

How to work here: branch `recipes/stepN-...` from `main`, `bash tests/run_tests.sh`, build the simulator (`firmware/sim/README.md`), run `summit_sim --selftest` and `node tests/web/e2e.mjs`, open a PR, merge when CI is green.

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
