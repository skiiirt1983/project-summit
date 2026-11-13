# Open questions and defaults chosen (for Paul to review in batches)

Each entry: the question, the choice made, why. Newest first. Say if any should change.

## R6 — Web import

- **Upload limits.** Choice: page / photo images up to 2 MB each, PDFs up to 32 MB, 200 pages per recipe; an upload is refused if the card would have under 8 MB left.
- **Inbox PDFs after conversion.** Choice: the PDF is moved into the recipe folder as `original.pdf` (not deleted), so the inbox empties and the recipe can be re-converted later.
- **No delete in the API or storage layer.** Deleting recipes (and "Delete recipes" on module removal) waits for your go-ahead, per D-009.
- **pdf.js** 4.10.38 (Apache-2.0), about 500 KB gzipped, will be embedded in the firmware image next to the page (the assets partition has no file system yet).

## R5 — Timers and chips

- **Ringing.** Choice: a finished timer rings until stopped, and stops by itself after 1 minute. D-008 says "ring through the alarm sound path": the service sets `timers/ringing`; the audio service that plays the sound is hardware work.
- **Limits.** Choice: 8 timers at once, 1 second to 48 hours.
- **Chip priority at narrow widths.** Choice: the timer chip never drops (like time and battery); "Cooking now" drops together with weather (before next event).
- **Where timers are managed.** Choice: a bar at the top of the Recipes page (Cancel / Stop). Timers started from a recipe are named "<recipe>: Step N of M".
- **Timers do not survive a restart.** Choice: kept in memory only for now. Say if they should be saved.

## R4 — Recipes panel

- **Zoom in book mode.** D-008: "pinch or double-tap-and-hold to zoom". Choice: a Zoom / Fit button and a double-tap on the page toggle 2x; drag to pan. Pinch needs multi-touch, which the simulator can't test: listed in HARDWARE_TODO.md. Double-tap-and-hold not built.
- **Page fit.** Choice: full width in landscape (the top of the page is readable at once; scroll for the rest), whole page in portrait.
- **Sample recipes.** `tests/fixtures/sd` holds generated sample pages (drawn by `tools/make_recipe_fixtures.py`), not real cookbook scans, to avoid copyright questions.

- **Swipes inside a recipe.** D-008 wants swipe = next page/step, but a horizontal swipe also changes pages in the shell. Choice: while a recipe is open the panel takes horizontal swipes (`ui/swipe_lock`); you leave with the Back button or the time chip (Home). In the library, swipes change pages as usual.
- **Panels and storage (hard rule 2).** Choice: the panel gets a `RecipeStore` (over the `Storage` interface in the framework) when it is created; it never touches the SD driver. `PanelContext` gained `publish` and `set_state` so panels can raise events and own state.
- **Thumbnails in the library.** Choice: none yet (text cards). Decoding full 1280 px pages for a grid would be slow on the device; plan: the web converter also writes a small `thumb.jpg` (R6) and the grid shows it.
- **Timer request format.** Choice: event `timers/start` = `"<seconds>|<name>"`, name = "<recipe>: Step N of M". The timer service arrives in R5.
- **Search keyboard.** Choice: LVGL's on-screen keyboard appears when the search box is tapped.
- **"Recently cooked"** is recorded only when the clock is synced (`time/unix`).
- **Fixed while building R4:** removing a module while its page was showing left the panel un-hidden (crash on the next page change). The shell now hides every shown panel.

## R3 — Storage

- **One storage implementation for device and host.** Choice: `PosixStorage` over a mounted folder (`/sdcard` on the device, a local folder in the simulator and tests). Why: the same code is tested on the host; only mounting is hardware-specific.
- **The device never formats a card.** Choice: `format_if_mount_failed = false`; an unreadable card just shows as "no card". Why: formatting deletes user data.
- **SD wiring assumptions** (slot 0 default pins, 4-bit, LDO channel 4) are guesses from the usual ESP32-P4 layout, not from the board. Listed in HARDWARE_TODO.md.
- **Saving a recipe rebuilds the whole index.** Choice: simple and always consistent; fine for hundreds of recipes. Revisit if SD scans are slow on hardware.

## R2 — recipe.json format

- **`source` shape.** D-008 lists "source" without a shape. Choice: an object `{type: pdf|url|manual, name?, url?}`. Why: room for the file name and the page address without a format bump.
- **"Recently cooked first" needs a timestamp.** Choice: optional `cooked` (unix time) in `recipe.json`, copied into the index. Why: survives an index rebuild.
- **Metric quantities.** Choice: grams/millilitres etc. show decimals (`375 ml`, `1.25 kg`), not fractions; everything else snaps to halves, thirds, quarters, eighths within 0.02. Why: "187 1/2 g" reads wrong.
- **Time ranges.** Choice: lower bound ("25–30 minutes" → 25 min), as in D-008's example.
- **Invalid recipes.** Choice: any invalid field refuses the whole `recipe.json` (it is skipped in the library and reported), rather than loading it partly.

## R1 — Modules

- **D-007 item 4 (web UI pairing lock) is still in DECISIONS.md, but Paul said in chat (2026-11-08) it was pasted by mistake and to remove it.** Choice: not built; ARCHITECTURE.md says "no login". Please delete D-007.4 or confirm you want it after all.
- **"Delete recipes" on Remove.** D-008 specifies it, D-009 says stop and ask for anything that deletes user data. Choice: Remove only hides the module and keeps all data; the delete option is not built yet. It needs your go-ahead (proposed: separate button, confirm dialog naming the recipe count, device + web UI).
- **How to reach the Modules screen on the device.** Choice: a "Modules" button (next to "Updates") in the quick-settings sheet, and `ui/open_screen` = `settings/modules`. Why: there is no settings menu yet; quick settings is the only system surface.
- **Where an installed module's page goes.** Choice: appended after the existing pages unless `ui/page_order` lists it. Why: matches the existing page-order rule.
- **SD card state topics.** Choice: `storage/sd_present` (bool) and `storage/sd_free_mb` (int), published by the storage service. Install re-checks requirements at the moment of install only; a module stays installed if the card is removed later (panel shows "Insert the SD card", per D-008).
- **Module enabled flag key.** Choice: `modules/<id>/installed` (bool, default false), additive, no schema bump.
