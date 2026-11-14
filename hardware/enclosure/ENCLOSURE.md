# SLOPEFORM Home — Enclosure Design Brief

> For Claude Code. Put this file at `hardware/enclosure/ENCLOSURE.md` in the
> `project-summit` repo and link it from `CLAUDE.md`. It is the single source of truth
> for the case. When a decision changes, update this file first, then the CAD.

---

## 1. What we're building

A 3D-printed case for the Waveshare ESP32-P4-WIFI6 5" touch display (1280×720),
8000 mAh LiPo and LIS2DW12 accelerometer. It sits on a desk or hangs on a wall, in
landscape or portrait (the firmware rotates the UI using the accelerometer).

It's released openly on MakerWorld and GitHub (GPL-3.0), so it has to print well on a
stock Bambu printer without supports on cosmetic faces, and it has to look like a
product, not a project.

**Name check:** SLOPEFORM → the identity is *a slope*, *a ridgeline*, *alpine
restraint*. Brand aesthetic: clean, minimal, blocky-modern alpine.

---

### Headline feature: rotation
Screen rotation is a **core feature**, not a nice-to-have. The screen unit and base must look
deliberate and work fully in **landscape and portrait**: ports reachable, mic not
blocked, vents not covered, stable on the base, no "upside-down" detailing (e.g. a
wordmark that only reads one way). Every design review render includes both orientations.

---

## 2. What we're NOT building (see `ref/not-like-this.png`)

**Important:** the chosen design (the Peak, §3) is also a one-piece triangle that stands on
its own. **The triangle shape isn't the problem.** The reference image is a guide to the
*details* that made it look cheap and generic. Don't treat it as "avoid triangles".

What makes the reference look wrong, and what the Peak does instead:

| ❌ In the reference | ✅ In the Peak |
|---|---|
| Fat bezel framing the glass | Slim cream bezel, proportions matched to the LyricFlash enclosure |
| Screen sunk deep behind a raised lip | Screen depth matched to LyricFlash; module sits flush with the body |
| Solid, heavy wedge body | **Open** triangle: the hole through the middle keeps it light |
| Hard edges and generic chamfers | Medium-soft rounded corners inside and out, one continuous surface |
| Shiny plastic with visible layer lines | Stone-look or TPU body, **surface quality over print time** (§4) |
| Looks like a printer accessory | Reads as one calm object; nothing looks bolted on |

