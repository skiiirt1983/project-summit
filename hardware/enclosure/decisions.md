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
| No sharp edges: near full round on every outside edge, inside and out. Only the screen opening stays crisp. **Approved by Paul 2026-11-13 and applied to the full body (`ROUND_ALL = True`)** | Paul, after handling the slices | Current 7 / 4 mm rounds |
| Bed end (the portrait end) only: 45° chamfer blended into the round; the port end is a plain full round (default, Paul to confirm; he has not asked for matching ends); elephant-foot compensation on; seam on an inside corner of the opening; 0.08 mm layers over the rounds | No overhang at the bed, rounds don't feel stepped | Plain round at the bed |

| Both USB-C openings stay, tidied: rounded corners (R2.5) and a 1 mm round on the outside mouth. PORT coupon updated to the same shape | Paul 2026-11-13; ports stay usable in both orientations | Removing both; keeping one |

## Decided from the TPU fit coupons (SEAT-body, CLIP-body), 2026-11-13

| Decision | Why | Rejected |
|---|---|---|
| No flat ceilings in TPU parts. The lip that hides the frame has a 45° underside (0.6 mm straight edge at the screen opening, then 45°); the frame's top edge is chamfered to tuck under it | The flat lip on SEAT-body sagged into strings | Flat lip |
| Edges that touch the print plate are 45° chamfers, never rounds. Coupons: 1 mm chamfer | Rounds sagged and the first layer flared | Rounds at the plate |
| Real body's bed end keeps the approved 45° chamfer blended into the round (**default, Paul to confirm**) | It already meets the plate at 45°, and a bare 1 mm chamfer would leave a hard edge on the end it stands on in portrait. END-bed coupon tests it | Plain 1 mm chamfer, no round |
| Pocket's top end (a ceiling when the body prints on end) gets 45° sides 6.5 mm high. **A 17.8 mm flat strip remains** (**default, Paul to confirm**) | There are only 9 mm of body beyond the pocket; a full 45° peak needs about 15 mm. The brief allows TPU bridges up to about 20 mm; END-top coupon tests it | Full peak (body ~10 mm wider at the port end); supports inside the pocket; splitting the body |
| Portrait pad recesses end in a cone | They open onto the plate; a flat top would be a ceiling | Flat-topped recess |
| CLIP-body unchanged | Printed clean | — |
| New coupons END-bed and END-top: slices of the real body in its real print orientation | Tests the actual plate edge, top rounds, lip and pocket top before a full-body print | Printing the whole body to find out |

## Port-end cap (Paul chose option A, 2026-11-13)

