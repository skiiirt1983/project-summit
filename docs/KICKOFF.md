# Next steps (handoff — read this first)

Updated after every merge so a new session can continue (D-009).

- **Phase 0 (steps 1–6): done and merged.** Phase 1 waits for hardware.
- **Now: Recipe reader module (D-008), built without the board (D-009).** Work mode: merge each step when CI is green, log defaults in `docs/OPEN_QUESTIONS.md`, hardware-only items in `docs/HARDWARE_TODO.md`. Stop only for the items D-009 lists.

| Step | What | Status |
| --- | --- | --- |
| R1 | Modules screen (device + web UI), module manifest + requirements | done (#7) |
| R2 | `recipe.json` format 1: validator, scaling, fractions, step-time detection, index rebuild | done (#8) |
| R3 | Storage interface + local-folder fake "SD card" for host/simulator | done (#9) |
| R4a | Recipes panel: library + card mode, both orientations | done (#10) |
| R4b | Book mode: page images (JPEG decode in the simulator), zoom, page swipe | done (#11) |
| R5 | Recipe timers, timer chip, "Cooking now" chip | done (#12) |
| R6a | Recipe web API: list, save, streamed file upload/download, inbox | done (#13) |
| R6b | Web UI import page: pdf.js conversion, text parser, edit screen, inbox conversion, sample PDFs, browser test | done (#14) |
| R7 | Recipe URL import: JSON-LD extraction (device fetch coded, untested) | done (#15) |
| D-011 | Updates refuse while a recipe timer runs; URL import shows the new recipe with Edit | done (#19) |
| D-012 S1 | Sleep rules (when to sleep, timed wake, what blocks), RTC snapshot, timers restored after sleep; INT1 moved to GPIO4 in docs | done (#20) |
| D-012 S2 | Power service on the bus, sleep settings, fake sleep/wake in the simulator, device deep-sleep port (compiled only) | done (#21) |
| D-010 | "Delete recipes" button with count confirm (device + web UI); web UI lock withdrawn | done (#16) |
| F1 | Edit a saved recipe in the web UI (also covers fixing URL imports) | done (#17) |
| M1 | Motion and controls: motion tokens, page swipe with both pages moving, rubber band, sheet and screen moves, press feedback, page dots | done (simulator only) |
| F2 | Library thumbnails (`thumb.jpg`) in the Recipes panel (simulator; device needs the JPEG decoder) | done (#18) |

**All planned Recipe reader steps (R1–R7) are built for the simulator.** What is left, in suggested order:

1. Nothing is waiting on Paul: D-010 answered the open items (R1-R3 defaults accepted). Defaults for R4-R7 are in `docs/OPEN_QUESTIONS.md` for review when convenient.
2. No-hardware work is done. D-011 and D-012 are applied; Paul's answers of 2026-11-14 are applied too (face-down on USB = screen off + mute, no deep sleep; URL import = web UI highlight + tappable "New recipe" device toast, never a jump). Remaining sleep defaults are in `docs/OPEN_QUESTIONS.md` ("D-012 — Sleep") for review.
3. Optional before hardware: the remaining motion steps listed under "Not yet built" in ARCHITECTURE.md "Motion and controls" (recipe step moves, dialogs, toast slide, rotation morph).
4. Phase 1 on the board: everything in `docs/HARDWARE_TODO.md` (display/touch BSP and UI loop first; then SD card, Wi-Fi, JPEG decoder, double-tap, audio, deep sleep and the INT1 pin check).

How to work here: branch `recipes/stepN-...` from `main`, `bash tests/run_tests.sh`, build the simulator (`firmware/sim/README.md`), run `summit_sim --selftest`, `node tests/web/e2e.mjs` and `node tests/web/e2e_recipes.mjs`, open a PR, merge when CI is green.

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
