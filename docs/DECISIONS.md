# Decisions from Paul (via the Claude design session)

Newest first. Each entry answers a question Claude Code asked. Apply it, update ARCHITECTURE.md where noted, then continue.

## D-012 — Low-power sleep that wakes for timers and alarms; accelerometer interrupt pin (2026-11-13)

### Sleep mode
- **Goal:** on battery, the device sleeps (screen off, Wi-Fi off, ESP32-P4 deep sleep) whenever nothing needs it, and wakes itself on time for timers and alarms. A 4-hour recipe timer must ring on time from sleep.
- **Enter sleep:** after the idle timeout (setting, default 5 min) with no touch, or face-down for 30 s; never while a recipe is open with the screen-on rule active, while an update runs, or while on USB power if the user has chosen "stay on when charging" (setting, default on).
- **Before sleeping:** save running timers and the next alarm to RTC memory (survives deep sleep) and set the RTC timer wake for the earliest due item minus a wake margin (default 5 s; measure the real wake-to-screen time on the board and set the margin to cover it).
- **Wake sources:** RTC timer (timer/alarm due), LIS2DW12 INT1 (pick-up / double-tap), power button. Note which one woke us.
- **After wake:** fast path. Timer/alarm wake → bring up screen + audio and ring before starting Wi-Fi. Pick-up/tap wake → show the clock immediately, then start Wi-Fi and refresh data in the background. Show stale data with its age rather than a blank panel.
- **Timers across sleep:** they survive deep sleep (RTC memory + RTC timer keep counting). This replaces the "memory only until the RTC is proven" interim in D-011 for the sleep case. Full power loss (battery dead or disconnected) still follows D-011 (needs the external RTC; HARDWARE_TODO).
- **Wi-Fi while asleep:** none. Optional later: a periodic wake (e.g. every 30 min) to refresh weather/calendar, off by default.
- **Testing:** host tests for the sleep scheduler (earliest due item, wake margin, what blocks sleep). Simulator: fake sleep/wake. Real deep-sleep current, wake time and C6 power-down are hardware items for HARDWARE_TODO.md.

### Accelerometer interrupt pin change (wiring, not yet wired)
- Only LP GPIOs (GPIO0–15 on the ESP32-P4) can wake the chip from deep sleep. GPIO21 cannot.
- **Move LIS2DW12 INT1 from GPIO21 to GPIO4** (proposed; on the 40-pin header per the schematic net list). Optional INT2 → GPIO5 instead of GPIO22.
- Confirm against the ESP32-P4 datasheet LP GPIO list and the board silkscreen in Phase 1 before wiring; if GPIO4/5 aren't usable, pick another free GPIO in 0–15. Add this check to HARDWARE_TODO.md.
- Update ARCHITECTURE.md (wiring table, sleep section), CLAUDE.md if it names the pin, and any code constant that uses GPIO21.

## D-011 — Recipe timers across restarts; URL import review (2026-11-13)

1. **Timers across a restart: agree with the proposal.**
   - Until the RTC is proven on the board, timers stay in memory only. Add "persist timers" to HARDWARE_TODO.md.
   - Once the RTC works: store running timers in a small NVS entry outside settings, written only on start/cancel. On start-up, a timer that ended while the device was off shows a toast ("Timer finished while off: <name>") and does not ring. A timer still running resumes.
   - New now: the update engine treats a running recipe timer like an alarm. Pre-flight refuses to install (or go to a previous version) while a timer is running, with the message "Finish or cancel your timers first".
2. **URL imports saved directly: accepted.** Recipe-site data is usually clean. After a URL import, open the new recipe straight away with a short toast "Imported. Tap Edit to fix anything." No separate pre-save screen.

## D-010 — Answers to OPEN_QUESTIONS.md R1–R3 (2026-11-12)

