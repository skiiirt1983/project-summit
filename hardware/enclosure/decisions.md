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

## Decided from Print 1 (material slices), 2026-11-13

| Decision | Why | Rejected |
|---|---|---|
| Body is TPU for AMS only | Paul's pick from the slices; no external spools | Stone PLA; other TPUs |
| Whole body at T1 (2 walls, 10 % gyroid) | Felt right, and stiff enough at the screen wall | T2, T3 |
| No firm zone for now; add one around the screen module only if the SEAT coupon needs it | T1 was stiff enough | Firm modifier by default |
| Back leg and bottom 13 mm | Confirmed on the slice | 9 mm |
| Print standing on the portrait end | Layer lines on the outside were OK; it stays put when tapped | Split body |
| No sharp edges: near full round on every outside edge, inside and out. Only the screen opening stays crisp. **Waiting for Paul's OK on the renders (`out/q-round.png`) before the full body changes** | Paul, after handling the slices | Current 7 / 4 mm rounds |
| Bed end: 45° chamfer blended into the round; elephant-foot compensation on; seam on an inside corner of the opening; 0.08 mm layers over the rounds | No overhang at the bed, rounds don't feel stepped | Plain round at the bed |

## Fixed 2026-11-13
- **Mic exit holes were blocked.** The mics sit 29.9 mm off the centre line, which is behind the solid corners of
  the triangle, so the holes in frame and body did not reach the hollow. The exit holes (cover, frame, body) are now
  11 mm off the centre line, where the hollow is. The holes in the carrier plate stay over the mics; sound crosses
  the free strip beside the battery. Checked in CAD: both exits are open. Coupons are not affected.

## Defaults picked by Claude on 2026-11-12 — default, Paul to confirm

Paul asked for the full model to be built ahead without waiting. Each of these is a default, Paul to confirm.

| § | Default | Why | Alternatives |
|---|---|---|---|
| 8.5 | Cream bezel 4.5 mm wide (option B), corner radius 8.25 | Between LyricFlash's 2.95 and the target picture's ~6| A 2.95, C 6 |
| — | Back leg and bottom 13 mm | Read off `ref/peak-final-60.png` | 9 mm (brief's sketch value) |
| 8.1 | (superseded by Print 1, above) TPU for AMS body around a rigid PETG inner frame; optional firm modifier shape is `coupons/modifier-firm-zone.stl` | Brief's preferred option | Stone PLA body |
| 8.2 | Screen module is a self-contained snap-fit unit (cream bezel with walls + carrier plate + back cover). It pushes into the inner frame from the front and is held by four printed spring clips. To remove it, push through the 22 mm finger hole inside the triangle | No magnets or screws to buy; nothing visible on the front | 4 magnet pairs (fallback if the clip coupon fails twice) |
| 8.2 | Inner frame is held in the TPU body by the body's own 1.5 mm lip, which also hides the frame edge | Keeps one thin seam around the cream bezel | Glue; frame flange |
| — | Board screws to a 2 mm carrier plate through its four M2.5 standoffs (4× M2.5×5 countersunk). The plate seats on a step in the bezel walls and is the divider between board and battery | LyricFlash pattern; nothing presses on the glass | Posts from the back cover |
| — | Buttons: three 1.8 mm paperclip holes in the module's top wall, reached after pulling the module out. No actuators | Brief: cheapest option; power-off is rare | Holes through the body (20 mm long, not practical) |
| — | microSD: open-to-swap. Slot window in the module wall, reached after pulling the module out | Slot faces the thick top of the front wall | Slot through the body |
| — | Mics: sound leaves through the back. Ø4 pass-through in plate, frame and body skin; the one Ø2.0 × 1.6 LyricFlash hole is in the back cover. Opens into the hollow, never covered in either orientation | Mics face the back and sit behind the battery | Side ports (20+ mm of body in the way) |
| — | Battery sits against the end away from the ports; a 20.6 mm strip at the port end stays free for mics, accelerometer and wires | Keeps the mic path and the Wi-Fi antenna clear | — |
| — | Accelerometer: on the carrier plate in that free strip, long side across the module. Seat not detailed until the board is measured | Rigid, square to the screen | On the back cover |
| 8.7 | Speaker: not fitted, no room reserved | No space left in the front wall with a 13 mm battery | Mount in the hollow, firing down |
| 8.8 | Wall mount: none | Not needed for the Peak | Keyhole in the back leg |
| 8.9 | Branding: none | Brief: no logos on the front | Small wordmark on the back leg |
| 8.6 | Colours: grey stone-look body, cream bezel, teal pads | As in the target picture | — |
| 8.4 | Print: body in one piece, standing on its portrait end, supports only inside the pocket. Bezel face down. | Layer lines run around the triangle, no supports on visible faces | Split body (needed if PLA) |
| — | Pads: 4× Ø12 under the bottom, 3× Ø8 on the portrait end, 2.5 thick in 1.5 recesses | The portrait end has only a narrow flat ring on the legs | Printed-in TPU feet |
| — | CAD layout: `params.py`, `parts.py` (module + frame), `peak.py` (body + assembly), `test_coupons.py`, `render_all.py` | One file per part was more files than the model needs | Brief's `bezel.py` / `back_shell.py` / `base.py` |

## Known risks in this pass
- Front wall is 34.7 mm thick because the battery is a 13 mm placeholder. Each mm off the battery comes straight off the wall.
- The lip-to-glass gap depends on two STEP heights (7.6 + 4.0). Measurement 4 in `COUPON-RESULTS.md` checks it.
- Mic path is about 20 mm from mic to open air, through the free strip beside the battery and out 19 mm sideways of the mic. Needs a listening test on the real board before the design is frozen.
- The frame goes in under a 1.9 mm TPU lip. The SEAT coupon tests whether 68D TPU flexes enough.
- Nothing has been printed. Fits are as drawn, not as tested.
