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

## Enclosure
Case design brief: `hardware/enclosure/ENCLOSURE.md` (single source of truth for the case). Update it first, then the CAD.
**Starting a case session? Read `hardware/enclosure/CLAUDE.md` first**: it holds the current status, the decisions, how Paul
likes to work, what went wrong before, and a starter prompt.

## Project status (2026-11-20)
- **Firmware:** Phase 0 steps 1 to 6 are merged to `main` (monorepo and partition table, CI, framework, LVGL simulator
  and shell, update engine with rollback, web UI). Phase 1 (real hardware: demo flash, I2C scan, rotation spike) needs
  the board; Paul now has the board but not yet the battery or accelerometer.
- **Enclosure:** designed and modelled; fit testing on coupons is nearly done; no full case printed yet. Details in
  `hardware/enclosure/CLAUDE.md`.
- Paul keeps decisions in the Claude design doc (see rule 10 above) and answers questions best with pictures and
  A/B/C choices.