1. **No web UI lock.** It's a home device with no emails or passwords on it. D-007 item 4 (pairing) is withdrawn: remove it from D-007 and from ARCHITECTURE.md. No login, no on-device "Allow" prompt.
2. **"Delete recipes" on Remove: approved as proposed.** Separate button (not part of Remove), confirm dialog that names the recipe count ("Delete 12 recipes from the SD card? This can't be undone."), available on device and in the web UI.
3. **SD wiring guesses: approved.** Keep them as defaults and keep the item in HARDWARE_TODO.md until checked on the board.
4. All other R1–R3 defaults in OPEN_QUESTIONS.md are accepted as chosen.

## D-009 — Work mode until the board arrives (2026-11-12)

Paul has no hardware yet and limited time with this model. Maximise progress with minimal stops. This replaces "propose a plan and wait" in D-008.

1. **Plan, then build without waiting.** Write the D-008 step plan into `docs/KICKOFF.md`, then start immediately. Merge each step yourself once CI is green, post a 2–3 line summary, and start the next step.
2. **Build everything that can be tested without the board**, in this order:
   1. Modules screen + module manifest/requirements (simulator + host tests).
   2. `recipe.json` format 1: validator, quantity scaling, fraction formatting, step-time detection, index rebuild (host tests).
   3. Storage layer behind an interface, with a host/simulator fake that uses a local folder as the "SD card".
   4. Recipes panel: library, card mode, book mode, both orientations (simulator).
   5. Recipe timers + glance-strip chip + "Cooking now" chip (simulator).
   6. Web UI import: PDF upload, pdf.js conversion, edit screen, inbox conversion (browser test harness, sample PDFs in `tests/fixtures/`).
   7. Recipe URL import: JSON-LD extraction (host tests on saved sample pages; the device-side fetch is coded but marked untested).
3. **Questions:** if a sensible default fits ARCHITECTURE.md and DECISIONS.md, pick it, log it in `docs/OPEN_QUESTIONS.md` (question, choice made, why), and keep going. Paul reviews that file in batches.
   **Only stop and ask** for: partition table, settings versioning rule, health gate / rollback / update catalog, licence, anything that deletes user data, or anything that can't be undone.
4. **Hardware-only code** (real SD card driver, double-tap on the LIS2DW12, screen-on while cooking, battery behaviour): write it behind the same interfaces, never claim it works, and list each item in `docs/HARDWARE_TODO.md` with how to test it in Phase 1.
5. **Handoff:** after every merge, update a short "Next steps" section at the top of `docs/KICKOFF.md` so a new session (possibly a different model) can pick up exactly where this one stopped.

## D-008 — First installable module: Recipe reader (2026-11-12)

New feature, planned after Phase 0. Add a "Modules and Recipe reader" section to ARCHITECTURE.md from this entry, then propose a step plan (small steps, as before) and wait for Paul's go-ahead before building.

### Modules (v1 "installable")
- v1 modules ship inside the firmware but start uninstalled. Real downloadable code modules stay a v2 item (WASM).
- New on-device **Modules** screen (also in the web UI): name, icon, short description, requirements, Install / Remove.
- Each module has a manifest with requirements, e.g. `requires: { sd_card: true, min_free_mb: 200 }`. Install checks them and explains what is missing ("Insert a microSD card").
- Install = enabled flag in settings (additive key, no schema bump) + the module's panel is added to the page order. Remove = hidden from pages; its data on the SD card is kept unless the user also chooses "Delete recipes".
- If the SD card is removed later, the panel stays in the menu and shows "Insert the SD card with your recipes" instead of crashing.

### Recipe reader: two modes
1. **Card mode** (parsed recipe in our own layout): title, servings, ingredient list on one side, one big step at a time on the other. Works in landscape (side by side) and portrait (ingredients above, step below), using size tokens.
2. **Book mode** (original pages): PDF pages shown as images, swipe or double-tap for next page, pinch or double-tap-and-hold to zoom.
- Every recipe has book mode if it came from a PDF. Card mode exists when parsing found ingredients + steps (or the user filled them in).

