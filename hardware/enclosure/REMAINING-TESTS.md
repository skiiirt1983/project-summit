# Remaining tests

**Left to do: A. Reprints: 1 · B. Tests not done yet: 0 · C. Needs the board: 1 · D. Measurements: 4**

Only what is still open. Finished tests are not here (the record of those is COUPON-RESULTS.md). Tick a box or write in a blank. Never press on the screen glass.

## Print settings (use these for every remaining piece)

| Piece | Settings |
|---|---|
| BTN-pin-1, -2, -3 (PETG) | 0.4 mm nozzle. 0.12 mm layers. Line width 0.42 mm for every line type. Wall generator Classic. 3 walls, 100 % infill (they are only 2.6 mm thick). Supports off. They stand on their heads. Elephant foot compensation 0.15 mm. |
| BTN-body (TPU for AMS) | TPU for AMS profile speeds. 0.2 mm layers. Line width 0.42 mm. 2 walls, 10 % gyroid. Supports off. Elephant foot compensation 0.15 mm. Filament dried. Prints standing up. |
| Where the settings are | Bambu Studio: Process, tab Quality, 'Line width'; tab Strength, 'Wall generator'. Set both before slicing. Classic keeps every wall exactly 0.42 mm; the other generator (Arachne) squeezes wall widths to fit thin features, which changes how a lip or a pocket comes out. |

## A. Reprints (pieces changed by your last results)

Two things to print: the TPU strip (coupons/buttons-tpu.3mf) and the three pins (coupons/buttons-petg.3mf). Glass ring, cap, clip, seat and back-cover tests are finished.

### A4. Button pins, second try

![A4](tests/A4-buttons.png)

**What you're testing and why:** Last time the pins slid freely and the holes printed clean, but the heads were too small and too sunk in, and the pins fell out when the strip was tipped. Now: bigger heads that stand out more, a thin grip band on each pin that the soft body holds lightly (so they stay put but can still be pulled out), and a groove across the POWER head so you can find it by touch. Two pieces change: the TPU strip (its dimples are bigger) and the pins. BTN-frame and BTN-cup stay as printed. Update: only POWER gets a pin to the outside now (BOOT and RESET become small paperclip holes inside the module), so you only need to try ONE pin. The strip has three identical tunnels; use any one.

| Piece | Where it is | Note |
|---|---|---|
| **BTN-body** | coupons/buttons-tpu.3mf (or BTN-body.stl) | TPU, new: bigger dimples. Prints standing up |
| **BTN-pin-1, -2, -3** | coupons/buttons-petg.3mf (or BTN-pin-N.stl) | PETG, new. Stand on their heads. One is enough now; the others are spares |
| **BTN-frame, BTN-cup** | already printed | reuse them |

**Print settings:** BTN-body: TPU for AMS settings from the table. Pins: 0.12 mm layers, 3 walls, 100 % infill. Everything with line width 0.42, Classic walls.

**What to do:**

1. Stack the strip: BTN-cup (flat side towards the body), then BTN-frame, then the new BTN-body with its curved ridge at the far end. Line up the three pairs of holes.
2. Push a pin in from the ridge end until it is through all three layers (any one of the three holes). Notice how hard it is to push in.
3. Press the pin with a fingertip, then let go.
4. Hold the strip with the ridge pointing down and shake gently, then lay it on its side.
5. Pull the pin out again with a fingernail.

| Question | Answer |
|---|---|
| Is the pin easy to push in and press? | ☐ yes  ☐ a little stiff  ☐ too stiff, it sticks |
| Is the head easy to find and press with a fingertip? | ☐ yes  ☐ too small  ☐ too sunk in  ☐ too proud |
| Does the pin stay in when you tip the strip with the ridge down? | ☐ yes  ☐ it falls out |
| Can you pull a pin out again with a fingernail? | ☐ yes, easily  ☐ yes, but hard  ☐ no |
| Did the three dimples in BTN-body print clean (round, not blocked)? | ☐ yes  ☐ no |

## B. Tests not done yet

Nothing left here. The seat test is finished, and the T1/T2/T3 material test was answered long ago (TPU for AMS, T1, no firm zone, 13 mm legs right).

## C. Needs the board (when it arrives)

Needs the new pins from A4 and the real board. The plate, ports, labels and the first button test are done.

### C5. POWER pin, BOOT and RESET on the board

![C5](tests/D-board.png)

