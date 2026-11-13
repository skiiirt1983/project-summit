# Coupon results (Paul fills this in)

**Status: Print 1 (material slices) is done and applied. From Print 2, SEAT-body and CLIP-body are back (2026-11-14) and applied. The body now gets a push-on port-end cap, and the cap coupons have their own file, `coupons-cap.3mf` (CAP-corner, BODY-corner; BODY-corner reissued 2026-11-15). `coupons-fit-body.3mf` holds SEAT-body and END-bed, both unchanged since 2026-11-14. PETG fit coupons and the measurements are still to come.**

Print the coupons, try them, tick or write an answer in each box. Each answer names the
`cad/params.py` value it changes, so the full model can be updated in one pass.
Bring the **"Measure these"** numbers back at the same time.

Files are in `hardware/enclosure/coupons/` (picture: `coupons/coupons.png`).

![coupons](coupons/coupons.png)

## What to print

The 3MF files carry shapes and names only. Set the walls and infill per object in Bambu Studio
(right-click the object → per-object settings). Everything is already lying in its print orientation.

| File | Object | Material | Walls | Infill | Notes |
|---|---|---|---|---|---|
| `coupons-material.3mf` (done) | **T1** | TPU for AMS | 2 | 10 % gyroid | softest: **chosen** |
| | **T2** | TPU for AMS | 3 | 20 % gyroid | middle |
| | **T3** | TPU for AMS | 5 | 35 % gyroid | firmest |
| `coupons-fit-petg.3mf` | **GLASS-ring** | PETG (cream if you have it) | 4 | 15 % | prints face down |
| | **SEAT-bezel**, **CLIP-bezel** | PETG | 4 | 15 % | print face down |
| | **SEAT-frame**, **CLIP-frame** | PETG | 4 | 15 % | |
| | **SEAT-plate**, **SEAT-cover** | PETG | 4 | 15 % | |
| | **MIC-plate**, **PORT-block** | PETG | 4 | 15 % | |
| `coupons-fit-body.3mf` (**new 2026-11-14**) | **SEAT-body** | TPU for AMS | 2 | 10 % gyroid (T1) | supports off. Lip now has a 45° underside; plate edges are 1 mm 45° chamfers |
| | **END-bed** | TPU for AMS | 2 | 10 % gyroid (T1) | the real body's first 12 mm off the plate. Supports off, elephant-foot compensation 0.15, 0.08 mm layers over the rounds |
| `coupons-cap.3mf` | **CAP-corner** | TPU for AMS | 2 | 10 % gyroid (T1) | one corner of the port-end cap, end-face down, two pegs up. Unchanged: reuse the printed one |
| | **BODY-corner** (**new 2026-11-15**) | TPU for AMS | 2 | 10 % gyroid (T1) | the 20 mm of body under it, seam face up: one round hole, one short slot. Supports off |
| (kept) | **CLIP-body** | — | — | — | printed clean on 2026-11-14; no reprint. `CLIP-body.stl` is unchanged |

SEAT-frame and CLIP-frame changed on 2026-11-14 (their top outer edge is now chamfered to tuck under the sloped
lip). If you printed them before that date, the old ones still fit the old CLIP-body but not the new SEAT-body.

The material slices are the real triangle outline, 15 mm thick, name sunk into the top.
All layer heights 0.2 mm for the coupons, except GLASS-ring and the bezel pieces at 0.12 mm.

## 1. Material slices: DONE (Print 1, 2026-11-13)

| Question | Paul's answer | Applied |
|---|---|---|
| Body material | **TPU for AMS.** Not stone PLA, and no other TPUs (no external spools) | `FRAME_FIT` stays 0.00; stone PLA slice P1 and the PLA fallback removed |
| Feel on the outside | **T1** (2 walls, 10 % gyroid) | `SOFT_ZONE = "T1"` |
| Stiff enough at the screen wall | **T1 was stiff enough** | `FIRM_ZONE = None`; firm modifier only if the SEAT coupon shows the pocket flexing |
| Stays put when tapped | **yes** | no change (`PAD_D`, `LEG_T`, `BACK_ANGLE`) |
| Layer lines on the outside | **OK** | print orientation stays: standing on the portrait end |
| Back leg 13 mm | **right** | `LEG_T = 13.0` confirmed |
| Extra | **No sharp edges anywhere**; bed edges 45° chamfer blended into a round; elephant-foot compensation on; seams on an inside edge; finer layers on the curved edges | `ROUND_ALL = True` (approved and applied 2026-11-13), `BED_CHAMFER`, `BODY_PRINT` |

## 1b. SEAT-body and CLIP-body in TPU: DONE (2026-11-14)

