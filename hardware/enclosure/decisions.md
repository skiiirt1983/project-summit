# Enclosure decisions

| Date | Decision | Why | Rejected |
|---|---|---|---|
| 2026-11-09 | Battery lives in the screen module | Short wires, nothing crosses a joint | Battery in base |
| 2026-11-11 | Shape: the Peak, a one-piece hollow rounded triangle, front at 60° | Paul's pick (`ref/peak-final-60.png`) | Pebble, pedestal, wedge + base, "3 tablets" look |
| 2026-11-11 | Opening medium, back leg + bottom ~9 mm, outer corners ~16 mm | Paul's pick (q2, q3) | Crisp corners, ring-like corners |
| 2026-11-11 | Board dimensions come from Waveshare's STEP + drawing (see `dimensions.md`) | Official files exist; calipers only for conflicts and gaps | Measuring everything by hand |
| 2026-11-11 | CAD: build123d 0.13, renders via three.js in headless Chromium | Works in the cloud session; no GPU needed | CadQuery, OpenSCAD |

## Proposed, waiting for Paul
- **Portrait end (§8.3):** USB-C ports face the right end (seen from the front); portrait stands on the left end, ports on top. Puts buttons + card slot on the top edge in landscape. Picture: `out/q-portrait-end.png`.
- **Back leg and bottom thickness:** CAD uses 13 mm, read off `ref/peak-final-60.png`. The brief's sketch value was ~9 mm.

## Read off the target image (2026-11-11), not yet confirmed
Back leg 60°, inner corner radius 12 mm, soft end edges 7 mm outside / 4 mm into the opening, body 155.5 mm wide.