**What you're testing and why:** Only POWER goes to the outside of the case now (your call: RESET can be a programmed short press of POWER, and you have never needed BOOT). BOOT and RESET stay as small paperclip holes in the screen module. This checks the new POWER pin still returns when you let go, and that a paperclip reaches BOOT and RESET through the first two holes of BTN-cup (the holes in that piece are bigger than the real ones, which is fine for this check).

| Piece | Where it is | Note |
|---|---|---|
| **BTN-cup, one new BTN-pin** | from test A4 | reuse them |

**Print settings:** Nothing to print.

**What to do:**

1. Lay the board on the table with the buttons along the top edge, as in the picture. From the USB-C end: 1 = RESET, 2 = BOOT, 3 = POWER.
2. Hold BTN-cup about 3 mm from the buttons with the right-hand hole over POWER. Push the pin through it until it touches the button.
3. Press, let go. Does the board react once per press, and only while you press?
4. Switch the board off, then use the pin to switch it back on.
5. Slide BTN-cup to the left so the other two holes are over BOOT and RESET. Push a straightened paperclip through each and feel for the click.

| Question | Answer |
|---|---|
| Does the pin click POWER? | ☐ yes  ☐ no |
| Does the pin come back when you let go (POWER not held down)? | ☐ yes  ☐ no, it stays down |
| Can you switch the board back on through the pin? | ☐ yes, short press  ☐ yes, needs a hard press  ☐ no |
| Does a paperclip click BOOT and RESET through the first two holes? | ☐ both  ☐ only ___  ☐ neither |

## D. Measurements (calipers)

Calipers, in mm, one decimal. D1, D2 and D5 need the battery, D6 the accelerometer.

### D1. Battery thickness

![D1](tests/D-measure.png)

**What you're testing and why:** The most important number. It sets how thick the front wall of the case is.

**What to do:**

1. Lay the battery flat. Close the jaws across the thickest point, including the taped circuit board at the end. Just touching: do not squeeze.

| Question | Answer |
|---|---|
| Thickness (T) | ☐ ___ mm |

### D2. Battery length and width

**What you're testing and why:** Checks the battery fits its bay (106 x 70 mm now).

**What to do:**

1. Measure the outer edges of the pouch, including the folded seam and the circuit-board end.

| Question | Answer |
|---|---|
| Length (L) | ☐ ___ mm |
| Width (W) | ☐ ___ mm |

### D5. Battery wires

**What you're testing and why:** Decides which way round the battery goes and where its wires pass.

**What to do:**

1. Note which edge the wires leave from. Measure from the pouch to the plug.

| Question | Answer |
|---|---|
| Wires leave from | ☐ a short end  ☐ a long side |
| Wire length | ☐ ___ mm |

### D6. Accelerometer board

**What you're testing and why:** So its seat can be made the right size; it has to sit rigid and square.

**What to do:**

1. Measure the little board: length, width, total height including the tallest part and any pins, hole size, and the spacing between hole centres.

| Question | Answer |
|---|---|
| Length x width | ☐ ___ x ___ mm |
| Height | ☐ ___ mm |
| Hole size and spacing | ☐ hole ___ mm  ☐ spacing ___ mm |

---

<details><summary>For Claude: what each answer changes (ignore this)</summary>

| Test | Answer | Changes in cad/params.py |
|---|---|---|
| A4 | ease / stiff | `stiff/sticks -> PIN_RIB_D -0.1 (now 3.2) or TUNNEL_D +0.1; too loose later -> PIN_RIB_D +0.1` |
| A4 | head | `too small -> PIN_HEAD_D (now 8.0); too sunk/proud -> PIN_PROUD (now 1.2); CBORE_D follows (8.6)` |
| A4 | stay in | `fall out -> PIN_RIB_D +0.1, PIN_RIB_W +0.5, or a retaining lip in the dimple` |
| A4 | pull out | `hard -> PIN_RIB_D -0.1; no -> PIN_PROUD +0.4` |
| A4 | POWER groove | `no -> POWER_BAR deeper or longer` |
| A4 | dimples clean | `no -> TUNNEL_D +0.2 / CBORE_D +0.2 (teardrop in the TPU body)` |
| C5 | click / return / hard press | `no click -> BTN_TIP_Y or BUTTON_HOLE_D; stuck -> PIN_RIB_D -0.2 (the grip band holds the pin down); POWER no -> PIN_OVERTRAVEL` |
| D1 | T | `BATT_T (drives FRONT_T)` |
| D2 | L, W | `BATT_L, BATT_W; decides the speaker (decisions.md)` |
| D5 | wires | `battery orientation, WIRE_NOTCH` |
| D6 | accelerometer | `ACC_L, ACC_W, ACC_T, seat on the plate` |

</details>
