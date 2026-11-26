"""Test cradle for the Waveshare ESP32-P4-WIFI6-Touch-LCD-5 (UNOFFICIAL: for bench software testing only).

Two TPU for AMS parts, no supports, no flat ceilings:
  tray        base with four screw holes + walls. The board stack drops in from the front and is screwed down
              onto the base through the board's own four standoffs. Walls are cut away (open at the top) at the
              USB-C ports, the microSD slot and the three buttons, and the base has windows for the 40-pin
              header, speaker / battery / RTC sockets, the camera connector and the two mic holes.
  bezel ring  front frame with a slim lip over the glass. It sits on the tray's walls by a tongue and groove
              (friction fit, comes off by hand). The ring's underside is the roof over the cut-outs.

Frame (the same as ../cad): x along the long side (USB-C at -x), y across (buttons at -y), z out of the screen,
0 at the front face of the bezel, negative into the case. All numbers come from ../cad/params.py (measured on the
real board) except the ones in the TEST CRADLE block below.

Run:  python cradle.py   ->  STL + 3MF + render inputs in this folder
"""
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "cad"))

from build123d import (Axis, Box, Cylinder, Cone, Mesher, Pos, Rot, RectangleRounded, chamfer, export_stl,  # noqa: E402
                       extrude)

import params as p  # noqa: E402

# ---- TEST CRADLE settings (TPU for AMS; tune after the first print) --------------------------------------------
GLASS_CLEAR = 0.45         # room between the glass edge and the pocket wall, per side. PETG needed 0.55 (0.30 did not fit); the soft TPU can be a little tighter
WALL = 3.5                 # wall around the pocket (8 lines of 0.42)
LIP_T = 1.2                # bezel lip over the glass
LIP_OVERLAP = 1.35         # how far the lip covers the glass edge (same as the LyricFlash bezel)
LIP_GAP = 0.25             # air between the lip and the glass (never press the glass)
BASE_T = 2.5               # base thickness
OUT_R = 7.0                # outer corner radius
CHAMFER = 1.0              # 45 degree chamfer on the edge that sits on the print plate (a round sags in TPU)
TONGUE_W = 1.4             # tongue on the tray walls
TONGUE_H = 2.0
GROOVE_FIT = 0.10          # groove is this much wider than the tongue (the TPU hole fit that worked on the cap pegs)
SNUG_GROOVE_FIT = 0.0      # the spare "snug" ring, in case the standard ring is loose
SCREW_HOLE_D = 2.9         # M2.5 clearance
SCREW_HEAD_D = 5.4         # countersink for a flat-head M2.5 screw, wide end on the underside
WINDOW_MARGIN = 1.5        # extra room round the sockets in the base
MIC_HOLE_D = 4.0
NOTCH_MARGIN = 1.0

# ---- derived ------------------------------------------------------------------------------------------------
Z_GLASS = -(LIP_T + LIP_GAP)                         # front of the glass
Z_BOARD = Z_GLASS - p.GLASS_TO_BOARD                 # the board's component face (it faces the base)
Z_BASE_TOP = Z_GLASS - (p.GLASS_TO_BOARD + p.STANDOFF_H)   # standoff tips land here
Z_BASE_BOT = Z_BASE_TOP - BASE_T
Z_WALL_TOP = -(LIP_T + 2.0)                          # tray walls end here; the tongue goes on up to -LIP_T
Z_TONGUE_TOP = -LIP_T

CAV_L, CAV_W = p.GLASS_L + 2 * GLASS_CLEAR, p.GLASS_W + 2 * GLASS_CLEAR
CAV_R = p.GLASS_R + GLASS_CLEAR
OUT_L, OUT_W = CAV_L + 2 * WALL, CAV_W + 2 * WALL
OPEN_L, OPEN_W = p.GLASS_L - 2 * LIP_OVERLAP, p.GLASS_W - 2 * LIP_OVERLAP
OPEN_R = max(p.GLASS_R - LIP_OVERLAP, 0.5)


def slab(length, width, radius, z_bot, z_top):
    return Pos(0, 0, z_bot) * extrude(RectangleRounded(length, width, radius), z_top - z_bot)


def ring_between(inner_off, outer_off, z_bot, z_top):
    """A rounded-rectangle ring whose edges are `inner_off` and `outer_off` outside the pocket wall."""
    outer = slab(CAV_L + 2 * outer_off, CAV_W + 2 * outer_off, CAV_R + outer_off, z_bot, z_top)
    return outer - slab(CAV_L + 2 * inner_off, CAV_W + 2 * inner_off, CAV_R + inner_off, z_bot - 1, z_top + 1)