**Family resemblance:** SLOPEFORM Home should look like a sibling of the **LyricFlash
enclosure** (the ESP32-S3 1.75" case). Bezel proportions, how deep the screen sits, and
the edge/corner treatment are copied from that design and scaled to the 5" display, not
reinvented. Get the LyricFlash CAD/3MF from Paul (`ref/lyricflash-enclosure/`) and measure:
- visible bezel width around the glass
- screen depth: how far the glass sits below (or above) the bezel face
- bezel lip/overlap onto the glass edge
- corner and edge radii
Record each value in `dimensions.md` with source `lyricflash`. If a value doesn't
scale sensibly from 1.75" round to 5" rectangular, show Paul the options rather than guessing.

Positive references: `ref/peak-final-60.png` (the chosen design), Paul's concept images
(`ref/concept-*.jpg`, for the soft stone look), and the LyricFlash enclosure.

---

## 3. Design direction: the Peak

> **Update 2026-11-11: new starting point.** Paul's concept images (`ref/concept-pedestal.jpg`,
> `ref/concept-pebble.jpg`) set the style: a soft, stone-like pebble shape that reads as
> **one object**, not a tablet in a dock, a monitor or a digital photo frame. It has a
> speckled stone-look surface, a cream rounded bezel sunk into the stone, and no hard
> edges. Two shapes to explore: **pedestal** (pebble head on a pinched neck and a pebble
> foot) and **pebble** (one loaf shape on a small foot). The concept is far too big;
> scale it to the real 5" screen. The rest of this section is older thinking: keep the
> requirements (rotation, battery in the screen part, TPU feet) but follow the concept's look.
>
> **CHOSEN 2026-11-11: the hollow triangle ("the Peak").** Reference: `ref/triangle-one-piece.png`.
> This replaces the pebble, wedge and base + screen ideas below where they conflict.
> - **One continuous body:** seen from the side it's an open triangle (a mountain peak)
>   with soft rounded corners inside and out. It must read as ONE piece, not three panels
>   joined together (Paul rejected a version that looked like "3 tablets").
> - **Screen module:** a separate part that sits **flush** in the front wall. The front
>   wall is thicker to hold the screen, board and battery, but outside it keeps the same
>   surface as the rest of the body. The only visible joint is a thin, even seam
>   around the module.
> - **Battery** stays in the screen module, as decided earlier.
> - **Landscape:** sits on the bottom of the triangle. **Portrait:** stands on its end,
>   on the triangle outline. TPU for AMS pads go on both resting surfaces.
> - Keep the earlier requirements: stone-look surface, cream bezel matched to
>   LyricFlash, mic holes from LyricFlash, simple buttons, surface quality first.
> - **Proportions (picked 2026-11-11, see `ref/q1-angles.png`, `ref/q2-opening.png`, `ref/q3-corners.png`):**
>   - Front angle: **60° from the desk** (decided). Keep it a parameter. Final reference:
>     `ref/peak-final-60.png`.
>   - Opening: medium. Front wall thick enough for module + battery, back leg and bottom
>     about 9 mm (sketch values, refine once the module is measured).
>   - Corners: medium-soft (about 16 mm outer radius in the sketch). Not crisp, and not so
>     round it turns into a ring.
> - Still to explore, with renders: how it prints (split lines that keep layer lines off
>   the visible faces) and how the screen module attaches and comes out for service.
>
> **Materials, decided 2026-11-13 (Print 1): the body is TPU for AMS, whole body at T1 (2 walls, 10 %
> gyroid). No stone PLA and no other TPUs. No sharp edges anywhere: near full round on every outside
> edge, inside and out; only the screen opening stays crisp. The notes below are the earlier thinking.**
>
> **Materials (2026-11-11): body in TPU for AMS (68D) is a serious option.** Paul has
> plenty and wants to try it.
> - Softness comes from slicer settings: fewer walls and lower infill make TPU prints
>   more flexible. Use **Bambu Studio modifiers** (modifier meshes or height-range
>   modifiers) to make some areas soft and grippy (base edges, rear corners, outer skin)
>   and others firm (around the screen module and attachment points).
>   Starting points to test on coupons: soft = 2 walls + 10–15 % gyroid; firm = 4–5 walls + 30–40 %.
> - **Rigid inner frame:** TPU doesn't hold snap-fits, screws or the screen square. Plan a
>   thin rigid frame (PETG) that holds the screen module and battery, with the TPU body
>   around it. The frame does the holding and the TPU does the feel.
> - TPU for AMS notes: hardened steel nozzle recommended (0.4/0.6/0.8; not 0.2), dry
>   70 °C for 8 h, glue on the plate, keep humidity under 20 %, max overhang about 70°,
>   bridges up to about 20 mm. Check the triangle's inner top corner and opening against those.
> - **Fallback:** stone/marble-effect PLA for a rigid body. These hide layer lines well,
>   but most stone and marble PLAs soften at about 52 °C, so keep them away from sunny
>   windowsills. Some are abrasive (StoneFil is about 50 % stone powder), so use a hardened nozzle.
> - Plan: print the same coupon (a short slice of the triangle) in TPU for AMS at 2–3
>   infill/wall settings, plus one stone PLA, and let Paul handle them before picking.

Two parts: **screen unit** + **base**.

**Screen unit:** display, board and bezel, styled like the LyricFlash enclosure
(bezel, screen depth, rounded corners). No logos on the front (optionally a tiny
debossed SLOPEFORM wordmark on an edge).

