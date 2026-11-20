# CLAUDE.md: SLOPEFORM Home enclosure ("the Peak")

> **KEEP THIS FILE CURRENT. Whenever anything about the enclosure changes (a decision, a parameter, a test result, a
> measurement, the status, an open item, a file or a command), update this file in the same commit. Update the root
> `CLAUDE.md` project status too if the overall status changes. A stale CLAUDE.md misleads the next session.**

Handoff for a new Claude Code session working on the case. Read this first, then
`ENCLOSURE.md` (the design brief), `decisions.md` (dated log, open items) and `REMAINING-TESTS.md`
(what Paul still has to print or measure). Status below is as of 2026-11-20 (branch
`claude/vigilant-keller-12zcdw`). The firmware side of Project Summit is in the root `CLAUDE.md` and
`docs/ARCHITECTURE.md`; this file only covers `hardware/enclosure/`.

## 1. Where things stand

- **The design is decided and modelled in full.** A hollow, rounded triangle ("the Peak"), front face at 60° from the
  desk, 3D printed. It is a parametric build123d model (`cad/`), validated: every part is a valid solid, no part overlaps
  another, both mic exits are open.
- **Nothing has been printed as a full case yet.** Everything so far was tested on small coupons (pieces of the real
  parts) that Paul prints and fills in a form for. Passed: material, body ends, cap push-fit, clips, seat, back cover,
  port block, carrier plate, glass ring, first button test on the real board.
- **Open right now:** A4 pin retest (new TPU strip, printing now, any one of its three tunnels; one pin), C5 (POWER pin on the
  board again to confirm the grip band does not hold the button down, and a paperclip through the BOOT/RESET holes), battery and accelerometer measurements (D1, D2, D5, D6; not arrived), and the
  **speaker** decision (needs the battery length). See section 7.

## 2. How to work with Paul (read this; it saved a lot of rework)

- Paul designs by discussion and iteration. One focused change at a time, render it, show it, wait. Keep replies short.
- **Show, don't describe.** Questions come with pictures labelled A/B/C; he answers with a letter. Plain words, no CAD
  jargon, **no parameter names in anything Paul reads** (keep those in a "For Claude" table at the end of a form).
- Never invent dimensions. Unknown number: a clearly marked `PLACEHOLDER` with extra room, listed in `dimensions.md`
  (`drawing`, `measured` or `PLACEHOLDER`).
- When Paul says "work ahead", pick the sensible default, log it in `decisions.md` as *default, Paul to confirm*, keep going.
- **Test pieces before full parts.** Every fit question goes through a coupon. Never claim a fit works that has not been
  printed; say what was tested and what was not.
- Forms for Paul are **Word files** he fills in (`REMAINING-TESTS.docx`, built by `cad/remaining_tests.py` from one
  source, plus the Markdown twin). Send files with the file tool; do not just commit them. He answers with ticks and
  short notes; read them with `python-docx` and check for double ticks or answers that contradict his message.
- A finished test leaves the checklist and goes into `COUPON-RESULTS.md` (the record). One checklist only.
- **Do not change files Paul already printed.** Coupon STLs in `coupons/` are what he has in his hands. If a piece
  changes, reissue it under the same name only when he needs to reprint, and say so. Rebuilding everything with
  `--all` changes them: only after a design change, and say it.
- Commit and push to `claude/vigilant-keller-12zcdw` (never another branch, no PR unless asked). Pushes sometimes need
  a retry loop. Commit trailer: the Co-Authored-By and Claude-Session lines from the session's attribution reminder.
- Be honest when something was your mistake (the SEAT coupon had no snap ridge; the BTN-frame coupon had a loose
  fragment). Paul appreciates it and checks.
- Do not draw logos or artwork in CAD; ask for an SVG. Do not create planning documents he did not ask for.

## 3. The design (decided)

