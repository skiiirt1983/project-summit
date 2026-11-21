# Coupon results: the record

This file is the **record of finished tests** and what each answer changed.
**What is still to do is in [`REMAINING-TESTS.md`](REMAINING-TESTS.md)** (also as `REMAINING-TESTS.docx` to fill in).

All results below were reported by Paul; the dates in this project's notes before this point were not reliable, so
entries are listed in order (round 1, 2, ...) and not by date.

## Round 1: material slices T1, T2, T3 (and stone PLA P1)

| Question | Paul's answer | Applied |
|---|---|---|
| Body material | TPU for AMS. Not stone PLA, no other TPUs (no external spools) | PLA slice and fallback removed |
| Feel on the outside | T1 (2 walls, 10 % gyroid) | `SOFT_ZONE = "T1"` |
| Stiff enough at the screen wall | T1 was stiff enough | `FIRM_ZONE = None` |
| Stays put when tapped | yes | no change |
| Layer lines on the outside | OK | prints standing on the portrait end |
| Back leg 13 mm | right | `LEG_T = 13.0` |
| Extra | No sharp edges anywhere; no rounds on the plate; elephant-foot compensation; seam inside; fine layers on rounds | `ROUND_ALL`, `BED_CHAMFER`, `BODY_PRINT` |

## Round 2: SEAT-body and CLIP-body in TPU

| Finding | Applied |
|---|---|
| Flat lip over the pocket sagged into strings | Lip underside is 45° (`LIP_LAND`, `FRAME_TOP_CHAMFER`) |
| No flat ceilings in TPU; rounds at the plate sag and flare | Coupon plate edges are 1 mm chamfers (`PLATE_CHAMFER`); body ends meet the plate at 45° |
| CLIP-body printed clean | No change |

