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
| `coupons/reprint-tpu.3mf` | BODY-corner | **to print** (REMAINING-TESTS A1) |
| `coupons/reprint-petg.3mf` | CLIP-frame, CLIP-bezel, SNAP-bezel, SNAP-plate, SNAP-cover | **to print** (A2, A3) |
| `coupons/coupons-material.3mf` | T1, T2, T3 | printed, finished |
| `coupons/coupons-fit-body.3mf` | SEAT-body, END-bed | printed, unchanged |
| `coupons/*.stl` | one per piece | CAP-corner, CLIP-body, SEAT-frame, SEAT-bezel, GLASS-ring, MIC-plate, PORT-block are as first issued |