| Area | Decision |
|---|---|
| Shape | One hollow rounded triangle, front 60°, back 60°, legs 13 mm, outer corners R16, **no sharp edges anywhere**: near full round on every outside edge, inside and out. Only the screen opening is crisp |
| Orientation | Landscape on the bottom; **portrait stands on the end away from the USB-C ports**. Ports face the right end seen from the front |
| Material | **TPU for AMS only** (Paul owns it; no other spools, no stone PLA). Whole body at "T1": 2 walls, 10 % gyroid. No firm zone. Inner frame, bezel, plate, cover and pins are PETG |
| Parts | main body (TPU) + **port-end cap** (TPU, 19.55 mm, six pegs, no glue) + inner **frame** (PETG, slides in from the open end) + **screen module** (cream bezel with walls, carrier plate, back cover; all PETG) + one **POWER pin** + TPU **pads** |
| Cap | Carries both USB-C openings. A 0.6 mm V-groove at the seam, mirrored at the other end so both look intended |
| Module | Snap-fit: four clips hold it in the frame; plate screws to the board's four M2.5 standoffs; back cover snaps in (two ridges per long side) and opens with a bent paperclip through a pull slot in its floor (next to the battery) |
| Bezel | Copied from LyricFlash: glass sits 1.25 mm below the face behind a 1.0 mm lip (overlap 1.35, gap 0.25). Visible cream 4.75 mm |
| **Buttons** | **Only POWER goes to the outside** (buttons from the USB-C end: RESET, BOOT, POWER): a printed pusher pin runs up the slope through the module wall, the frame and the TPU to a dimple on the top ridge. **BOOT and RESET are 1.8 mm paperclip holes in the PETG module**, reached after pulling the module out; Paul plans RESET as a programmed short press of POWER (a firmware item, see 7). **No battery switch is fitted** (the newest board needs a hard POWER press to turn back on; a battery switch can leave it unable to) |
| Mics | Face the back; sound leaves through the cover into the hollow (exit holes moved to 11 mm off the centre line, where the hollow is). Hole Ø2.0 as LyricFlash |
| Printing | Body stands on its portrait end, **no supports, no flat ceilings in TPU** (45° undersides on every lip; teardrop tops on horizontal holes); bed edges are a 45° chamfer blended into the round; elephant-foot compensation; seam on an inside corner; fine layers over rounds |
| Open decision | **Speaker** (see 7) |

Key tuned values (all in `cad/params.py`, every value that a coupon decides is tagged `COUPON`): peg hole +0.10 with
0.2 mm slots; clip bump 0.75; snap bump 0.50; plate clearance 0.175 (tight is fine, screws hold it); seam 0.30; glass
pocket 0.55 per side (0.30 did not fit); USB opening 13.5 x 8.0 R2.5; frame clearance 0.25. Body 120.1 deep x 136.5 wide
x 108.3 tall; front wall 34.9 mm, driven by a **placeholder 13 mm battery**.

## 4. The real board (Waveshare ESP32-P4-WIFI6-Touch-LCD-5, newer revision)

Official STEP and drawing: github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-5 (`hardware/mechanical`, commit a334bad).
Glass 126.9 x 70.7 (measured 126.93 x 70.71); board 118.5 x 64.5; USB-C x2 on one short end (the **upper** port is
UART); three side-push buttons on one long edge, **measured 1.9 mm further from the USB-C end than the drawing says**,
centre 1.4 above the board, 0.75 proud; sockets X = BAT (J2), Y = SPK (J5), Z = RTC (H3); microSD slot on the buttons
edge; the 15-pin camera connector exists but is unused. Glass front to standoff top 11.8, to header top 11.6.
**The newest board revision differs from Waveshare's files; trust measurements over the drawing.**

## 5. Waiting on hardware

Battery (D1 thickness, D2 length and width, D5 wire exit and length), accelerometer breakout (D6). The battery sets
the front wall thickness, whether a speaker fits, and where the battery wires cross the carrier plate (`WIRE_NOTCH` is
still a placeholder). Placeholders also live in `ACC_*`, `BATT_*`.

## 6. Known risks

