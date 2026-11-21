# Remaining tests

**Left to do: A. Reprints: 1 · B. Tests not done yet: 0 · C. Needs the board: 1 · D. Measurements: 4**

Only what is still open. Finished tests are not here (the record of those is COUPON-RESULTS.md). Tick a box or write in a blank. Never press on the screen glass.

## Print settings (use these for every remaining piece)

| Piece | Settings |
|---|---|
| BTN-pin-rib2.8, -rib2.9, -rib3.0 (PETG) | 0.4 mm nozzle. 0.12 mm layers. Line width 0.42 mm for every line type. Wall generator Classic. 3 walls, 100 % infill (they are only 2.6 mm thick). Supports off. They stand on their heads (laid that way in the 3MF). Elephant foot compensation 0.15 mm. |
| Where the settings are | Bambu Studio: Process, tab Quality, 'Line width'; tab Strength, 'Wall generator'. Set both before slicing. Classic keeps every wall exactly 0.42 mm; the other generator (Arachne) squeezes wall widths to fit thin features, which changes how a lip or a pocket comes out. |

## A. Reprints (pieces changed by your last results)

One thing to print: three pins (coupons/buttons-petg.3mf). Glass ring, cap, clip, seat and back-cover tests are finished.

### A4. Button pin: which grip is right (third try)

![A4](tests/A4-buttons.png)

**What you're testing and why:** With no grip band the pins fell out; with the 3.2 mm band they stuck, were hard to pull, and one held the POWER button down. The right size is in between, so here are three pins that differ only in the band: 2.8, 2.9 and 3.0 mm, marked by 1, 2 or 3 grooves across the head. You try all three in the TPU strip you already printed and tell me which feels right. Only POWER gets a pin now, so the strip's three tunnels are just three places to try them.

| Piece | Where it is | Note |
|---|---|---|
| **BTN-pin-rib2.8 (1 groove), BTN-pin-rib2.9 (2 grooves), BTN-pin-rib3.0 (3 grooves)** | coupons/buttons-petg.3mf (or the three STLs) | PETG, new. Stand on their heads |
| **BTN-body, BTN-frame, BTN-cup** | already printed | reuse them |

**Print settings:** Pins: 0.12 mm layers, 3 walls, 100 % infill, line width 0.42, Classic walls, supports off, elephant foot compensation 0.15 mm. They stand on their heads.

**What to do:**

1. Stack the strip as before: BTN-cup (flat side towards the body), then BTN-frame, then BTN-body with its curved ridge at the far end. Line up one set of three holes.
2. Push the 1-groove pin in from the ridge end until it is through all three layers. Press it and let go. Tip the strip with the ridge down. Pull the pin out with a fingernail.
3. Do the same with the 2-groove pin, then the 3-groove pin (use a different set of holes if you like, to keep a fresh tunnel for each).

| Question | Answer |
|---|---|
| 1-groove pin (2.8 mm band): how does it feel? | ☐ falls out when tipped  ☐ just right: stays in, easy to press, easy to pull  ☐ a little stiff  ☐ too stiff, sticks |
| 2-groove pin (2.9 mm band): how does it feel? | ☐ falls out when tipped  ☐ just right: stays in, easy to press, easy to pull  ☐ a little stiff  ☐ too stiff, sticks |
| 3-groove pin (3.0 mm band): how does it feel? | ☐ falls out when tipped  ☐ just right: stays in, easy to press, easy to pull  ☐ a little stiff  ☐ too stiff, sticks |
| Which one would you choose? | ☐ 1 groove  ☐ 2 grooves  ☐ 3 grooves  ☐ none of them |
| With a free-moving pin, is the head easy to find and press? | ☐ yes  ☐ too small  ☐ too sunk in  ☐ too proud |

## B. Tests not done yet

Nothing left here. The seat test is finished, and the T1/T2/T3 material test was answered long ago (TPU for AMS, T1, no firm zone, 13 mm legs right).

## C. Needs the board (when it arrives)

Needs the pin you choose in A4 and the real board. The plate, ports, labels and most of the button test are done.

### C5. POWER pin, BOOT and RESET on the board

![C5](tests/D-board.png)

**What you're testing and why:** Only POWER goes to the outside of the case now (your call: RESET can be a programmed short press of POWER, and you have never needed BOOT). BOOT and RESET stay as small paperclip holes in the screen module. This checks the POWER pin (the one you pick in A4) returns when you let go, and that a paperclip reaches BOOT and RESET through the first two holes of BTN-cup (the holes in that piece are bigger than the real ones, which is fine for this check).

| Piece | Where it is | Note |
|---|---|---|
| **BTN-cup, the pin you chose in A4** | from test A4 | reuse them |

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
| Does the pin (the one you chose in A4) come back when you let go, so POWER is not held down? | ☐ yes  ☐ no, it stays down |
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
| A4 | which pin | `1 groove -> PIN_RIB_D 2.8, 2 -> 2.9, 3 -> 3.0. 'None': too loose -> try 3.05 and 3.1; too tight -> 2.7. Then retest in C5` |
| A4 | head | `too small -> PIN_HEAD_D (now 8.0); too sunk/proud -> PIN_PROUD (now 1.6); CBORE_D follows (8.6)` |
| C5 | click / return / hard press | `no click -> BTN_TIP_Y or BUTTON_HOLE_D; stuck -> PIN_RIB_D -0.1 (the grip band holds the pin down; 3.2 did); POWER no -> PIN_OVERTRAVEL` |
| D1 | T | `BATT_T (drives FRONT_T)` |
| D2 | L, W | `BATT_L, BATT_W; decides the speaker (decisions.md)` |
| D5 | wires | `battery orientation, WIRE_NOTCH` |
| D6 | accelerometer | `ACC_L, ACC_W, ACC_T, seat on the plate` |

</details>
