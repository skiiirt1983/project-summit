# Open questions and defaults chosen (for Paul to review in batches)

Each entry: the question, the choice made, why. Newest first. Say if any should change.

**D-010 (2026-11-13): Paul accepted all R1-R3 defaults below and the SD wiring guesses (still to be checked on the board, see HARDWARE_TODO.md).**

## First flash feedback (2026-11-22)

From Paul's test on the board (touch, boot screen and shell work):

- **Choppy page swipes.** While dragging, only the current page moves and black shows behind it; the next page appears only after the finger lifts. A phone shows both pages together during the drag. Doing that means preparing the neighbouring panel (show, snapshot, hide) at drag start, which touches the one-panel-at-a-time rule and has side effects (a panel's show/hide sets state such as `ui/keep_awake`). **Needs Paul's decision** before it is built. Meanwhile frame rate was raised: draw buffers in internal RAM (were PSRAM), redraw timer 16 ms and touch read 8 ms (were 33 ms), and an on-screen FPS readout (`CONFIG_LV_USE_PERF_MONITOR`, alpha only) so the real numbers can be read.
- **"Rotation locked / unlocked" toasts at start-up.** They come from a long-press on the top bar, so something touched the bar with no finger there. Touch is now ignored while the boot screen shows, and the first 40 touches are written to the serial log (`touch down x= y=`).
- **Boot screen** background changed to solid very dark grey; it shows in portrait unless `sensor/orientation` reads landscape (no accelerometer service yet, so always portrait for now), and the shell starts landscape.
- **Sliders.** Brightness now sets the backlight (and the saved value applies at start-up). Volume does nothing until the audio driver exists (HARDWARE_TODO).
- **Flashing speed** is the flasher's serial baud rate, not the firmware: raise it (921600) in the flasher.

## Motion and controls (2026-11-16)

Paul asked for the front-end feel to be designed (buttons, phone-like transitions). The design is in ARCHITECTURE.md "Motion and controls". Choices made, say if any should change:

- **Timings and curves** (press 80 ms, page up to 320 ms, sheet 300 / 220 ms, screens 320 / 240 ms) are starting values chosen on the simulator; they should be tuned by hand on the panel.
- **Outgoing page as a picture.** Keeps the "one panel holds LVGL objects" rule, at the cost of about 1.7 MB for a third of a second. Alternative if it is slow on the board: let two panels be shown during a swipe (a change to the panel contract, so it needs a decision).
- **Updates and Modules come in from the right**, like a phone's settings pages, instead of appearing at once. Their close button is unchanged; swipe-right-to-go-back is not built.
- **Page dots** at the bottom of the page while swiping and for 1.2 s after. They overlay the page; panels do not reserve space for them.
- **No whole-screen fades or scaling**, to protect the frame rate. The existing rotation fade is unchanged.
- **Self-test waits** went from 300 to 400 ms after a page change, because the page move is now up to 320 ms.

## D-012 — Sleep

**Decided (Paul, 2026-11-14):**

- **Face-down on USB: DND, no deep sleep.** The screen turns off and sounds are muted (alarms and timers still ring, and the screen comes on for them), but the device never deep-sleeps, so Wi-Fi and data stay fresh. Applies on USB power whatever "stay on when charging" says, so that setting no longer affects face-down. On battery nothing changes: face-down for 30 s deep-sleeps. Built as sleep blocker `face_down_usb` plus state `power/dnd` and `power/screen_off`; the display and audio drivers that act on them are hardware work (HARDWARE_TODO.md).
- **URL import: web UI highlight, device toast, never a jump.** The web UI highlights the new recipe (as built). The device shows "New recipe: <title>" and opens it only when the toast is tapped. Built as `notify/toast_link` (toast with a screen to open) and `ui/open_request`; the device screen never changes by itself. Replaces the D-011 wording "open the new recipe straight away", see the entry below.

- **Ringing blocks sleep.** Not in D-012's list, added: the device does not sleep while a timer or alarm is ringing.
- **Blockers beat face-down.** Choice: an update, an open recipe, ringing, or "stay on when charging" keep the device awake even face-down.
- ~~**"Stay on when charging" also applies face-down.**~~ **Answered (2026-11-14):** face-down on USB gives screen off + mute, never deep sleep. See above.
- **Open recipe limit.** Choice: the screen-on rule holds for 30 minutes without a touch (the limit already in ARCHITECTURE.md for `ui/keep_awake`), then normal sleep rules apply.
- **Something due within the wake margin** (5 s): the device stays awake for it instead of sleeping.
- **Idle timeout 0 = never** sleep when idle (face-down still sleeps). Settings: `power/idle_sleep_min` (default 5), `power/stay_on_charging` (default on); both additive, no schema bump.
- **What a touch is.** Choice: any press on the screen (`input/activity` from the shell) and a double-tap on the case restart the idle time; turning the device back over does too.
- **Snapshot.** Up to 8 timers (names cut to 80 bytes) and the next alarm, under 1 KB. A ringing timer is not saved.
- **Alarms.** There is no alarm service yet; the rules already take `alarm/next_in_s` for the wake time.
- **Simulator.** In interactive runs a "sleep" wakes at once with a toast; the self-test fakes the sleep and the timer wake.
- **Power button wake** is not armed: its pin is not known yet (HARDWARE_TODO).
- **Periodic refresh wake** (optional in D-012): not built.

## Follow-ups after R7

- **Editing a saved recipe (web UI).** Choice: an "Edit" button per recipe in the Recipes tab opens the same form as import, filled from the device. It changes text only (title, servings, tags, ingredients, steps); pages, photo, source and created date are kept, and the recipe keeps its id (so its folder name does not change with the title). Why: no rename means nothing on the card moves.
- **Timers when editing.** Choice: a step whose text is unchanged keeps its timer; a changed or new step has none saved (the device still detects times in the text when cooking).
- **Quantities when editing.** Ingredients are shown as lines ("1 1/2 cup flour, sifted") and parsed again on save, so an unusual quantity can be rounded to the nearest shown fraction.
- **Library thumbnails on the device.** Choice: a tile shows `thumb.jpg` (the 320 px picture of page 1 made at PDF import) as a band across the top, cropped to fill. Recipes without one (typed, or imported from a website) show text only. Why not `photo.jpg` for website recipes: it is the full-size photo (up to 2 MB) and the device does not resize images, so a library of them would be slow to draw. If you want pictures for website recipes, the browser could make a thumbnail at import time.
- ~~Keep timers across a restart?~~ **Answered by D-011 / D-012:** memory only until the RTC is proven (HARDWARE_TODO); across deep sleep they are kept (D-012).
- **Update guard for timers (D-011).** Choice: a ringing timer counts as running too, and the guard also applies to firmware uploaded from the web UI (same pre-flight).
- **"Open the new recipe straight away" after a URL import (D-011).** Read as the web UI, since that is where Edit is: the new recipe's row is highlighted and scrolled into view, with the message ending "Tap Edit to fix anything." The device screen does not jump to the recipe (someone may be cooking). ~~Say if you meant the device.~~ **Answered (2026-11-14):** web UI highlight stays; the device gets only a "New recipe: <title>" toast that opens the recipe when tapped, never a jump. See D-012 above.
- **URL imports** are still saved directly; fix them afterwards with Edit. No separate pre-save edit screen.

## R7 — Import from a website

- **No edit screen for URL imports.** Choice: the recipe is saved directly, since structured data is usually clean. Fix anything afterwards with Edit in the Recipes tab.
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
