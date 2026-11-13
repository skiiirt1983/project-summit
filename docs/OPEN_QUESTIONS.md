# Open questions and defaults chosen (for Paul to review in batches)

Each entry: the question, the choice made, why. Newest first. Say if any should change.

**D-010 (2026-11-13): Paul accepted all R1-R3 defaults below and the SD wiring guesses (still to be checked on the board, see HARDWARE_TODO.md).**

## R7 — Import from a website

- **No edit screen for URL imports.** Choice: the recipe is saved directly, since structured data is usually clean. Editing saved recipes in the web UI would cover fixes (not built yet).
- **Which addresses the device may fetch.** Choice: public `https://` names only; IP addresses, `localhost`, single-label, `.local` / `.lan` / `.internal` / `.home` names, ports and credentials are refused, and every redirect is re-checked. Why: with no login on the web UI, the device must not become a way to probe the home network.
- **Photo.** Choice: only the first image, only over https, only if it is a JPEG up to 2 MB; otherwise the recipe is saved without a photo. Not resized on the device.
- **Sites without JSON-LD** (microdata only, or behind scripts/paywalls) are not supported: "This page has no structured recipe data to import".
- **Sample pages** in `tests/fixtures/web` are written for this project, not copied from real sites.

## R6 — Web import

- **Several recipes in one PDF.** Choice: the edit screen offers a choice and imports one at a time; every imported recipe keeps all pages of that PDF. Splitting pages per recipe is not attempted.
- **Units are normalised** ("cups" → `cup`, "tablespoons" → `tbsp`, "grams" → `g`); ranges take the lower number ("2-3 cloves" → 2). The user can edit before saving.
- **`thumb.jpg`** (320 px) is written on import for lists. The web UI shows it; the device library still shows text cards (needs a JPEG decoder on the device first).
- **Parser language.** English headings only (Ingredients / Method / Directions / Instructions / Steps / Preparation). Other languages fall back to pages only, with manual entry.
- **Editing an existing recipe** from the web UI is not built (the API supports it: `save` with an `id`).

- **Upload limits.** Choice: page / photo images up to 2 MB each, PDFs up to 32 MB, 200 pages per recipe; an upload is refused if the card would have under 8 MB left.
- **Inbox PDFs after conversion.** Choice: the PDF is moved into the recipe folder as `original.pdf` (not deleted), so the inbox empties and the recipe can be re-converted later.
- **Delete in the storage layer.** Only `remove_tree`, added for "Delete recipes" (D-010); it refuses the card root and top-level folders. There is still no way to delete a single recipe.
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

- ~~D-007 item 4 (web UI pairing lock).~~ **Resolved by D-010:** no web UI lock, no login, no "Allow" prompt. Removed from D-007.
- ~~"Delete recipes" on Remove.~~ **Resolved by D-010 and built:** separate button, confirm names the count, on the device and in the web UI. Defaults chosen while building it:
  - The button shows whenever the card holds recipes, installed or not (so you can still delete after Remove). It is hidden when there are none or no card.
  - It deletes every folder in `/recipes/` except `inbox` (including folders with an unreadable `recipe.json`, which are counted too). PDFs still waiting in the inbox are kept: they were never recipes.
  - The web request must repeat the count from the question; if recipes were added or removed in between, it is refused and you are asked again.
  - Cancel is the first button in the device dialog; Delete is red.
- **How to reach the Modules screen on the device.** Choice: a "Modules" button (next to "Updates") in the quick-settings sheet, and `ui/open_screen` = `settings/modules`. Why: there is no settings menu yet; quick settings is the only system surface.
- **Where an installed module's page goes.** Choice: appended after the existing pages unless `ui/page_order` lists it. Why: matches the existing page-order rule.
- **SD card state topics.** Choice: `storage/sd_present` (bool) and `storage/sd_free_mb` (int), published by the storage service. Install re-checks requirements at the moment of install only; a module stays installed if the card is removed later (panel shows "Insert the SD card", per D-008).
- **Module enabled flag key.** Choice: `modules/<id>/installed` (bool, default false), additive, no schema bump.
