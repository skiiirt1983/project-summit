"""Screen module and inner frame, built in the module frame.

Module frame: origin at the centre of the bezel face, x along the long side
(USB-C ports at -x), y down the slope (buttons and card slot at -y), +z out of
the screen. Depths below the face are negative z.
"""
import math

from build123d import (Box, CounterSinkHole, Cylinder, Pos, Rectangle, RectangleRounded,
                       Rot, extrude)

import params as p


def slab(length, width, radius, z_top, z_bot):
    """Rounded rectangle solid between two depths."""
    return Pos(0, 0, z_bot) * extrude(RectangleRounded(length, width, radius), z_top - z_bot)


def _along_y(radius, length):
    return Rot(90, 0, 0) * Cylinder(radius, length)


def _along_x(radius, length):
    return Rot(0, 90, 0) * Cylinder(radius, length)


def teardrop(diameter, length, point=None):
    """A round hole along y, centred on the origin and running from y = 0 towards -y by `length`.
    With `point` = "-x" the hole gets a 45 degree peak on that side, so it prints without a ceiling
    (the TPU body stands with module -x pointing up)."""
    r = diameter / 2
    hole = Pos(0, -length / 2, 0) * _along_y(r, length)
    if point == "-x":
        diamond = Rot(0, 45, 0) * Box(2 * r, length, 2 * r)
        hole += Pos(0, -length / 2, 0) * (diamond & (Pos(-2 * r, 0, 0) * Box(4 * r, length + 2, 4 * r)))
    return hole


def button_xs():
    return [p.board_xy(xb, 0)[0] for xb in p.BUTTONS_X]


def button_tunnels(y_from, y_to, diameter, point=None):
    """One tunnel per button from y_from to y_to (both negative, y_to further out)."""
    out = None
    for x in button_xs():
        one = Pos(x, y_from, p.BTN_Z) * teardrop(diameter, abs(y_to - y_from), point)
        out = one if out is None else out + one
    return out


def button_pins():
    """The three PETG pusher pins at rest, in the module frame. Each is a Ø PIN_D shaft with a head."""
    out = None
    for i, x in enumerate(button_xs()):
        shaft = Pos(x, p.BTN_TIP_Y - p.PIN_LEN / 2, p.BTN_Z) * _along_y(p.PIN_D / 2, p.PIN_LEN)
        head_y = p.PIN_HEAD_REST_Y - p.PIN_HEAD_T / 2
        head = Pos(x, head_y, p.BTN_Z) * _along_y(p.PIN_HEAD_D / 2, p.PIN_HEAD_T)
        rib = Pos(x, p.PIN_RIB_Y, p.BTN_Z) * _along_y(p.PIN_RIB_D / 2, p.PIN_RIB_W)
        pin = shaft + head + rib
        if i == p.POWER_PIN:                                                       # POWER pin: a groove across its head
            groove = Pos(x, p.PIN_HEAD_REST_Y - p.PIN_HEAD_T + p.POWER_BAR[2] / 2 - 0.01, p.BTN_Z) * Box(
                p.POWER_BAR[0], p.POWER_BAR[2] + 0.02, p.POWER_BAR[1])
            pin = pin - groove
        out = pin if out is None else out + pin
    return out


def usb_tunnel(length, extra=0.0, size=None):
    """One plug-sized tunnel with rounded corners, running along x, centred on the origin."""
    wide, high = size or p.USB_WINDOW
    section = RectangleRounded(high + extra, wide + extra, p.USB_WINDOW_R)
    return Rot(0, 90, 0) * extrude(section, length / 2, both=True)


def usb_windows(x_from, x_to):
    """Two plug-sized tunnels running along x between the given positions."""
    out = None
    for yb in p.USB_Y:
        _, y = p.board_xy(0, yb)
        tunnel = Pos((x_from + x_to) / 2, y, -p.board_depth(p.USB_H)) * usb_tunnel(abs(x_to - x_from))
        out = tunnel if out is None else out + tunnel
    return out


def mic_points():
    """Where the mics are (holes in the carrier plate go here)."""
    return [p.board_xy(*m) for m in p.MICS]


def mic_exits():
    """Where the sound leaves through cover, frame and body: same position along the
    module, but moved in towards the centre line so the holes open into the hollow."""
    return [(x, math.copysign(p.MIC_EXIT_Y, y)) for x, y in mic_points()]


def clip_z():
    return -(p.MOD_D - p.CLIP_FROM_BACK)