- The POWER pin's grip band (Ø3.2 in a Ø3.0 TPU tunnel) must hold the pin in place without holding the button down.
- The POWER button position along the board is the midpoint of two possible readings (+1.26 mm); one answer in C5 settles it.
- Mic path is ~20 mm through the free strip and a cavity: needs a listening test on the real board before the design is frozen.
- The pull slot position beside the battery is untested (the coupon has a test slot).
- Nothing has been printed as a whole case: first full-body print is still ahead.

## 7. Open decisions

1. **Speaker** (25.9 x 26.1 x 4.52 mm, wires from the middle of a side). Recommended: in the free strip at the port end,
   firing out of the back into the hollow. The strip is 20.6 mm wide and needs about 27, so the battery must be at most
   about 98 mm long. Decide when D2 is known.
2. **Firmware (for Paul's design doc, not built):** RESET as a programmed short press of POWER.
3. Defaults awaiting Paul's OK (all logged as *default, Paul to confirm*): the paperclip pull slot, the pin grip band, mic
   exit positions, battery strip layout, no wall mount, no branding, colours (grey body, cream bezel, teal pads).

## 8. Things that went wrong (do not repeat)

- Flat TPU ceilings sag into strings; Bambu Studio flags floating cantilevers. Check every new TPU feature for overhangs
  (the cap exists because the screen opening's top edge was a 77 mm bridge when printed on end).
- Rounds at the print plate sag and flare: chamfer 45° instead.
- A coupon cut line can slice a feature and leave a loose fragment. Check every coupon file for one solid.
- A coupon must contain the feature it tests (the first snap test had no ridge).
- Arachne varies wall widths on thin features; ask Paul for line width 0.42 and "Classic" walls on every fit part.
- Do not date entries ahead of the real date; list results by round.
- Paul measures from places you did not specify: say exactly where the calipers go.

## 9. Toolchain and files

```bash
pip install build123d pillow python-docx trimesh shapely   # build123d 0.13, OCP bundled
cd hardware/enclosure/cad/render && npm i && cd ..          # three.js renderer; needs Playwright + Chromium
python peak.py            # full model: STEP / STL / 3MF -> ../out/ (git-ignored)
python render_all.py      # review renders (landscape, portrait, exploded, cut, buttons)
python test_coupons.py    # reissue pieces (see its docstring; --all rebuilds everything)
python test_pictures.py   # pictures for the checklist
python remaining_tests.py # REMAINING-TESTS.md + .docx
```

| File | What |
|---|---|
| `params.py` | **every** dimension and clearance, with a source tag (`drawing`/`measured`/`PLACEHOLDER`/`COUPON`) |
| `parts.py` | screen module, frame, button pins, tunnels, body cutter |
| `peak.py` | body, cap split, pegs, pads, assembly, exports |
| `test_coupons.py`, `test_pictures.py`, `remaining_tests.py`, `render_all.py` | coupons, checklist pictures and documents, renders |
| `../coupons/` | print files (STL, 3MF) as issued to Paul; `../renders/` latest renders; `../ref/` reference images and the LyricFlash 3MF |
| `../decisions.md`, `../dimensions.md`, `../COUPON-RESULTS.md` | the log, the provenance table, the record of finished tests |

Headless rendering uses three.js in Chromium (no GPU in the cloud container). `ref/lyricflash-enclosure/` holds the
LyricFlash print plates 3MF the bezel and mic holes were measured from.

## 10. Start a new session with this (and keep this file current)

> Read `hardware/enclosure/CLAUDE.md`, `ENCLOSURE.md`, `decisions.md` and `REMAINING-TESTS.md`, check out
> `claude/vigilant-keller-12zcdw`, and summarize in 3 bullets where things stand. I will give you my test results next.
> Apply them one test at a time, update `params.py`, `decisions.md` and `COUPON-RESULTS.md`, regenerate
> `REMAINING-TESTS.docx`, show me renders, and send me any new files directly. Update `hardware/enclosure/CLAUDE.md` in the
> same commit as every change.