def bed_chamfer(part, at_top):
    """45 degree chamfer round the outer edge of the face that sits on the plate."""
    z = part.bounding_box().max.Z if at_top else part.bounding_box().min.Z
    face = [f for f in part.faces() if abs(f.center().Z - z) < 1e-6 and f.normal_at().Z * (1 if at_top else -1) > 0.99]
    outer = max(face, key=lambda f: f.area).outer_wire().edges()
    return chamfer(outer, CHAMFER)


def bx(xb0, yb0, xb1, yb1):
    """A board-frame rectangle (x0, y0, x1, y1) as a module-frame centre and size."""
    ax, ay = p.board_xy(xb0, yb0)
    bx_, by = p.board_xy(xb1, yb1)
    return ((ax + bx_) / 2, (ay + by) / 2), (abs(bx_ - ax), abs(by - ay))


def tray():
    outer = slab(OUT_L, OUT_W, OUT_R, Z_BASE_BOT, Z_WALL_TOP)
    outer = bed_chamfer(outer, at_top=False)
    mid = WALL / 2
    tongue = ring_between(mid - TONGUE_W / 2, mid + TONGUE_W / 2, Z_WALL_TOP - 0.01, Z_TONGUE_TOP)
    part = outer + tongue
    part -= slab(CAV_L, CAV_W, CAV_R, Z_BASE_TOP, Z_WALL_TOP + 1)                      # the pocket the stack drops into

    # Screw holes at the four standoffs: countersunk on the underside (wide end on the plate: no ceiling).
    for xb, yb in p.STANDOFFS:
        x, y = p.board_xy(xb, yb)
        part -= Pos(x, y, Z_BASE_BOT + BASE_T / 2) * Cylinder(SCREW_HOLE_D / 2, BASE_T + 2)
        cone_h = SCREW_HEAD_D / 2 - SCREW_HOLE_D / 2                                      # 45 degrees: as tall as it is wide
        part -= Pos(x, y, Z_BASE_BOT + cone_h / 2) * Cone(SCREW_HEAD_D / 2, SCREW_HOLE_D / 2, cone_h)

    # Cut-outs in the walls, open at the top (the ring's underside is their roof).
    z_open_top = Z_WALL_TOP + 3.0                                                         # through the tongue as well
    # USB-C end: both ports in one notch, down to the plug's lower edge (it dips 1.2 into the base).
    y_a, y_b = (p.board_xy(0, yb)[1] for yb in p.USB_Y)
    half = p.USB_WINDOW[0] / 2 + NOTCH_MARGIN
    y_lo, y_hi = min(y_a, y_b) - half, max(y_a, y_b) + half
    z_usb = Z_BOARD - p.USB_H
    z_bot = z_usb - p.USB_WINDOW[1] / 2
    x_out, x_in = -(OUT_L / 2) - 1.0, -(CAV_L / 2) + 1.2                                  # through the wall and 1.2 into the pocket
    part -= Pos((x_out + x_in) / 2, (y_lo + y_hi) / 2, (z_bot + z_open_top) / 2) * Box(
        x_in - x_out, y_hi - y_lo, z_open_top - z_bot)
    # microSD slot and the three buttons: one notch in the long wall on the buttons side, down to the base.
    x_sd0 = p.board_xy(p.SD_X[0] - 1.0, 0)[0]
    x_btn = p.board_xy(p.BUTTONS_X[-1] + 4.5, 0)[0]
    x_lo, x_hi = min(x_sd0, x_btn), max(x_sd0, x_btn)
    y_out, y_in = -(OUT_W / 2) - 1.0, -(CAV_W / 2) + 1.2
    part -= Pos((x_lo + x_hi) / 2, (y_out + y_in) / 2, (Z_BASE_TOP + z_open_top) / 2) * Box(
        x_hi - x_lo, y_in - y_out, z_open_top - Z_BASE_TOP)

    # Windows through the base (vertical, so no ceilings): sockets that face the base, camera, mics.
    for name, box in (("header", p.HEADER_BOX), ("speaker", p.J5_BOX), ("battery", (30.85, 1.02, 38.55, 6.22)),
                      ("rtc", (112.11, 13.36, 117.11, 16.76)), ("camera", (89.5, 26.7, 95.3, 37.8))):
        xb0, yb0, xb1, yb1 = box
        (cx, cy), (w, h) = bx(xb0 - WINDOW_MARGIN, yb0 - WINDOW_MARGIN, xb1 + WINDOW_MARGIN, yb1 + WINDOW_MARGIN)
        part -= Pos(cx, cy, Z_BASE_BOT + BASE_T / 2) * extrude(RectangleRounded(w, h, 1.0), BASE_T + 2, both=True)
    for xb, yb in p.MICS:
        x, y = p.board_xy(xb, yb)
        part -= Pos(x, y, Z_BASE_BOT + BASE_T / 2) * Cylinder(MIC_HOLE_D / 2, BASE_T + 2)
    return part