def cup():
    """Cream bezel with walls: the front half of the module. Prints face down."""
    part = slab(p.MODULE_L, p.MODULE_W, p.BEZEL_R, 0, -p.MOD_D)
    part -= slab(p.OPEN_L, p.OPEN_W, p.OPEN_R, 1, -p.LIP_T - 0.01)                 # opening
    part -= slab(p.CAV1_L, p.CAV1_W, p.GLASS_R + p.GLASS_CLEAR, -p.LIP_T, -p.PLATE_Z)   # glass + display
    part -= slab(p.CAV2_L, p.CAV2_W, p.GLASS_R + p.GLASS_CLEAR + p.LEDGE,
                 -p.PLATE_Z, -p.MOD_D - 1)                                         # plate, battery, cover
    part -= usb_windows(-p.MODULE_L / 2 - 1, -p.GLASS_L / 2 + 1)
    wall_y = -(p.MODULE_W / 2 + p.CAV1_W / 2) / 2                                  # middle of the upper wall
    reach = p.BEZEL_WALL + 3
    part -= button_tunnels(-(p.CAV1_W / 2 - 0.5), -(p.MODULE_W / 2 + 1), p.BUTTON_HOLE_D)   # pin holes
    sd_x, _ = p.board_xy(sum(p.SD_X) / 2, 0)
    part -= Pos(sd_x, wall_y, -p.board_depth(p.SD_H)) * Box(p.SD_SLOT[0], reach, p.SD_SLOT[1])
    for side in (-1, 1):
        for x in p.CLIP_X:                                                         # grooves the frame clips bite into
            part -= Pos(x, side * (p.MODULE_W / 2 + p.CLIP_R - p.CLIP_BUMP - 0.1), clip_z()) * _along_x(
                p.CLIP_R + 0.1, p.CLIP_W)
        for x in p.SNAP_X:                                                         # grooves for the back-cover snaps
            part -= Pos(x, side * p.CAV2_W / 2, -(p.MOD_D - 3.0)) * Box(
                p.SNAP_LEN + 2, 2 * (p.SNAP_BUMP + 0.1), 1.6)
    return part


def plate():
    """Carrier plate: the board screws to it, and it divides board from battery."""
    length, width = p.CAV2_L - 2 * p.PLATE_CLEAR, p.CAV2_W - 2 * p.PLATE_CLEAR
    radius = p.GLASS_R + p.GLASS_CLEAR + p.LEDGE - p.PLATE_CLEAR
    part = slab(length, width, radius, -p.PLATE_Z, -p.PLATE_Z - p.PLATE_T)
    for xb, yb in p.STANDOFFS:                                                     # screws go in from the battery side
        x, y = p.board_xy(xb, yb)
        part -= Pos(x, y, -p.PLATE_Z - p.PLATE_T) * Rot(180, 0, 0) * CounterSinkHole(
            p.SCREW_D / 2, p.SCREW_HEAD_D / 2, p.PLATE_T)
    for x0, y0, x1, y1 in (p.HEADER_BOX, p.J5_BOX):                                # parts taller than the standoffs
        ax, ay = p.board_xy(x0, y0)
        bx, by = p.board_xy(x1, y1)
        part -= Pos((ax + bx) / 2, (ay + by) / 2, -p.PLATE_Z - p.PLATE_T / 2) * Box(
            abs(bx - ax) + 2, abs(by - ay) + 2, p.PLATE_T + 1)
    for x, y in mic_points():
        part -= Pos(x, y, -p.PLATE_Z - p.PLATE_T / 2) * Cylinder(p.MIC_CLEAR_D / 2, p.PLATE_T + 1)
    part -= Pos(-length / 2 + p.WIRE_NOTCH[1] / 2, 0, -p.PLATE_Z - p.PLATE_T / 2) * Box(
        p.WIRE_NOTCH[1] + 0.01, p.WIRE_NOTCH[0], p.PLATE_T + 1)
    return part


def battery_stop_x():
    """Battery sits against the +x end; everything left of this line stays free
    for the mics, the accelerometer and the wires."""
    inner_half = p.CAV2_L / 2 - p.COVER_CLEAR - p.COVER_RIM
    return inner_half - 0.5 - p.BATT_L - 0.5