**Battery lives in the screen unit** (decided by Paul, 2026-11-09). This means short
wires and no cable crossing between screen and base, so nothing gets pinched or flexes
when the screen is rotated or lifted off. The base is purely passive: no
electronics and no wiring. It needs its own mass or footprint to stay stable, because
the heavy battery is up in the screen unit.

**Side profile:** the mountain lives here, not on the front. The back of the screen unit
isn't a flat box: it rises to a low, soft ridgeline (asymmetric, like a slope profile),
with rounded transitions. The front stays a clean rounded rectangle.

**Back:** gently shaped "ridge" surface; magnets/locators for the base; wall-hang
keyhole or cleat slot; vent slots shaped as a short contour-line pattern (functional + brand).

**Base ("the Slope"):** separate printed base the screen unit sits on/in.
- Holds the screen in **landscape and portrait** (rotation is the headline feature).
- Viewing angle ~15–20° back from vertical (parametric).
- Heavy or wide enough that touch input doesn't tip it.
- Side silhouette = a mountain slope with rounded edges; looks good on its own.
- TPU for AMS feet and lining (see §4).
- Screen unit off the base = wall-hang.

**Colour:** default single colour (dark/charcoal or snow white); optional accent
(the "horizon line") via an AMS filament change at a set layer.

> All of the above is a direction, not a lock. §8 lists the open decisions to settle
> with Paul *before* CAD gets detailed.

---

## 4. Hard requirements

### Fit & internals (MEASURE, don't guess)
- [ ] Get the official Waveshare 2D/3D drawing (STEP if available) for the exact board +
      display. If unavailable, Paul measures with calipers. Record every dimension in
      `hardware/enclosure/dimensions.md` with its source (`datasheet` / `measured` / `STEP`).
- [ ] LiPo (8000 mAh): measure L×W×H **including** the protection board and wire exit.
      Leave ≥1 mm swell clearance on the thickness axis; never clamp the pouch.
- [ ] LIS2DW12 breakout: mount rigidly (orientation detection depends on it). Fix its
      axis orientation relative to the display and document it for firmware.
- [ ] Cable routing for battery, accelerometer (I2C SDA GPIO7 / SCL GPIO8, INT1 → GPIO21)
      and speaker, if used.

### Access (from architecture doc)
- [ ] UART USB-C reachable (flashing/recovery) — not just the charge port.
- [ ] **Buttons: keep it simple, don't over-engineer.** (The LyricFlash S3 case lost a lot
      of time to button actuators.) First find where BOOT/RESET/power actually sit on the
      board, then pick the cheapest option that works:
      1. Button already lands on an edge → plain cut-out, done.
      2. Rarely used (BOOT/RESET; recovery is mostly via the web UI / USB flasher) →
         pinhole for a paperclip, or "open the back to reach it".
      3. Only a button used **day-to-day** justifies a printed actuator/flexure.
      Propose the option per button and get Paul's OK before modelling any actuator.
      Timebox: if a button solution needs more than one test coupon iteration, stop and
      fall back to option 2.
- [ ] microSD slot reachable without opening the case (or documented as open-to-swap).
- [ ] Power switch, if added, recessed so the base/wall doesn't press it.
- [ ] Ports must stay usable in **both** landscape and portrait on the base
      (i.e. not on the edge that sits in the base in either orientation, or the base
      has a cable notch).

### Audio / LyricFlash
- [ ] **Mic holes: reuse the proven design from the LyricFlash enclosure** (the ESP32-S3
      1.75" case). It worked well, so copy its hole geometry exactly (diameter, wall
      thickness at the hole, any chamfer or countersink). Don't redesign it.
      Ask Paul for that enclosure's CAD/3MF or the hole dimensions before modelling.
      Place one hole over each onboard mic on this board (positions from the Waveshare
      drawing or measured), and check that neither the base nor the TPU parts cover them
      in landscape or portrait.