Why: printed on end, the port end of the screen opening is a ceiling (lip hanging in the air, a 60 to 77 mm bridge
on the opening's top edge). Bambu Studio flagged it on END-top. Rejected: printing the body on its bottom; supports
inside the pocket.

| Decision | Why |
|---|---|
| The last 19.55 mm at the port end is a separate TPU cap. The seam sits where the screen opening's corners start to curve, so the main body's opening and pocket are straight all the way to the top | Main body prints on end with no ceilings (checked on the mesh: 6 mm² left, the crowns of the two 4 mm mic holes) |
| No glue. Six TPU pegs on the cap (Ø5 × 8) push into holes in the body: two in each leg, one in each front corner | Paul: per the brief, and the cap must come off |
| The cap is the service hatch: both USB-C openings are in it. With it off, the pocket is open at the end, so the inner frame slides in and out there (no more forcing it under the lip) | Paul |
| **BOOT/RESET/POWER holes and the card slot are not in the cap: they don't line up.** The buttons sit 73 to 96 mm from the USB end of the board and the card slot 51 to 67 mm; the cap covers the first 18 mm. They stay on the module's top wall, reached by pushing the module out | Geometry (Waveshare drawing) |
| A 0.6 mm V-groove runs round the outside at the seam, with a matching groove mirrored at the other end | Paul: the two lines should look intended |
| The cap prints with the port end face on the plate, pegs up. So both ends of the body now have the same 45° chamfer blended into the round, and the USB mouths are chamfered, not rounded | No rounds on the plate; the two ends now match |
| With the cap: clip pockets run up to the open top, the finger hole and landscape pad recesses get a teardrop top, landscape pads move 7 mm in to clear the seam, the pocket-top roof is removed | No flat ceilings in TPU |
| **Approved by Paul 2026-11-13 and applied to the full model (`CAP = True`)** | Renders OK |
| Peg holes: 0.20 mm larger than the pegs; one round hole sets the position and every other hole is a 0.4 mm slot pointing at it; 0.5 mm lead-in on every hole. Peg length unchanged | Cap test 1: too tight, and the pair of pegs would not close although each seated fully alone, so the spacing was fighting. The slots are Claude's addition (default, Paul to confirm; `PEG_SLOT = 0` turns them off) |

## Decided from results round 4 ("Results for Claude 2")

| Decision | Why |
|---|---|
| Peg holes 0.10 mm larger than the pegs, slots 0.2 mm | Second cap try was loose and wobbled; half steps back, since 0.00 was too tight |
| Clip bump 0.75 mm | Click was weak, close to none; finger survived ten pulls |
| USB-C openings 13.5 x 8.0 mm | Paul's cable needs the +1 tunnel |
| Mic hole stays 2.0 mm | 1.8 printed clean, so the LyricFlash size is safe |
| Carrier plate 0.05 mm larger per side | Plate was loose on its step |
| Back-cover snap ridges moved from the middle of each long side to two near the ends; cover snap height unchanged | The SEAT corner coupon contained no snap ridge, so "falls out" was the coupon's fault (Claude's). New full-width SNAP pieces test it properly |
| Bezel-into-frame clearance unchanged | Loose and right were both ticked; tightening would change the pocket and SEAT-body, which fitted |
| One checklist of open tests: `REMAINING-TESTS.md` / `.docx`, built by `cad/remaining_tests.py`. `COUPON-RESULTS.md` is now only the record | Paul: the form had become confusing |

## Decided from results round 5 (REMAINING-TESTS groups A and B)

| Decision | Why |
|---|---|
| **Cap, clip and seat are finished.** Peg holes 0.10 + 0.2 slots, clip bump 0.75, seam 0.30, groove 0.6 all stay as they are | All answers right / flush / good |
| Carrier plate 0.175 mm per side (was 0.15) | 0.20 loose on a corner piece, 0.15 tight on the full-width piece |
| Back-cover snap ridge 0.50 mm (was 0.40) | Held but weak. The bezel's groove follows automatically |
| Back cover opens with a bent paperclip through a 2.5 x 10 mm slot in its floor, beside the battery. The old edge notch is gone (default, Paul to confirm) | The cover sits flush, so a fingernail cannot catch it, and an edge notch can only push the cover in, not pull it out. The slot works like the button holes. Only the SNAP coupon tests it (with a test slot); the real slot position is untested |

## Decided from results round 6 (PETG retest)

| Decision | Why |
|---|---|
| Back cover finished: snap ridge 0.50, level, opens with a paperclip through the pull slot | Snap right, level, opens |
| Carrier plate stays 0.175 mm per side. A change to 0.20 was made and reverted the same day | Paul: it was fine before the change. Tight is acceptable because the screws to the board hold the plate. No plate reprint |

## Button access (2026-11-16, Paul's note about the newest Waveshare board)

Paul: the latest Waveshare ESP32-S3 came with an updated PCB with unreleased changes, and it needs a hard press of the on-board POWER button to turn back on; the switches fitted earlier may not work as planned. So **all three buttons must be pressable from outside with the case assembled**, not only reachable after pulling the module out.