def cover(test_slot_x=None):
    """Back cover: a shallow tub that snaps into the cup and holds the battery.

    The pull slot goes in the free strip beside the battery (the battery would block it anywhere
    else). The SNAP coupon has no free strip, so it passes test_slot_x for a test slot.
    """
    length, width = p.CAV2_L - 2 * p.COVER_CLEAR, p.CAV2_W - 2 * p.COVER_CLEAR
    radius = p.GLASS_R + p.GLASS_CLEAR + p.LEDGE - p.COVER_CLEAR
    top = -(p.PLATE_Z + p.PLATE_T)
    part = slab(length, width, radius, top, -p.MOD_D)
    part -= slab(length - 2 * p.COVER_RIM, width - 2 * p.COVER_RIM, max(radius - p.COVER_RIM, 0.5),
                 top + 1, -p.MOD_D + p.COVER_FLOOR)
    rib_h = 6.0
    part += Pos(battery_stop_x() - 0.6, 0, -p.MOD_D + p.COVER_FLOOR + rib_h / 2) * Box(
        1.2, width - 2 * p.COVER_RIM - 16, rib_h)                                  # battery stop
    for side in (-1, 1):
        for x in p.SNAP_X:
            part += Pos(x, side * width / 2, -(p.MOD_D - 3.0)) * Box(p.SNAP_LEN, 2 * p.SNAP_BUMP, 1.2)
    for x, y in mic_exits():                                                      # the LyricFlash mic hole
        part -= Pos(x, y, -p.MOD_D + p.COVER_FLOOR / 2) * Cylinder(p.MIC_HOLE_D / 2, p.COVER_FLOOR + 1)
    slot_x = test_slot_x if test_slot_x is not None else battery_stop_x() - 4.0
    part -= Pos(slot_x, 0, -p.MOD_D + p.COVER_FLOOR / 2) * Box(                    # paperclip pull slot
        p.PULL_SLOT[0], p.PULL_SLOT[1], p.COVER_FLOOR + 1)
    return part


def frame():
    """Rigid PETG sleeve that lives in the body. The module clips into it."""
    floor_z = -(p.MOD_D + p.FRAME_CLEAR)
    inner_l, inner_w = p.MODULE_L + 2 * p.FRAME_CLEAR, p.MODULE_W + 2 * p.FRAME_CLEAR
    part = slab(p.FRAME_L, p.FRAME_W, p.FRAME_R, -p.FRAME_TOP, floor_z - p.FRAME_FLOOR)
    part -= slab(inner_l, inner_w, p.BEZEL_R + p.FRAME_CLEAR, 1, floor_z)
    part -= Pos(0, 0, floor_z - p.FRAME_FLOOR / 2) * Cylinder(p.PUSH_HOLE_D / 2, p.FRAME_FLOOR + 1)
    for x, y in mic_exits():
        part -= Pos(x, y, floor_z - p.FRAME_FLOOR / 2) * Cylinder(p.MIC_CLEAR_D / 2, p.FRAME_FLOOR + 1)
    part -= usb_windows(-p.FRAME_L / 2 - 1, -p.MODULE_L / 2 + 1)
    part -= button_tunnels(-(p.MODULE_W / 2), -(p.FRAME_W / 2 + 1), p.TUNNEL_D)        # pin guides
    tip = clip_z() + 3.0                                                           # free end, towards the front
    for side in (-1, 1):
        y_wall = side * (inner_w / 2 + p.FRAME_WALL / 2)
        for x in p.CLIP_X:
            for dx in (-1, 1):                                                     # slots either side of the finger
                part -= Pos(x + dx * (p.CLIP_W + p.CLIP_SLOT) / 2, y_wall, tip - p.CLIP_LEN / 2) * Box(
                    p.CLIP_SLOT, p.FRAME_WALL + 1, p.CLIP_LEN)
            part -= Pos(x, y_wall, tip + p.CLIP_SLOT / 2) * Box(
                p.CLIP_W + 2 * p.CLIP_SLOT, p.FRAME_WALL + 1, p.CLIP_SLOT)          # slot across the tip
            bump = Pos(x, side * (inner_w / 2 + p.CLIP_R - p.CLIP_BUMP - p.FRAME_CLEAR), clip_z()) * _along_x(
                p.CLIP_R, p.CLIP_W - 2)
            part += bump
    c = p.FRAME_TOP_CHAMFER                                                        # tuck under the sloped lip
    ring = slab(p.FRAME_L + 6, p.FRAME_W + 6, p.FRAME_R + 3, -p.FRAME_TOP + 0.01, -p.FRAME_TOP - c)
    ring -= Pos(0, 0, -p.FRAME_TOP - c) * extrude(RectangleRounded(p.FRAME_L, p.FRAME_W, p.FRAME_R), c + 0.02, taper=45)
    return part - ring