- [ ] Speaker grille if a speaker is fitted; keep it away from the mic port.

### Thermal & safety
- [ ] Passive venting for the ESP32-P4 and the charger IC; no airflow trapped over the battery.
- [ ] Battery physically separated from hot components where possible.
- [ ] Material must survive a sunny windowsill: default **PETG** (not PLA). Paul has
      tuned PETG HF profiles. ASA is an option for the base.

### TPU for AMS (Paul has plenty)
TPU for AMS, for our purposes, can be used for:
- non-slip feet on the base
- a soft lining where the screen unit meets the base
- bumpers
- soft corners and edges
- overall anti-slip

It's a material that won't mark paint. Some TPU pieces can be swapped out, and the case
still works without them.

- [ ] **Not for seals or gaskets.** TPU for AMS is 68D Shore hardness, too hard to seal.
      Don't use it for the mic port, display seat or any gasket.

### Assembly
- [ ] Screen unit: snap-fit shell (bezel + back shell); no glue. Optional M2/M2.5 heat-set
      inserts as a variant for people who want screws.
- [ ] Display is held by the frame, not the touch glass edge alone. No pressure on the
      touch panel.
- [ ] Opens with a fingernail/spudger notch for battery/SD service, without breaking clips.
- [ ] Magnets: standard N52 discs (e.g. 6×2 mm or 8×3 mm, parametric), press-fit pockets
      or a print-pause insertion. Polarity documented so the base works both orientations.

### Printability (Bambu P2S, Bambu Studio, 0.4 mm nozzle)
- [ ] **Surface quality beats print time.** Fewer visible layer lines is the goal:
      - choose each part's print orientation for its best visible faces first
      - fine layer heights on curved/rounded surfaces (variable layer height in Bambu
        Studio, or ≤0.12 mm where it matters)
      - ironing on flat top faces where it helps
      - seams hidden on a back or inside edge
      - document these settings in the 3MF
- [ ] TPU parts (2026-11-13): no flat ceilings. Undersides of lips and ledges are 45°; edges that touch
      the plate are 45° chamfers, not rounds.
- [ ] Body (2026-11-13): edges on the bed get a 45° chamfer blended into the round (no overhang);
      elephant-foot compensation on; seam on an inside edge; finer layers over the rounded edges.
- [ ] Every part prints without supports on visible faces. Supports allowed only inside.
- [ ] Bezel/front prints face-down for a clean front face.
- [ ] Wall thickness multiples of line width (0.42 → 1.68 / 2.1 mm).
- [ ] Snap-fit and press-fit clearances parametric (start 0.15–0.25 mm), validated with a
      **tolerance test coupon** before the full print.
- [ ] Each part fits a 256×256 mm plate.
- [ ] Deliver a 3MF with plates and per-part settings, plus STEP + STL.

---

## 5. Tooling

- **CAD:** Python + **build123d** (preferred) or CadQuery. Fully parametric; one
  `params.py` holds every dimension and clearance. OpenSCAD only if build123d is
  impossible in the environment.
- **Exports:** STEP (for remixers), STL, and 3MF (MakerWorld).
- **Renders:** generate preview PNGs (front, side, back, on-base landscape, on-base
  portrait) after every significant change so Paul can review without opening CAD.
- **Layout:**
  ```
  hardware/enclosure/
    ENCLOSURE.md        # this file
    dimensions.md       # every dimension, marked drawing / measured / PLACEHOLDER
    decisions.md        # dated log of design decisions and defaults to confirm
    REMAINING-TESTS.md  # the one checklist of tests and measurements still to do (+ .docx to fill in)
    COUPON-RESULTS.md   # record of finished tests and what each answer changed
    tests/              # pictures used by REMAINING-TESTS
    ref/                # reference images, LyricFlash 3MF
    cad/params.py       # every dimension and clearance
    cad/parts.py        # screen module (bezel, carrier plate, back cover) + inner frame
    cad/peak.py         # body, pads, full assembly, exports
    cad/test_coupons.py # material slices and fit coupons
    cad/test_pictures.py, cad/remaining_tests.py   # pictures and the REMAINING-TESTS documents
    cad/render_all.py   # review renders (cad/render/ = headless three.js renderer)
    coupons/            # ready-to-print coupon 3MF + STL
    renders/            # latest review renders
    out/                # STEP / STL / 3MF build output (gitignored)
  ```

