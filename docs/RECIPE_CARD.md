# Recipe card redesign (requested by Paul, 2026-11-23) — to do

Status: **not started.** Written down so it is not lost; it goes before the end-of-project firmware polish (see
`docs/KICKOFF.md`, "Still to do, in order"). Design changes belong in the design doc (CLAUDE.md rule 10): this file is
Paul's request in his words, the questions below need his answers before the card is built.

## What the card is today
The tile in the Recipes library (`RecipesPanel::render_grid`, `firmware/panels/recipes/src/panel.cpp`): 2 columns in
portrait, 4 in landscape. Top to bottom: a thumbnail band of **page 1** of the recipe (`thumb.jpg`, made at import, cropped
to fill), the title (`text_m`), and a small muted line ("Steps + N pages  #tags").

## What Paul wants
1. **Keep the card size exactly the same.**
2. **Pull images out of the recipe** to use on the card. If several images are found, a **selector in the web UI** to pick
   which one the card uses (not just page 1).
3. **Drop the small text** under the title (the "Steps + pages #tags" line).
4. **A description text box in the web UI** (new, optional, per recipe).
5. **Title: a little bigger, bolder, centred in the card.**
6. **What fills the card, by what the recipe has:**
   | Has photo | Has description | Card shows |
   | --- | --- | --- |
   | yes | no | title, then the **photo** underneath (the default instead of the small text) |
   | yes | yes | title, then the **photo** (the default; see question 1) |
   | no | yes | title, then the **description** text |
   | no | no | the **title alone, centred in the whole card, as big as will fit** |

## Paul's answers (2026-11-23), now decisions
1. **Photo wins** when a recipe has both a photo and a description (no per-recipe switch).
2. **Description: short, "less is more", not a whole sentence, and in a bigger font than today's small text.** Decided by
   Claude within that: at most about 60 characters and 2 lines on the card (the web UI box shows a counter and stops there;
   longer text is kept in the recipe but cut on the card). Paul can change the number.
3. **Page-count badge:** a small **red circle attached to the card's top-right corner, like a notification badge, showing the
   number of pages in the recipe** (his sketch has a "3"). Not a speech bubble. Shown only when the recipe has page images.
   It overlaps the corner (half on the card, half off) so the grid needs a little padding so it is never clipped at the
   screen edge. The tags and the old "Steps" text are gone from the card.
4. **Photo: cropped to fill** the area under the title, as in his drawing: a rectangle inside the card with a small margin,
   **no extra border or frame** (the card keeps its own rounded outline). The thin line in the sketch only marks where the
   photo goes.
5. **Title: wrap over several lines and shrink to fit**; with no photo and no description it fills the whole card, centred,
   as large as it can be.

## Layout (from Paul's sketch)
```
   ___________________      (3)  <- red page-count badge on the corner
  |      TITLE        |
  |  ___________________  |
  | |                 | |
  | |      photo      | |
  | |_________________| |
  |___________________|
```
Title big, bold, centred at the top; the photo (or the short description) fills the rest of the card; same card size as now.

## How it can be built (notes, not decisions)
- **Images:** in the browser while importing a PDF, pdf.js can list the embedded images of each page (operator list) and
  render them to JPEG; for a website import the page's own photo (`photo.jpg`, from the JSON-LD/og:image) is already
  fetched. All candidates are kept as small JPEGs; the web UI shows them as thumbnails to pick from; the chosen one is
  saved as the card image (sized for the card, so the device decodes a small file).
- **Description:** new optional string in `recipe.json` (`description`), editable on the web edit screen; additive, so no
  format bump (check `parse_recipe` ignores/keeps it), and copied (shortened) into `/recipes/index.json` so the library does
  not open every recipe to draw its tiles.
- **Device:** the tile layout above in LVGL; the auto-sized title needs the available font sizes (pick the biggest that
  fits, wrapping). **Depends on the JPEG decoder** (ESP32-P4 hardware JPEG, `HARDWARE_TODO.md` "Page images on the device"):
  the card photo shows only once images work on the device; the description and title-only cards do not need it.
- Tests: host tests for the card choice (the table above) and the description limit; simulator screenshots in both
  orientations; the web import page gets a browser test for the image selector.