| Paul's finding | Applied |
|---|---|
| The flat lip over the pocket in SEAT-body sagged into strings | Lip underside is now 45°: a 0.6 mm straight edge at the screen opening (`LIP_LAND`), then a 45° slope out to the frame. Frame top edge chamfered to match (`FRAME_TOP_CHAMFER`). Same on the real body |
| No flat ceilings in TPU parts | Real body, printed on end: the pocket's top end now has 45° sides (`ROOF_H = 6.5`). **One flat strip remains there, 17.8 mm wide** (see decisions.md). Portrait pad recesses, which open onto the plate, now end in a cone |
| Rounded bottom edges sag at the plate, first layer flares | Coupons: every plate edge is a 1 mm 45° chamfer (`PLATE_CHAMFER`), no rounds on the plate. Real body: the bed end already meets the plate at 45° (chamfer blended into the round), unchanged |
| CLIP-body printed clean | No change; file kept as issued |

## 1c. END-bed (a slice of the real body, printed standing up like the real thing)

END-top was withdrawn on 2026-11-14 (Bambu Studio's floating-cantilever warning was real). That end of the body
is now a separate cap; see 1d.

| Question | Answer | Changes |
|---|---|---|
| First layer and the 45° edge at the plate | ☐ clean ☐ flares ☐ sags | flares → elephant-foot compensation up (`BODY_PRINT`); sags → tell Claude: bed end goes to a plain 1 mm chamfer, no round |
| Do the rounds above the chamfer feel smooth? | ☐ yes ☐ stepped | stepped → finer layers on the rounds (`BODY_PRINT`) |
| Pad recesses (cone tops) clean? Pads fit? | ☐ yes ☐ loose ☐ tight | `PAD_FIT` |

## 1d. CAP-corner + BODY-corner (the push-on cap: fit and seam)

**Test 1, 2026-11-15 (peg hole = peg, `PEG_FIT = 0.00`):**

| Paul's finding | Applied |
|---|---|
| Too tight: needed force, would not close, very hard to pull apart | `PEG_FIT = 0.20` |
| Faces don't close: gap about 0.5 mm one side, 1 to 2 mm the other. Each peg pushed in on its own goes all the way | Not a depth problem, so `PEG_L` is unchanged. The two pegs were fighting each other. All holes but one are now a short slot pointing at that one (`PEG_SLOT = 0.4`), and every hole has a 0.5 mm lead-in (`PEG_LEAD`) |
| Seam groove and flush faces: can't judge until it closes | Recheck in test 2 |
| CAP-corner plate edge clean; pegs round and upright | No change. **CAP-corner is unchanged: reuse the printed one** |

**Test 2: reprint BODY-corner only** (`coupons/coupons-cap.3mf`, or `BODY-corner.stl`). Push the printed CAP-corner
into it until the faces meet. Pull it off and push it on ten times.

| Question | Answer | Changes |
|---|---|---|
| Push-fit | ☐ falls off ☐ loose ☐ right: snug, comes off by hand ☐ too tight ☐ won't go in | loose → `PEG_FIT` −0.10; too tight → +0.10 |
| Do the faces close up all the way round? | ☐ yes ☐ gap of ___ mm, even ☐ gap, uneven | even gap → `PEG_L` −0.5; uneven → `PEG_SLOT` +0.3 |
| Does the cap wobble or shift sideways once on? | ☐ no ☐ yes | yes → `PEG_SLOT` −0.2 |
| Seam: does the little V-groove look intended? | ☐ yes ☐ too faint ☐ too heavy | `GROOVE` (now 0.6) |
| Do the outside faces line up across the seam? | ☐ flush ☐ step of ___ mm | step → more or thicker pegs (tell Claude) |

## 2. GLASS-ring (the front 6 mm of the bezel, all the way round)

Lay the screen module in it, glass down. Never press on the glass.

| Question | Answer | Changes |
|---|---|---|
| Glass drops in? | ☐ falls in, rattles ☐ just right ☐ tight ☐ won't go | rattles → `GLASS_CLEAR` −0.10; tight → +0.10; won't go → +0.20 |
| Lip covers only the black border, not the picture? | ☐ yes ☐ covers picture on: ____ side | `LIP_OVERLAP` |
| Black border looks even enough behind the cream? | ☐ yes ☐ no | no → bezel width question again (`BEZEL_WALL`) |
| Gap between lip and glass when the board rests on the bench | ☐ none (touching) ☐ small ☐ large | `LIP_GAP`, and the "module depth" measurement below |

## 3. SEAT (one corner: body + frame + bezel + plate + cover)

Push SEAT-frame into SEAT-body. Then SEAT-bezel into the frame. SEAT-plate sits on the step inside
the bezel, SEAT-cover snaps in behind it.

| Question | Answer | Changes |
|---|---|---|
| Frame into body (with the cap, the real frame slides in from the open port end, so only the side-to-side fit matters here) | ☐ loose ☐ right ☐ too hard ☐ won't go under the lip | loose → `FRAME_FIT` −0.10; too hard → +0.10; won't go → `FRAME_TOP` or split the body |
| Does the T1 body wall bulge or flex where the frame sits? | ☐ no ☐ a little ☐ yes | yes → `FIRM_ZONE = "T2"` (firm modifier around the pocket only) |
| Bezel into frame | ☐ loose ☐ right ☐ tight | loose → `FRAME_CLEAR` −0.05; tight → +0.10 |
| Seam between cream and body: even? width? | ☐ good ☐ too wide ☐ too tight / rubbing | `SEAM` (now 0.30) |
| Bezel face flush with the body face? | ☐ flush ☐ proud by ___ ☐ sunk by ___ | `FRAME_FLOOR` / `POCKET_D` (tell Claude the number) |
| Plate on its step | ☐ loose ☐ right ☐ tight | `PLATE_CLEAR` (now 0.20) |
| Cover snap | ☐ falls out ☐ right ☐ too hard to open | falls out → `SNAP_BUMP` +0.10; too hard → −0.10; sloppy → `COVER_CLEAR` −0.05 |

## 4. CLIP (a straight piece with one spring clip)

Push CLIP-frame into CLIP-body, then slide CLIP-bezel in until it clicks. Pull it out ten times.

| Question | Answer | Changes |
|---|---|---|
| Click and hold | ☐ no click ☐ weak ☐ right ☐ too stiff | none/weak → `CLIP_BUMP` +0.15; too stiff → `CLIP_BUMP` −0.15 or `CLIP_LEN` +3 |
| Finger survived ten pulls? | ☐ yes ☐ whitened ☐ snapped | whitened/snapped → `CLIP_LEN` +4 and `CLIP_BUMP` −0.10. Second failure → drop clips, use magnets (brief: one iteration only) |
| Finger printed free (slots open)? | ☐ yes ☐ fused | fused → `CLIP_SLOT` +0.2 |

## 5. MIC-plate (three holes: 1.8, 2.0, 2.2)

| Question | Answer | Changes |
|---|---|---|
| Which is the smallest hole that printed clean and round? | ☐ 1.8 ☐ 2.0 ☐ 2.2 | `MIC_HOLE_D` (keep 2.0 if clean: that is the LyricFlash hole) |

## 6. PORT-block (two plug tunnels: "+0" as modelled, "+1" a millimetre bigger)

Hold it against the board's USB end and push your usual cables through to the ports.

| Question | Answer | Changes |
|---|---|---|
| Thickest cable you use: which tunnel does the plug pass and seat fully? | ☐ +0 ☐ +1 ☐ neither (plug is ___ × ___) | `USB_WINDOW` |
| Do both tunnels line up with both ports at once? | ☐ yes ☐ off by ___ | `USB_Y` (would mean the drawing is off; tell Claude) |

---

## Measure these (most important first)

Calipers in mm, one decimal. Don't squeeze the battery.

| # | What | Where to put the calipers | Updates |
|---|---|---|---|
| 1 | **Battery thickness** | Flat on the bench, jaws across the thickest point, including the taped protection board. Just touching, no pressure | `BATT_T` (sets the front-wall thickness) |
| 2 | **Battery length and width** | Outer edges of the pouch including the folded seam and the protection board end | `BATT_L`, `BATT_W` (bay is 106 × 70.4 now) |
| 3 | **Module depth** | Screen face down on a soft cloth. Depth rod from the bench (= glass front) to the top of the 40-pin header | `MODULE_DEPTH` (STEP says 12.8, drawing says 11.6) |
| 4 | **Glass front to standoff top** | Same set-up, depth rod to the top of one of the four brass standoffs | checks `GLASS_TO_BOARD` + `STANDOFF_H` (should be 11.6). This sets the lip gap over the glass |
| 5 | **Battery wires** | Which edge they leave from (short end or long side), and length from pouch to plug | battery orientation, `WIRE_NOTCH` |
| 6 | **Accelerometer board** | Length × width; height including the tallest part and any pins; hole diameter; hole centre spacing | `ACC_L`, `ACC_W`, `ACC_T` |
| 7 | **Glass outline** | Across the glass, long side and short side | checks `GLASS_L`, `GLASS_W` (126.9 × 70.7) |
| 8 | **USB-C plug overmould** of the cable you'll leave plugged in | Width and thickness of the plastic grip behind the metal tip | `USB_WINDOW` (now 12.5 × 7.0) |
| 9 | **Speaker**, if you want it fitted | Diameter and total depth | not modelled yet; needs a home (see decisions) |

Read off the board's printed labels (no calipers):

| # | What | Updates |
|---|---|---|
| 10 | Order of POWER / BOOT / RESET, starting from the USB end | button access (all three are paperclip holes now) |
| 11 | Which USB-C port is "UART" (upper or lower when the buttons are on top) | cable guidance only |
| 12 | Which 2-pin socket is BAT and which is SPK | `WIRE_NOTCH` position |
| 13 | Do the mics have a hole on **top** of their metal can (facing away from the board)? | confirms the mic path out of the back |