---

## 6. Working agreement (how to collaborate with Paul)

- Paul designs by **discussion and iteration**, not big-bang rewrites. Make one focused
  change at a time, render it, show it, wait for feedback.
- Ask before locking anything in §8. Offer 2–3 options with a recommendation.
- **Show, don't describe.** Paul finds text-only descriptions hard to picture. Every
  design question comes with pictures: a rough render of each option (front, side,
  landscape and portrait), labelled A/B/C, so he can answer by picking a letter.
  Use plain words, not CAD jargon (say "the lip the screen rests on", not "retention
  flange"). If a term is unavoidable, point to it on the render.
- Ask questions one or two at a time, in the order they affect the design.
- Keep replies short and direct.
- Never invent dimensions. If a number is unknown, stop and ask Paul to measure, or use a
  clearly marked placeholder in `params.py` (`# PLACEHOLDER — measure`).
- Print-test small coupons before full parts: fit is validated on plastic, not on screen.
- Log each decision in `decisions.md` (date, decision, why, alternatives rejected).
- Do not create complex decorative artwork (logos, illustrations) in CAD from scratch;
  leave a placeholder region and ask Paul for an SVG.

---

## 7. Milestones

1. **Dimensions:** collect and verify all component dimensions → `dimensions.md`.
2. **Peak at true size:** rebuild `ref/peak-final-60.png` in CAD around the measured screen
   module and battery. Render it next to the reference for Paul to confirm.
3. **Material coupons:** a short slice of the triangle in TPU for AMS at 2–3 wall/infill
   settings, plus one stone PLA, so Paul can handle them and pick.
4. **Fit coupons:** screen module seat and seam, mic holes, port windows, inner-frame clips.
5. **Screen module + inner frame:** fit the hardware, ports and vents; module comes out for service.
6. **Body:** full Peak body, print splits that keep layer lines off visible faces, TPU pads,
   tip test in landscape and portrait.
7. **Polish:** wordmark, colours, print orientation, 3MF plates with modifiers.
8. **Release pack:** STEP/STL/3MF, print settings, assembly guide with photos, BOM.

---

## 8. Open decisions (settle with Paul, with renders)

> **2026-11-14 status:** 1 (TPU for AMS, T1), 2 (snap-fit module with four clips, pulled out of the
> open port end), 3 (stands on the end away from the USB-C ports) and 4 (a push-on port-end cap, six
> pegs, no glue) are decided and tested on coupons. For the rest, Claude picked defaults so the full
> model could be built; they are in `decisions.md` as "default, Paul to confirm". Open tests:
> `REMAINING-TESTS.md`; finished ones: `COUPON-RESULTS.md`.

1. **Body material:** TPU for AMS (which soft/firm settings) or stone-look PLA? Decided by coupons.
2. **Screen module attachment:** how it locks into the body and comes out for service.
3. **Portrait end:** which end it stands on (keeps ports and buttons reachable).
4. **Print splits:** where the body divides into printable parts, and how the seams look.
5. **LyricFlash scaling:** which bezel/depth values carry over directly and which scale
   with screen size?
6. **Colourway:** body colour, cream bezel, TPU pad colour.
7. **Speaker:** fitted or not (affects grille and internal volume)?
8. **Wall mount:** wanted at all for the Peak, and if so, how?
9. **Branding:** wordmark position, or none?
