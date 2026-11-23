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

## Questions for Paul (answers needed before building)
1. Photo **and** description both exist: photo wins ("photo underneath by default")? Or a per-recipe choice in the web UI
   ("show: photo / description")? Proposed: photo wins, with that choice available.
2. Description length: a limit (for example 140 characters) and how many lines may show on the card?
3. The "has pages" (book mode) badge and the tags disappear from the card. Tags stay for the library filter chips; is any
   small indicator for "has page images" still wanted?
4. Photo crop: fill the area and crop (as the thumbnail does now), or fit the whole image?
5. Title-only card: wrap long titles over several lines, and shrink to fit?

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