def body_cutter():
    """Everything removed from the body's front wall, in the module frame.

    The lip that hides the frame has a 45 degree underside: a short straight land at the
    screen opening, then the pocket widens at 45 degrees until it reaches the frame's size.
    """
    frame_l, frame_w = p.FRAME_L + 2 * p.FRAME_FIT, p.FRAME_W + 2 * p.FRAME_FIT
    frame_r = p.FRAME_R + p.FRAME_FIT
    cut = slab(p.MODULE_L + 2 * p.SEAM, p.MODULE_W + 2 * p.SEAM, p.BEZEL_R + p.SEAM, 1, -p.LIP_LAND - 0.01)
    cut += Pos(0, 0, -p.LIP_D) * extrude(RectangleRounded(frame_l, frame_w, frame_r), p.LIP_RUN, taper=45)
    cut += slab(frame_l, frame_w, frame_r, -p.LIP_D, -p.POCKET_D)
    # The body prints standing on its end with the ports up, so the pocket's port end is a
    # ceiling. Give it 45 degree sides; only a ROOF_FLAT-wide strip in the middle stays flat.
    # (Not needed with the port-end cap: the cap prints with this end facing up.)
    if not p.CAP:
        span_z = p.POCKET_D - p.LIP_D
        span_y = frame_w - 2 * frame_r * (1 - math.sin(math.radians(45)))
        roof = Rot(0, -90, 0) * extrude(Rectangle(span_z, span_y), p.ROOF_H, taper=45)
        cut += Pos(-frame_l / 2 + 0.01, 0, -(p.LIP_D + p.POCKET_D) / 2) * roof
    # Pin tunnels (teardrop: the body prints standing, module -x up) and the dimples on the ridge.
    cut += button_tunnels(-(p.FRAME_W / 2 - 0.5), -99, p.TUNNEL_D, "-x")
    cut += button_tunnels(p.PIN_FLOOR_Y, -99, p.CBORE_D, "-x")
    skin_z = -p.POCKET_D - p.BACK_SKIN / 2
    cut += Pos(0, 0, skin_z) * Cylinder(p.PUSH_HOLE_D / 2, p.BACK_SKIN + 2)
    for x, y in mic_exits():
        cut += Pos(x, y, skin_z) * Cylinder(p.MIC_CLEAR_D / 2, p.BACK_SKIN + 2)
    cut += usb_windows(-p.BODY_WIDTH / 2 - 1, -p.FRAME_L / 2 + 1)
    tip = clip_z() + 3.0
    seam_x = -(p.BODY_WIDTH / 2 - p.CAP_T)                                         # module x of the cap seam
    for side in (-1, 1):                                                           # room for the clips to flex
        for x in p.CLIP_X:
            width = p.CLIP_W + 2 * p.CLIP_SLOT
            x_lo = seam_x - 1 if p.CAP else x - width / 2      # with the cap: run up to the open top, no ceiling
            cut += Pos((x_lo + x + width / 2) / 2, side * (p.FRAME_W / 2 + p.FRAME_FIT), tip - p.CLIP_LEN / 2) * Box(
                x + width / 2 - x_lo, 2 * (p.CLIP_BUMP + 0.3), p.CLIP_LEN + 1)
    if p.CAP:                                                                      # teardrop top on the finger hole
        r = p.PUSH_HOLE_D / 2
        peak_ = Rot(0, 0, 45) * Box(2 * r, 2 * r, p.BACK_SKIN + 2) & Pos(-2 * r, 0, 0) * Box(4 * r, 4 * r, 10)
        cut += Pos(0, 0, skin_z) * peak_
    return cut


def firm_zone():
    """Modifier volume for Bambu Studio: the stiff region around the frame pocket."""
    m = p.FIRM_ZONE_MARGIN
    return slab(p.FRAME_L + 2 * m, p.FRAME_W + 2 * m, p.FRAME_R + m, 5, -p.POCKET_D - p.BACK_SKIN - 5)


# --- stand-ins for the bought parts (renders and checks only) -------------------
def glass():
    return slab(p.GLASS_L, p.GLASS_W, p.GLASS_R, -p.GLASS_RECESS, -p.GLASS_RECESS - 1.3)


def view():
    return slab(p.VIEW_L, p.VIEW_W, 0.5, -p.GLASS_RECESS + 0.1, -p.GLASS_RECESS - 0.05)


def display():
    """LCD + board + parts as one block, up to the standoff tops."""
    x_mid = p.BOARD_X0 + p.BOARD_L / 2
    return Pos(x_mid, 0, 0) * slab(p.BOARD_L + 0.2, p.BOARD_W + 0.2, 1.0,
                                   -p.GLASS_RECESS - 1.3, -p.PLATE_Z)


def battery():
    x_mid = battery_stop_x() + 0.5 + p.BATT_L / 2
    top = -(p.PLATE_Z + p.PLATE_T + p.BATT_GAP)
    return Pos(x_mid, 0, 0) * slab(p.BATT_L, p.BATT_W, 2.0, top, top - p.BATT_T)


def accelerometer():
    x_mid = battery_stop_x() - 1.5 - p.ACC_W / 2
    top = -(p.PLATE_Z + p.PLATE_T)
    return Pos(x_mid, 0, top - p.ACC_T / 2) * Box(p.ACC_W, p.ACC_L, p.ACC_T)