END-top was then withdrawn (Bambu Studio's floating-cantilever warning was real), which led to the port-end cap.

## Round 3: cap push-fit, first try (`PEG_FIT` 0.00)

| Finding | Applied |
|---|---|
| Too tight; would not close; each peg seats fully on its own | `PEG_FIT = 0.20`, slotted holes `PEG_SLOT = 0.4`, lead-in `PEG_LEAD = 0.5`. `PEG_L` unchanged |
| CAP-corner plate edge clean, pegs round and upright | No change |

## Round 4: results document "Results for Claude 2"

| Test | Question | Paul's answer | Applied |
|---|---|---|---|
| Cap, second try | Push-fit | loose | `PEG_FIT` 0.20 → **0.10** |
| | Faces close all the way round | yes | `PEG_L` confirmed |
| | Wobbles or shifts sideways | yes | `PEG_SLOT` 0.4 → **0.2** |
| | Groove looks intended | yes | `GROOVE = 0.6` confirmed |
| | Faces line up | flush | no change |
| END-bed | Plate edge | clean | no change |
| | Rounds smooth | yes | no change |
| | Pad recesses clean, pads fit | yes | `PAD_FIT` confirmed |
| SEAT | Frame into body | right | no change |
| | Body wall bulges or flexes | a little | no change; asked again in REMAINING-TESTS B1 |
| | Bezel into frame | **loose and right both ticked** | no change (see cross-check) |
| | Line between cream and body | good | `SEAM = 0.30` confirmed |
| | Bezel face flush | flush | no change |
| | Plate on its step | loose | `PLATE_CLEAR` 0.20 → **0.15** |
| | Cover snap | falls out | **Coupon fault, not a fit result:** the corner piece had no snap ridge. Ridges moved near the ends (`SNAP_X`), new full-width SNAP pieces |
| CLIP | Click and hold | **no click and weak both ticked**; Paul: "weak, close to no click" | `CLIP_BUMP` 0.60 → **0.75** |
| | Finger after ten pulls | survived | no change |
| | Finger printed free | yes | no change |
| MIC-plate | Smallest clean hole | 1.8 | `MIC_HOLE_D` stays 2.0 (LyricFlash) |
| PORT-block | Which tunnel fits the cable | +1 | `USB_WINDOW` (12.5, 7.0) → **(13.5, 8.0)** |

## Round 5: REMAINING-TESTS groups A and B (everything that does not need the board)

| Test | Question | Paul's answer | Applied |
|---|---|---|---|
| Cap, third try | Push-fit | right: snug, comes off by hand | `PEG_FIT = 0.10` confirmed |
| | Faces close all the way round | yes | no change |
| | Wobbles or shifts sideways | no | `PEG_SLOT = 0.2` confirmed |
| | Groove looks intended; faces flush | yes; flush | `GROOVE = 0.6` confirmed |
| | Pad pocket with pointed top printed clean | yes | no change. **Cap test finished** |
| Clip, second try | Click and hold | right | `CLIP_BUMP = 0.75` confirmed |
| | Finger after ten pulls; printed free | fine; yes | no change. **Clip test finished** |
| Back cover snap | Plate on its step | tight (it was loose on the corner piece at 0.20, tight on the full-width piece at 0.15) | `PLATE_CLEAR` 0.15 -> **0.175**, halfway rather than the form's +0.05, which is the value that was loose |
| | Cover snap | holds but weak | `SNAP_BUMP` 0.40 -> **0.50** |
| | Cover back level with the bezel | level | no change |
| | Open with a fingernail | no, needs a tool | A flush cover has nothing to grip, so the notch is replaced by a 2.5 x 10 mm **pull slot** through the cover floor for a bent paperclip (`PULL_SLOT`). Real cover: in the free strip beside the battery. Default, Paul to confirm; the brief allows a spudger |
| Seat | Frame into body; bezel into frame | right; right | no change |
| | Body wall bulges or flexes | a little | no change (`FIRM_ZONE` stays off) |
| | Line between cream and body; bezel face level | good, even; level | `SEAM = 0.30` confirmed. **Seat test finished** (this also resolves the earlier double tick on "bezel into frame") |

Retest left: **SNAP-bezel, SNAP-plate, SNAP-cover** (all three change: the cover ridge height changes the bezel's groove too).
Waiting on the board: C1 to C4. Calipers: D1 to D8.

## Round 6: the PETG retest (SNAP pieces)

| Question | Paul's answer | Applied |
|---|---|---|
| Cover snap | right | `SNAP_BUMP = 0.50` confirmed |
| Cover back level with the bezel | level | no change |
| Opening the cover | "yes". The form he used still had the old wording ("with a fingernail at the notch"; the notch is now a paperclip slot), so read as: it opens | `PULL_SLOT` kept. Real slot position untested |
| Plate on its step | tight at 0.175, but Paul: "I thought it was ok before the change" | **`PLATE_CLEAR` stays 0.175.** I had loosened it to 0.20 and reverted: tight is fine because the screws to the board hold the plate. **Back cover finished, no reprint** |

## Round 7: the real board arrives (no battery or accelerometer yet)

| Test | Question | Paul's answer | Applied |
|---|---|---|---|
| Port block | Both tunnels line up with both ports | yes | `USB_Y` confirmed |
| | Plug goes fully home through the +1 tunnel | yes, both ports. "+0 is too tight" | `USB_WINDOW` 13.5 x 8.0 confirmed. **Port test finished** |
| Carrier plate | Screw holes over the standoffs; plate sits flat; header clears the cut-out | yes; yes; yes | `STANDOFFS`, `SCREW_D`, `PLATE_CLEAR = 0.175` confirmed. **Plate test finished** |
| Labels | Buttons 1, 2, 3 from the USB-C end | **1 = RESET, 2 = BOOT, 3 = POWER** | `POWER_PIN = 2`: the POWER pin gets a raised bar on its head |
| | Extra buttons or switches | "yes: camera" (the 15-pin camera connector in the drawing; not a button) | no change, camera is a later add-on |
| | UART USB-C port | **A, the upper one** (the one nearer the buttons) | note only |
| | Sockets X, Y, Z | **X = BAT (J2), Y = SPK (J5), Z = RTC (H3)** | wire notch placement waits for the battery |
| | Mic holes on top of the metal can | yes | confirms the mics listen out of the back |
| Glass ring | Does the glass drop in | **won't go.** Paul also sees white sticker-like adhesive on the inside of the glass edge, on the top, right (opposite the USB end) and bottom sides, and thinks the ring was printed with a different line width | Reprint with the standard line width, plus a larger ring B (+0.25 per side). Retest A5. **Paul later: the adhesive is on the back of the glass, facing the board, so it is not the cause** |
| Measurements | D3 header depth | 11.6 | `MODULE_DEPTH = 11.6` (documentation only) |
| | D4 standoff depth | 11.8 | `GLASS_TO_BOARD` 7.6 -> **7.8** (standoff top = 11.8) |
| | D7 glass | 126.93 x 70.71 | matches the drawing; no change |
| | D9 buttons | from the USB-C end: 74.54, 86.50, 98.05; stick out 0.75; centre height 1.4; no extra buttons | `BUTTON_STICKOUT = 0.75`, `BUTTON_H = 1.4`. The distances were measured from the connector tip (told later), so `BUTTONS_X` = 73.22 / 85.18 / 96.73 from the board edge (the drawing was right to within 0.7 mm) |
| | D8 speaker | wanted: 25.9 x 26.1 x 4.52 mm, wires from the middle of the side | **open decision, see `decisions.md`** |

Still to do: A4 (button strip, the TPU part is printing), A5 (glass rings), C5 (buttons on the board), D1, D2, D5, D6 (battery, accelerometer).

## Round 8: button pins on the bench, glass ring retest, first button test on the board

| Test | Question | Paul's answer | Applied |
|---|---|---|---|
| Button pins (A4) | Pins slide through all layers; move freely | freely; yes | `TUNNEL_D`, `PIN_D` confirmed |
| | Head easy to find and press | **too small and too sunk in** | `PIN_HEAD_D` 6.0 -> **8.0**, `CBORE_D` 6.5 -> **8.6**, `PIN_PROUD` 0.6 -> **1.2** |
| | Tip the strip with the ridge down | **all pins fall out** | New grip band on each pin (`PIN_RIB_D` 3.2, `PIN_RIB_W` 1.5) that the TPU holds lightly |
| | Pull a pin out with a fingernail | "yes, easily, falls out" | same fix. Pulling must stay easy; retest A4 |
| | Dimples printed clean | yes | no change |
| Glass ring (A5) | Which ring fits | **B only** (0.55 mm per side); just right | `GLASS_CLEAR` 0.30 -> **0.55**. `BEZEL_WALL` 3.15 -> 3.40 and `BEZEL_R` 8.25 -> 8.50 so the cup walls and every tested clearance stay as they were (visible cream 4.5 -> 4.75). **Ring test finished** |
| | Lip covers only the black border; border even; gap lip to glass | yes; yes; small gap | `LIP_OVERLAP`, `LIP_GAP` confirmed |
| Buttons on the board (C5) | Other two holes over RESET and POWER | yes, both | button pitch confirmed |
| | Each pin clicks its button | all three | no change |
| | Pin returns when you let go | yes (Paul's note: "too loose?", consistent with the pins falling out) | grip band, see above; retest |
| | Switch the board back on through POWER | yes | **works** |
| | How hard BOOT and RESET need pressing | very easy, like POWER | no change: `PIN_OVERTRAVEL` stays 0.5 |
| | Measured the button distances from where | **the outer face of the USB-C connector** (told later) | `BUTTONS_X` = 73.22 / 85.18 / 96.73 from the board edge (measured minus the 1.32 connector overhang): the drawing was right to within 0.7 mm |

New pieces to print: `coupons/buttons-tpu.3mf` (BTN-body, bigger dimples) and `coupons/buttons-petg.3mf` (3 pins). BTN-frame and BTN-cup stay as printed.

## Round 9: button pins, second try (grip band), POWER on the board

| Test | Question | Paul's answer | Applied |
|---|---|---|---|
| Pins (A4) | Easy to push in and press | **too stiff, it sticks** | `PIN_RIB_D` 3.2 -> **2.9** as the full-model default; three variants (2.8 / 2.9 / 3.0) to try |
| | Head easy to find | still "too sunk in" (those pins were stuck pushed in) | `PIN_PROUD` 1.2 -> **1.6**; judged again with free pins |
| | Tip the strip with the ridge down | the pin **stays in** | grip is enough; just less of it |
| | Pull a pin out | yes, but hard | less grip |
| | Dimples clean | yes | no change |
| POWER on the board (C5) | Pin clicks POWER | yes | works |
| | Pin comes back when you let go | **no, it stays down** | the grip band holds the button down: less grip, retest |
| | Switch the board back on through the pin | **yes, a short press** | works |
| | Paperclip through the BOOT and RESET holes | both click | BOOT and RESET paperclip holes work |

Retest left: **three pins with different grip bands** (`coupons/buttons-petg.3mf`), then C5 once more with the chosen pin.

## Round 10: three pins with grip bands 2.8 / 2.9 / 3.0, POWER on the board

| Test | Question | Paul's answer | Applied |
|---|---|---|---|
| Pins (A4) | All three pins (bands 2.8, 2.9, 3.0) | **all fall out when tipped**; none chosen | the answer lies between 3.0 and 3.2: next round 3.05 / 3.10 / 3.15; default `PIN_RIB_D` 3.1 |
| | Head with a free pin | easy to find and press | `PIN_PROUD = 1.6`, `PIN_HEAD_D = 8.0` confirmed |
| POWER on the board (C5) | Pin clicks POWER; comes back when you let go; board back on with a short press; paperclip through BOOT and RESET | yes; **yes** (not held down); yes; both | works. Retest only because the final band will be larger than the ones tried |

## Cross-check: every test this form ever asked

| Test | Questions | Status |
|---|---|---|
| Material slices | 7 | all answered (round 1) |
| SEAT-body / CLIP-body print quality | 4 findings | all answered (round 2) |
| END-top | 8 | withdrawn, never printed; replaced by the cap |
| END-bed | 3 | all answered (round 4) |
| Cap push-fit | 5 (+1 new) | answered twice, as intended: rounds 3 and 4. Third try is REMAINING-TESTS A1 |
| SEAT | 7 | all 7 ticked in the results document, **but Paul's message says the SEAT test is not done**. Kept as answered here; repeated as REMAINING-TESTS B1 (5 questions) with a "same as before" box. The plate and cover questions moved to A3 |
| CLIP | 3 | all answered (round 4). Second try is REMAINING-TESTS A2 |
| MIC-plate | 1 | answered (round 4). Finished |
| PORT-block | 2 | 1 answered (round 4). **"Do both tunnels line up with both ports" was skipped** (needs the board): REMAINING-TESTS C2 |
| GLASS-ring | 4 | **none answered** (needs the board): REMAINING-TESTS C1 |
| Measurements 1 to 9 | 9 | **none answered**: REMAINING-TESTS D1 to D8. Number 8 (USB plug size) dropped: the port test answered it |
| Board labels 10 to 13 | 4 | **none answered**: REMAINING-TESTS C4 |

Double ticks: SEAT "bezel into frame" (loose + right) and CLIP "click" (no click + weak). The CLIP one is resolved by
Paul's summary. The SEAT one was left unchanged: tightening it changes the pocket size and would mean reprinting
SEAT-body, which fitted.

## Files

| File | Pieces | State |
|---|---|---|
| `SNAP-bezel.stl`, `SNAP-plate.stl` (0.175), `SNAP-cover.stl` | | printed, finished |
| `coupons/coupons-material.3mf` | T1, T2, T3 | printed, finished |
| `coupons/coupons-fit-body.3mf` | SEAT-body, END-bed | printed, finished |
| `BODY-corner.stl`, `CAP-corner.stl`, `CLIP-frame.stl`, `CLIP-bezel.stl`, `CLIP-body.stl`, `SEAT-frame.stl`, `SEAT-bezel.stl`, `MIC-plate.stl`, `PORT-block.stl` | | printed, finished (no reissue needed) |
| `GLASS-ring.stl` | | printed or to print when the board is here (C1) |
| `coupons/*.stl` | one per piece | CAP-corner, CLIP-body, SEAT-frame, SEAT-bezel, GLASS-ring, MIC-plate, PORT-block are as first issued |
