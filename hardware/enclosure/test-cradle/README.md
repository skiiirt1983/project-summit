# Test cradle for the Waveshare ESP32-P4-WIFI6-Touch-LCD-5

**Unofficial. For bench software testing only, not part of the Peak case.** Nothing here has been printed yet.
It protects the board while you flash and test: a bezel round the screen, both USB-C ports, the three buttons and the
microSD slot open, and the back left open at the sockets. No battery, no accelerometer.

## Print (TPU for AMS, no supports)

| File | What | Notes |
|---|---|---|
| `tray.3mf` / `tray.stl` | the box: base, walls, screw holes | prints base-down, already laid that way |
| `rings.3mf` | `bezel-ring` and `bezel-ring-snug` | prints front face down, already laid that way. **You need one ring.** Print `bezel-ring` first; `bezel-ring-snug` is a spare with a tighter fit in case the first sits loose |

Settings: TPU for AMS profile speeds, 0.2 mm layers, line width 0.42 mm, Classic walls, supports off, brim off, elephant foot
compensation 0.15 mm, filament dried. Walls and infill: 2 walls and 10 % gyroid (the "T1" setting you chose for the Peak)
is enough; 3 walls and 15 % makes it stiffer if you want a tougher box.
Outside size 134.8 x 78.6 mm, 15.75 mm tall, fits one plate each.

You also need **4 x M2.5 flat (countersunk) head screws, 6 mm** (8 mm also works). Do not bottom them out in the standoffs.

## Put it together

1. Lay the tray on the table. Drop the board in, screen up, so its four brass standoffs land on the four holes in the base.
2. From the underside, put in the four screws and snug them up (the TPU squashes a little; do not overtighten).
3. Press the bezel ring down on the top of the walls until the tongue is all the way into the groove. Push evenly.
4. To take it off, pry gently at a corner with a fingernail.

## What is open

- **USB-C ports** (both, at one end): one wide notch in the end wall, open at the top. The upper port is UART.
- **Buttons and microSD slot**: one notch in the long wall on the buttons side. Press the buttons with a fingertip or a pen
  tip. From the USB-C end they are RESET, BOOT, POWER.
- **Underside windows:** the 40-pin header (for jumper wires), speaker socket, battery socket, RTC socket, camera
  connector, and the two mic holes.
- The ring's underside is the roof of the notches. The glass is never pressed (0.25 mm of air under the lip).

## If something does not fit (all values are in `cradle.py`, block "TEST CRADLE")

| Problem | Change |
|---|---|
| Glass will not drop in | `GLASS_CLEAR` up by 0.1 (now 0.45; the PETG Peak needed 0.55) |
| Glass rattles | `GLASS_CLEAR` down by 0.1 |
| Ring sits loose or falls off | print `bezel-ring-snug`, or `GROOVE_FIT` to 0 |
| Ring too hard to press on | `GROOVE_FIT` up to 0.2 |
| Screws will not go in | `SCREW_HOLE_D` up by 0.1; check the screw is flat-head |

Then run `python cradle.py` (needs build123d, see `../cad/README.md`) and print the changed file.
`python render.py` redraws `cradle-views.png`.

## Checked in the model (not on plastic)

Every part is one valid solid. Both USB-C plugs (13.5 x 8 mm), a pen on each button, and a microSD card pass the wall
notches without touching anything. The tray and rings have no flat ceilings in their print orientation. The positions come
from the measurements on the real board in `../dimensions.md`.