def bezel_ring(groove_fit=GROOVE_FIT):
    z_bot = Z_WALL_TOP
    part = slab(OUT_L, OUT_W, OUT_R, z_bot, 0.0)
    part = bed_chamfer(part, at_top=True)
    part -= slab(OPEN_L, OPEN_W, OPEN_R, z_bot - 1, 1)                                     # the opening: stays crisp
    part -= slab(CAV_L, CAV_W, CAV_R, z_bot - 1, -LIP_T)                                   # room for the glass edge
    mid = WALL / 2
    part -= ring_between(mid - TONGUE_W / 2 - groove_fit / 2, mid + TONGUE_W / 2 + groove_fit / 2,
                         z_bot - 1, Z_TONGUE_TOP)                                          # groove for the tongue
    return part


# ---- stand-ins for the real board, only to check clearances and to render ----------------------------------------
def stand_ins():
    glass = slab(p.GLASS_L, p.GLASS_W, p.GLASS_R, Z_GLASS - 1.3, Z_GLASS)
    cx, cy = (p.board_xy(0, 0)[0] + p.board_xy(p.BOARD_L, p.BOARD_W)[0]) / 2, 0.0
    lcd = Pos(cx, cy, 0) * slab(p.BOARD_L, p.BOARD_W, 1.0, Z_BOARD, Z_GLASS - 1.3)
    parts = {"glass": glass, "board": lcd}
    standoffs = None
    for xb, yb in p.STANDOFFS:
        x, y = p.board_xy(xb, yb)
        s = Pos(x, y, (Z_BOARD + Z_BASE_TOP) / 2) * Cylinder(2.75, Z_BOARD - Z_BASE_TOP)
        standoffs = s if standoffs is None else standoffs + s
    parts["standoffs"] = standoffs
    comps = None
    for xb0, yb0, xb1, yb1, h0, h1 in ((-1.32, 18.5, 6.4, 28.0, 0, 3.25), (-1.32, 36.4, 6.4, 46.0, 0, 3.25),   # USB-C shells
                                       (70.49, 61.3, 99.0, 65.25, -0.4, 1.8),                                  # buttons
                                       (51.1, 49.5, 67.2, 64.5, -0.6, 1.85),                                  # microSD socket
                                       p.HEADER_BOX + (0, 5.2)):                                              # 40-pin header
        (cx_, cy_), (w, h) = bx(xb0, yb0, xb1, yb1)
        c = Pos(cx_, cy_, Z_BOARD - (h0 + h1) / 2) * Box(w, h, h1 - h0)
        comps = c if comps is None else comps + c
    parts["components"] = comps
    return parts


def lay_flat(shape, flip=False):
    if flip:
        shape = Rot(180, 0, 0) * shape
    bb = shape.bounding_box()
    return Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * shape


def build():
    return {"tray": tray(), "ring": bezel_ring(), "ring-snug": bezel_ring(SNUG_GROOVE_FIT)}


COLOURS = {"tray": "#4a4f55", "ring": "#8d8f8c", "ring-snug": "#8d8f8c", "glass": "#0b0b0d", "board": "#2f6b46",
           "standoffs": "#c8a24a", "components": "#c9ccd1"}

if __name__ == "__main__":
    built = build()
    prints = {"tray": lay_flat(built["tray"]), "bezel-ring": lay_flat(built["ring"], flip=True),
              "bezel-ring-snug": lay_flat(built["ring-snug"], flip=True)}
    for name, shape in prints.items():
        export_stl(shape, str(HERE / f"{name}.stl"), tolerance=0.02, angular_tolerance=0.1)
        print(f"{name}: valid={shape.is_valid} solids={len(shape.solids())} size={[round(v, 1) for v in shape.bounding_box().size]}")
    for stem, names in (("tray", ["tray"]), ("rings", ["bezel-ring", "bezel-ring-snug"])):
        mesher, y = Mesher(), 0.0
        for n in names:
            part = Pos(0, y, 0) * prints[n]
            part.label = n
            mesher.add_shape(part)
            y += prints[n].bounding_box().size.Y + 6
        mesher.write(str(HERE / f"{stem}.3mf"))
    print("Z levels: lip", -LIP_T, "glass", round(Z_GLASS, 2), "board", round(Z_BOARD, 2), "base top", round(Z_BASE_TOP, 2),
          "base bottom", round(Z_BASE_BOT, 2), "| outside", round(OUT_L, 1), "x", round(OUT_W, 1), "x", round(-Z_BASE_BOT, 2))
