# Enclosure decisions

| Date | Decision | Why | Rejected |
|---|---|---|---|
| 2026-11-09 | Battery lives in the screen module | Short wires, nothing crosses a joint | Battery in base |
| 2026-11-11 | Shape: the Peak, a one-piece hollow rounded triangle, front at 60° | Paul's pick (`ref/peak-final-60.png`) | Pebble, pedestal, wedge + base, "3 tablets" look |
| 2026-11-11 | Opening medium, back leg + bottom ~9 mm, outer corners ~16 mm | Paul's pick (q2, q3) | Crisp corners, ring-like corners |
| 2026-11-11 | Board dimensions come from Waveshare's STEP + drawing (see `dimensions.md`) | Official files exist; calipers only for conflicts and gaps | Measuring everything by hand |
| 2026-11-11 | CAD: build123d 0.13, renders via three.js in headless Chromium | Works in the cloud session; no GPU needed | CadQuery, OpenSCAD |
| 2026-11-11 | Portrait stands on the end away from the USB-C ports (option A). Ports face the right end seen from the front | Cables work in both orientations; buttons + card slot land on the top edge in landscape | B: stand on the port end with a cable channel |
| 2026-11-11 | Glass lip (1.0 thick, 1.35 overlap, 0.25 gap) and Ø2.0 straight mic holes copied 1:1 from LyricFlash | Proven on the S3 case; brief says copy, don't redesign | Redesigning |

## Proposed, waiting for Paul
- **Back leg and bottom thickness:** CAD uses 13 mm, read off `ref/peak-final-60.png`. The brief's sketch value was ~9 mm.

## Read off the target image (2026-11-11), not yet confirmed
Back leg 60°, inner corner radius 12 mm, soft end edges 7 mm outside / 4 mm into the opening, body 155.5 mm wide.
- **Cream bezel width (§8.5):** A 2.95 mm (as LyricFlash) / B 4.5 mm / C 6 mm (as in the target picture). Recommended: B. Picture: `out/q-bezel.png` (`cad/q_bezel.py`).