| Decision | Why |
|---|---|
| Every button (POWER, BOOT, RESET) gets a printed PETG **pusher pin**: from the button, through a 3.4 mm hole in the module's top wall, a 3.0 mm guide tunnel in the frame and the TPU, to a 6.5 mm dimple on the top ridge. Pins are identical; the three heads sit 11.7 mm apart (the buttons' pitch) | Replaces the earlier 1.8 mm paperclip holes inside the module, which needed the module pulled out. The pin tip is fixed at the button; the pin returns on the button's own spring, no printed spring |
| **No battery slide switch is fitted, and none should be added.** The POWER button is the only on/off | A switch in the battery line can leave the board unable to turn itself on, which is exactly the trouble with the new revision |
| The pins stand 0.6 mm proud of the ridge so a fingernail can pull them, and bottom out 0.5 mm past the button's touch point | Easy removal; cannot over-press a button |
| Pins block the module from sliding out while they are in: pull the three pins first, then push the module out through the finger hole (default, Paul to confirm) | Pin tips sit in the module's holes; a spring-loaded or shorter pin would need a more complex part |
| Top ridge exit: in landscape the pins point up the slope and are pressed from above; in portrait the ridge is the vertical back edge and they are pressed from the side | Buttons are on the board's long edge, so the only exits are the ridge or the desk |
| Teardrop-topped tunnels and dimples in the TPU body (peak pointing toward the port end, which is up when printing) | No ceilings in TPU |
| **Everything about the buttons is from Waveshare's drawing, which the new revision may not match.** First check on the board: tests C4, C5 and D9. Button positions are `BUTTONS_X`, `BUTTON_H`, `BTN_TIP_Y`; every tunnel, hole and pin follows them | Paul: the new boards have caused a redesign before |
| After C4 (which button is POWER): mark the POWER pin (default: a raised bar on its head) | Pins are identical until then |

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
| — | (superseded 2026-11-16: see "Button access") Buttons: three 1.8 mm paperclip holes in the module's top wall, reached after pulling the module out | Brief: cheapest option; power-off is rare | — |
| — | microSD: open-to-swap. Slot window in the module wall, reached after pulling the module out | Slot faces the thick top of the front wall | Slot through the body |
| — | Mics: sound leaves through the back. Ø4 pass-through in plate, frame and body skin; the one Ø2.0 × 1.6 LyricFlash hole is in the back cover. Opens into the hollow, never covered in either orientation | Mics face the back and sit behind the battery | Side ports (20+ mm of body in the way) |
| — | Battery sits against the end away from the ports; a 20.6 mm strip at the port end stays free for mics, accelerometer and wires | Keeps the mic path and the Wi-Fi antenna clear | — |
| — | Accelerometer: on the carrier plate in that free strip, long side across the module. Seat not detailed until the board is measured | Rigid, square to the screen | On the back cover |
| 8.7 | Speaker: not fitted, no room reserved | No space left in the front wall with a 13 mm battery | Mount in the hollow, firing down |
| 8.8 | Wall mount: none | Not needed for the Peak | Keyhole in the back leg |
| 8.9 | Branding: none | Brief: no logos on the front | Small wordmark on the back leg |
| 8.6 | Colours: grey stone-look body, cream bezel, teal pads | As in the target picture | — |
| 8.4 | (superseded: see "Port-end cap") Body printed in two parts: main body standing on its portrait end, cap on its port end, no supports. Bezel face down | Layer lines run around the triangle, no supports on visible faces | One piece |
| — | Pads: 4× Ø12 under the bottom, 3× Ø8 on the portrait end, 2.5 thick in 1.5 recesses | The portrait end has only a narrow flat ring on the legs | Printed-in TPU feet |
| — | CAD layout: `params.py`, `parts.py` (module + frame), `peak.py` (body + assembly), `test_coupons.py`, `render_all.py` | One file per part was more files than the model needs | Brief's `bezel.py` / `back_shell.py` / `base.py` |

## Known risks in this pass
- Front wall is 34.7 mm thick because the battery is a 13 mm placeholder. Each mm off the battery comes straight off the wall.
- The lip-to-glass gap depends on two STEP heights (7.6 + 4.0). Measurement 4 in `COUPON-RESULTS.md` checks it.
- Mic path is about 20 mm from mic to open air, through the free strip beside the battery and out 19 mm sideways of the mic. Needs a listening test on the real board before the design is frozen.
- The frame goes in from the open port end under a 1.9 mm TPU lip: the SEAT test passed ("right").
- Printed and tested so far (all coupons, no full parts): material slices, body ends, the cap push-fit, the clip, the seat, the back-cover snap and the port block. Not yet tested: anything on the real board (glass ring, port line-up, plate holes), the real pull-slot position, the listening test for the mics, and a full body print.