### Getting recipes onto the device (no PDF parsing on the device)
- **Conversion happens in the browser** during import, in the web UI, using pdf.js (Apache-2.0, GPL-3.0 compatible, served from the device's web UI assets or the assets partition):
  - Renders each page to JPEG sized for the screen (long side 1280 px) → book mode.
  - Extracts text and runs a parser (look for headings like Ingredients / Method / Directions / Steps / Instructions) → draft card-mode recipe.
  - Shows an edit screen so the user can fix title, servings, ingredients and steps before saving.
- **Import sources:**
  - Upload a PDF from phone/computer in the web UI.
  - "Convert new PDFs on SD card": the device lists unconverted PDFs in `/recipes/inbox/`, the browser fetches each, converts it, and uploads the result. The on-device Recipes panel shows "3 new PDFs — open the web UI to convert" with a QR code.
  - Paste a recipe website URL: the **device** fetches the page and reads the schema.org `Recipe` JSON-LD block (most recipe sites have one). No PDF involved; card mode only, plus the main photo if present.
- Scanned / photo-only PDFs: book mode only (no OCR in v1).

### Storage on the SD card
```
/recipes/
  inbox/                    PDFs copied here from a computer, waiting for conversion
  <recipe-id>/
    recipe.json             format 1: title, servings, ingredients[{qty, unit, item, note}], steps[{text, timer_s?}], source, tags, created
    pages/001.jpg ...       book-mode pages (only if from a PDF)
    photo.jpg               optional hero image
    original.pdf            kept so a recipe can be re-converted later
```
- `recipe.json` carries a `format` number; unknown fields ignored, like the settings rule.
- A small index file (`/recipes/index.json`) lists id, title, tags, has_card, has_pages, for a fast library screen. Rebuildable from the folders if missing or corrupt.

### Kitchen features (v1 of the module)
- Library screen: grid of recipes with photo or first page thumbnail, search by title, filter by tag, recently cooked first.
- Tap a time in a step ("bake for 60 minutes") → start a named timer; timers show as a glance-strip chip and ring through the alarm sound path. Several timers at once.
- Servings +/- scales ingredient quantities (only parsed ones with a number; show fractions nicely, e.g. 1 1/2 cup).
- Tap an ingredient to tick it off.
- Double-tap the case (LIS2DW12 tap) = next step / next page. Messy-hands friendly. No tilt gestures.
- Screen stays on while a recipe is open (overrides idle dim), with a 30-minute safety timeout; returns to normal after leaving the panel.
- "Cooking now" chip in the glance strip while a recipe is open on another page.

### Testing
- Host tests: recipe.json parser/validator, quantity scaling and fraction formatting, step time detection ("bake 25–30 minutes" → 25 min), JSON-LD extraction from saved sample pages, index rebuild.
- Browser test: pdf.js conversion of 3 sample PDFs (text recipe, multi-recipe page, scanned image) in the web UI test harness.
- Simulator: both modes in 1280x720 and 720x1280.
- Hardware (Phase 1+): SD card read speed for page images, timer ringing on battery.

### Open for later (not v1)
- OCR for scanned recipes, voice "next step", shopping list export, sharing recipes between devices.

## D-007 — Step 6 web UI: import, labels, Wi-Fi setup (2026-11-07)

1. **Settings import = replace all.** Approved, with guards:
   - Validate the whole file first. Newer schema version or invalid → refuse with a clear message, change nothing.
   - Confirm before applying: "Replace all settings? Your current settings are backed up."
   - Keep the backup downloadable as "Undo import".
   - Export leaves out secrets by default (Wi-Fi password, printer access code, tokens), with a tick-box to include them. Import keeps current secrets if the file has none.
2. **Settings labels.** Raw keys are fine for alpha. Add optional `label` and `help` fields to the settings schema now (code only, no stored-settings change). Fill them before the first beta.
3. **Wi-Fi setup** is its own hardware step, agreed. The setup access point must use WPA2 with its password in the on-screen QR code. Never an open network.
4. *(Withdrawn by D-010: no web UI lock, no pairing.)*
