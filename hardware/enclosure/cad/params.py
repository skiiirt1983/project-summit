"""Every dimension and clearance for the SLOPEFORM Home enclosure (mm, degrees).

Source tags match ../dimensions.md:
  drawing      Waveshare drawing or STEP
  measured     measured by Paul, or read off the LyricFlash 3MF
  PLACEHOLDER  a guess with extra room; must be measured before a real print
  COUPON       a fit or material value to tune after the test prints
               (see ../COUPON-RESULTS.md for which answer changes which value)
"""

# =============================================================================
# Screen module hardware: Waveshare ESP32-P4-WIFI6-Touch-LCD-5
# Board frame: seen from the back, USB-C on the left. x from the USB end,
# y up from the 40-pin-header edge, heights above the board's component face.
# =============================================================================
GLASS_L = 126.9            # drawing
GLASS_W = 70.7             # drawing
GLASS_R = 3.0              # drawing
VIEW_L = 110.32            # drawing (visible screen area)
VIEW_W = 62.28             # drawing
BOARD_L = 118.5            # drawing
BOARD_W = 64.5             # drawing
BOARD_INSET_USB_END = 1.55   # drawing: board edge sits this far inside the glass edge
GLASS_TO_BOARD = 7.8       # measured: glass front -> top of a standoff is 11.8 (drawing said 11.6), so the extra 0.2
                           # went here (drawing/STEP: 7.6)
STANDOFF_H = 4.0           # drawing: M2.5 threaded standoffs above the board
STANDOFFS = ((3.25, 3.75), (115.25, 3.75), (3.25, 60.75), (115.25, 60.75))  # drawing
MODULE_DEPTH = 11.6        # measured on the real board: glass front -> top of the 40-pin header (the STEP said 12.8,
                           # the 2D drawing 11.6). Not used by the model: the standoffs (11.8) are taller than the header.
                           # Old note:
                           # The 2D drawing says 11.6; the larger one is used.  # PLACEHOLDER — measure
HEADER_BOX = (54.55, 1.12, 105.85, 7.72)   # drawing (STEP): 40-pin header, 5.2 tall
J5_BOX = (26.31, 57.93, 32.07, 62.78)      # drawing (STEP): 2-pin socket, 4.3 tall
MICS = ((14.55, 2.37), (14.55, 62.17))     # drawing (STEP): mic centres; they face the back
BUTTONS_X = (73.22, 85.18, 96.73)          # measured on the real board (2026-11-20) from the outer face of the USB-C
                                           # connector (74.54 / 86.50 / 98.05), minus the 1.32 mm it sticks out past the board edge.
                                           # So the drawing (72.76, 84.44, 96.12) was right to within 0.5 to 0.7 mm
POWER_PIN = 2                              # index in BUTTONS_X of the POWER button (C4: 1 = RESET, 2 = BOOT, 3 = POWER)
BUTTON_H = 1.4             # measured (newer board): button centre above the board (drawing: 0.7)
BUTTON_STICKOUT = 0.75     # measured: how far the buttons stand out past the board's top edge (drawing: 0.5)
PIN_BUTTONS = (2,)           # which buttons get a pin to the outside. Paul, 2026-11-20: only POWER. BOOT and RESET are small
                           # paperclip holes in the screen module (BUTTON_PIN_D), reached with the module pulled out.
                           # (0, 1, 2) is what the printed A4 test strip has
SD_X = (51.1, 67.2)        # drawing (STEP): microSD slot along the buttons edge
SD_H = 0.6                 # drawing (STEP): slot centre above the board
USB_Y = (23.26, 41.21)     # drawing: USB-C centres on the x = 0 end
USB_H = 1.2                # drawing (STEP): port centre above the board
USB_END = -1               # decided 2026-11-11: ports face the right end seen from the front

# --- Battery (8000 mAh LiPo) -------------------------------------------------
BATT_L = 105.0             # PLACEHOLDER — measure (generous; typical packs are <= 100)
BATT_W = 66.0              # PLACEHOLDER — measure (generous; typical packs are <= 65)
BATT_T = 13.0              # PLACEHOLDER — measure (generous; typical packs are 9-12)
BATT_SWELL = 1.0           # brief §4: never clamp the pouch
BATT_GAP = 0.6             # air between carrier plate and battery (the plate is the divider)

# --- Accelerometer breakout (LIS2DW12) ---------------------------------------
ACC_L = 26.0               # PLACEHOLDER — measure (generous)
ACC_W = 18.0               # PLACEHOLDER — measure (generous)
ACC_T = 5.0                # PLACEHOLDER — measure (board + parts + solder)

# =============================================================================
# Bezel: copied from LyricFlash (measured from its 3MF)
# =============================================================================
LIP_T = 1.0                # measured (LyricFlash): thickness of the lip over the glass
LIP_OVERLAP = 1.35         # measured (LyricFlash): how far the lip covers the glass edge
LIP_GAP = 0.25             # measured (LyricFlash)  # COUPON: tune after test print (GLASS RING)
GLASS_RECESS = LIP_T + LIP_GAP   # screen depth below the bezel face
BEZEL_WALL = 3.40          # cream wall outside the glass edge. Visible cream = BEZEL_WALL +
                           # LIP_OVERLAP = 4.5 (option B; default, Paul to confirm. LyricFlash is 2.95)
BEZEL_R = 8.50             # outer corner radius (LyricFlash 6.7 + the extra bezel width)
GLASS_CLEAR = 0.55         # gap glass edge -> pocket wall, per side. GLASS RING test: 0.30 did not fit, 0.55 'just right'.
                           # BEZEL_WALL grew by 0.25 with it, so the cup walls and every tested clearance stay as they were

# =============================================================================
# Screen module: cream cup (bezel + walls), carrier plate, back cover
# =============================================================================
LEDGE = 1.0                # step in the cup wall that the carrier plate seats against
PLATE_T = 2.0              # carrier plate; screws to the board's four M2.5 standoffs
PLATE_CLEAR = 0.175        # plate -> cup wall, per side. FINAL: tight on the full-width SNAP piece but fine
                           # (Paul); the screws to the board hold the plate, so snug is good. Do not loosen.
SCREW_D = 2.9              # M2.5 clearance hole
SCREW_HEAD_D = 5.2         # M2.5 countersunk head
COVER_FLOOR = 1.68         # back cover floor (4 lines at 0.42)
COVER_RIM = 1.26           # back cover rim (3 lines)
PULL_SLOT = (2.5, 10.0)    # slot through the cover floor for a bent paperclip, like the button holes.
                           # SNAP test: a fingernail cannot get behind a flush cover, so it needs a tool.
                           # COUPON: tune after test print (SNAP)
COVER_CLEAR = 0.20         # back cover -> cup wall, per side  # COUPON: tune after test print (SEAT)
SNAP_BUMP = 0.50           # back-cover snap ridge height. SNAP test: 0.40 held but weak
                           # COUPON: tune after test print (SNAP)
SNAP_LEN = 20.0            # back-cover snap ridge length
SNAP_X = (-38.0, 38.0)     # two ridges per long side, near the ends. (They were one ridge in the middle,
                           # which the SEAT corner coupon did not contain, so its cover could not hold.)
WIRE_NOTCH = (14.0, 6.0)   # PLACEHOLDER — battery + accelerometer wires through the plate
# Button access (2026-11-16). Paul: the newest Waveshare board needs a hard POWER press to turn back on,
# so every button (POWER, BOOT, RESET) must be pressable from outside with the case assembled. Each
# button gets a printed pusher pin (PETG) running up the slope from the button, through the module's
# top wall, the frame and the TPU, to a dimple on the apex ridge. No battery switch is fitted.
# All values below are untested: COUPON: tune after test print (BTN) and on the real board.
BUTTON_PIN_D = 1.8         # paperclip hole over BOOT and RESET (no pin, no tunnel through the frame or body)
BUTTON_HOLE_D = 3.4        # hole in the module's top wall for a button that has a pin; wider than the tunnel, so a small error in where
                           # the board sits cannot stop the pin reaching the button
TUNNEL_D = 3.0             # guide tunnel through the frame wall and the TPU body (teardrop in the TPU)
PIN_D = 2.6                # pusher pin shaft: 0.2 mm clear of the tunnel all round, so it slides freely
PIN_HEAD_D = 8.0           # finger pad (A4: Ø6.0 was too small)
PIN_HEAD_T = 1.2
CBORE_D = 8.6              # dimple in the ridge that the pin head sits in
PIN_PROUD = 1.2            # at rest the head stands this far out of the ridge (A4: 0.6 was too sunk in)
PIN_RIB_D = 3.2             # a short band on the shaft, a little wider than the TUNNEL_D tunnel: the soft TPU grips it so the
                           # pin stays put when the case is tipped (A4: loose pins fell out). Too grippy would stick a button
                           # down: COUPON: tune after test print (BTN)
PIN_RIB_W = 1.5
PIN_OVERTRAVEL = 0.5       # how far past 'touching the button' the pin can be pushed before its head bottoms
SD_SLOT = (18.5, 3.6)      # card slot window in the cup wall
MIC_HOLE_D = 2.0           # measured (LyricFlash). MIC results: 1.8 printed clean, so 2.0 is safe; kept
MIC_WALL = 1.6             # measured (LyricFlash): wall thickness at the hole (= COVER_FLOOR, nearly)
MIC_CLEAR_D = 4.0          # larger pass-through holes in plate, frame and body skin
MIC_EXIT_Y = 11.0          # where the sound leaves the back, measured from the centre line.
                           # The mics sit at +/-29.9, behind the solid corners of the triangle, so
                           # the exit holes are moved in to where the hollow is (flat is +/-15.6).
USB_WINDOW = (13.5, 8.0)   # opening for a cable plug (along the edge, through the thickness).
                           # PORT results: Paul's cable needs the +1 tunnel (was 12.5 x 7.0)
                           # COUPON: tune after test print (PORT)
USB_WINDOW_R = 2.5         # corner radius of the opening, to match a plug's rounded grip
                           # COUPON: tune after test print (PORT)
USB_MOUTH_R = 1.0          # round on the outside mouth of each opening (Paul 2026-11-13: keep both, tidy them)

# =============================================================================
# Inner frame (PETG): rigid sleeve inside the TPU body that the module clips into
# =============================================================================
FRAME_WALL = 1.68          # 4 lines at 0.42
FRAME_FLOOR = 1.68
FRAME_CLEAR = 0.25         # module -> frame, per side  # COUPON: tune after test print (SEAT)
FRAME_FIT = 0.00           # frame -> body pocket, per side (0 = snug in TPU; use 0.15 for a rigid PLA body)
                           # COUPON: tune after test print (SEAT)
FRAME_TOP = 1.5            # frame stops this far below the front face; the body's lip hides it
# No flat ceilings in TPU (Paul, 2026-11-13, after the flat lip on SEAT-body sagged into strings).
LIP_LAND = 0.6             # straight, crisp edge of the screen opening before the underside slopes away
                           # COUPON: tune after test print (SEAT)
FRAME_TOP_CHAMFER = 1.2    # 45 degree chamfer on the frame's top outer edge, so it tucks under the sloped lip
ROOF_H = 6.5               # body printed on end: the pocket's top end gets 45 degree sides this high.
                           # A flat strip stays in the middle (see ROOF_FLAT in derive()); a full peak
                           # would need 15 mm, and there are only 9 mm of body beyond the pocket.
                           # COUPON: tune after test print (END-top)
PLATE_CHAMFER = 1.0        # 45 degree chamfer on TPU edges that touch the print plate (coupons)
                           # COUPON: tune after test print
SEAM = 0.30                # visible gap between cream bezel and body, per side
                           # COUPON: tune after test print (SEAT)
PUSH_HOLE_D = 22.0         # finger hole behind the module, to push it out for service
CLIP_X = (-35.0, 35.0)     # clip positions along each long side
CLIP_W = 8.0               # spring finger width
CLIP_LEN = 14.0            # spring finger length  # COUPON: tune after test print (CLIP)
CLIP_BUMP = 0.75           # how far the clip bump bites into the module. CLIP results: 0.60 was weak,
                           # close to no click  # COUPON: tune after test print (CLIP)
CLIP_R = 1.0               # bump radius
CLIP_SLOT = 0.8            # slot around each finger
CLIP_FROM_BACK = 8.0       # bump height above the module's back face

# =============================================================================
# Peak body
# =============================================================================
FRONT_ANGLE = 60.0         # decided 2026-11-11
BACK_ANGLE = 60.0          # read off ref/peak-final-60.png
LEG_T = 13.0               # back leg + bottom, read off the target image (default, Paul to confirm;
                           # the brief's sketch value was ~9)
OUTER_R = 16.0             # outer corner radius (brief)
INNER_R = 12.0             # read off the target image
EDGE_R = 7.0               # soft outer edge on the two open ends (target image)
EDGE_R_IN = 4.0            # soft edge into the opening (target image)
# Decided 2026-11-13: no sharp edges. Every outside edge is a near full round, inside and
# out; only the screen opening stays crisp. (False gives the earlier EDGE_R / EDGE_R_IN rounds.)
ROUND_ALL = True
EDGE_FLAT = 0.4            # flat left between the outer and inner round on a leg's end
BED_END = 1                # which end sits on the print bed: +1 = the portrait end (away from the ports)
BED_CHAMFER = True         # bed end: 45 degree chamfer blended into the round, so nothing overhangs
SMALL_R = 1.0              # round on the mouths of the USB tunnels and the holes in the hollow
BACK_SKIN = 1.68           # body skin behind the frame, facing the hollow
FRONT_MARGIN = 5.0         # body showing above/below the bezel (target image)
SIDE_MARGIN = 11.0         # body left/right of the bezel (target image)

# --- Port-end cap (Paul, 2026-11-13: option A) --------------------------------------------------
# The body prints on end. Its last CAP_T at the port end is a separate TPU cap, so the main body's
# pocket is an open-topped channel with no ceilings. The cap pushes on with pegs: no glue.
CAP = True                 # approved by Paul 2026-11-13 (renders/peak-views.png); False gives the one-piece body
PEG_D = 5.0                # peg diameter
PEG_L = 8.0                # peg length
PEG_FIT = 0.10             # hole is this much larger than the peg on diameter.
                           # Test 2: 0.20 was loose (faces closed, groove and flush fine).
                           # Test 1 (2026-11-13): 0.00 was too tight; it bound, tilted and would not close.
                           # COUPON: tune after test print (CAP-corner + BODY-corner)
PEG_SLOT = 0.2             # (test 2: 0.4 let the cap wobble sideways)
                           # all holes but one are stretched this much towards that one, so a small
                           # error in peg spacing can't jam the cap. Test 1: each peg seated fully alone,
                           # but the pair would not close. 0 = plain round holes.
                           # COUPON: tune after test print (CAP-corner + BODY-corner)
PEG_LEAD = 0.5             # 45 degree lead-in at the mouth of each hole
                           # COUPON: tune after test print (CAP-corner + BODY-corner)
GROOVE = 0.6               # depth of the V-groove at the seam, and of the matching groove at the other end
                           # COUPON: tune after test print (CAP-corner + BODY-corner)
PAD_INSET = 25.0           # landscape pads: distance from each end (32 with the cap, to clear the seam)
PAD_INSET_CAP = 32.0

# --- TPU pads -------------------------------------------------------------------
PAD_D = 12.0               # landscape pads, under the bottom
PAD_D_END = 8.0            # portrait pads, on the end it stands on
PAD_T = 2.5
PAD_RECESS = 1.5           # pads stand PAD_T - PAD_RECESS = 1.0 proud
PAD_FIT = 0.10             # recess is this much larger on diameter  # COUPON: tune after test print (SEAT)

# =============================================================================
# Material settings (Bambu Studio, 0.4 nozzle). Not geometry: these are the
# per-object / modifier settings to type in. See ../COUPON-RESULTS.md.
# =============================================================================
MATERIAL_SLICE_T = 15.0    # thickness of each material test slice
TPU_SETTINGS = {           # COUPON: tune after test print (T1 / T2 / T3)
    "T1": {"walls": 2, "infill_pct": 10, "pattern": "gyroid"},   # softest
    "T2": {"walls": 3, "infill_pct": 20, "pattern": "gyroid"},   # middle
    "T3": {"walls": 5, "infill_pct": 35, "pattern": "gyroid"},   # firmest
}
# Stone PLA ("P1") was dropped after Print 1: the body is TPU for AMS only, no external spools.
BODY_PRINT = {             # slicer settings for the body (Bambu Studio); not geometry
    "orientation": "standing on the portrait end (BED_END), supports only inside the pocket",
    "elephant_foot_compensation_mm": 0.15,      # on; COUPON: tune after test print
    "seam": "painted onto an inside corner of the triangle opening",
    "layer_height_mm": 0.20,
    "layer_height_on_rounds_mm": 0.08,          # variable layer height over the rounded ends
}
SOFT_ZONE = "T1"           # whole body. Decided from Print 1 (2026-11-13): T1 felt right
FIRM_ZONE = None           # Print 1: T1 was stiff enough at the screen wall, so no firm modifier.
                           # Set to "T2"/"T3" only if the SEAT coupon shows the pocket flexing.
                           # COUPON: tune after test print (SEAT)
FIRM_ZONE_MARGIN = 4.0     # firm modifier = frame pocket grown by this much  # COUPON: tune after test print


# =============================================================================
# Derived (call derive() again after changing anything above)
# =============================================================================
def derive():
    g = globals()
    g["MODULE_L"] = GLASS_L + 2 * BEZEL_WALL            # cream face, long side
    g["MODULE_W"] = GLASS_W + 2 * BEZEL_WALL            # cream face, short side
    g["OPEN_L"] = GLASS_L - 2 * LIP_OVERLAP             # opening in the bezel
    g["OPEN_W"] = GLASS_W - 2 * LIP_OVERLAP
    g["OPEN_R"] = max(GLASS_R - LIP_OVERLAP, 0.5)
    g["CAV1_L"] = GLASS_L + 2 * GLASS_CLEAR             # pocket around the glass
    g["CAV1_W"] = GLASS_W + 2 * GLASS_CLEAR
    g["CAV2_L"] = CAV1_L + 2 * LEDGE                    # wider pocket behind the ledge
    g["CAV2_W"] = CAV1_W + 2 * LEDGE
    g["PLATE_Z"] = GLASS_RECESS + GLASS_TO_BOARD + STANDOFF_H      # depth of the plate's front face
    g["BATT_ZONE"] = BATT_GAP + BATT_T + BATT_SWELL
    g["MOD_D"] = PLATE_Z + PLATE_T + BATT_ZONE + COVER_FLOOR       # module depth, face to back
    g["FRAME_L"] = MODULE_L + 2 * (FRAME_CLEAR + FRAME_WALL)
    g["FRAME_W"] = MODULE_W + 2 * (FRAME_CLEAR + FRAME_WALL)
    g["FRAME_R"] = BEZEL_R + FRAME_CLEAR + FRAME_WALL
    g["POCKET_D"] = MOD_D + FRAME_CLEAR + FRAME_FLOOR + FRAME_FIT  # pocket depth in the body
    g["FRONT_T"] = POCKET_D + BACK_SKIN                            # front wall thickness
    g["BODY_WIDTH"] = MODULE_L + 2 * (SEAM + SIDE_MARGIN)
    g["BOARD_X0"] = -GLASS_L / 2 + BOARD_INSET_USB_END             # module x of the board's USB end
    # Button pins: the channel runs up the slope at the button's depth and leaves through the apex
    # arc (radius OUTER_R, starting FRONT_MARGIN above the seam).
    g["BTN_Z"] = -(GLASS_RECESS + GLASS_TO_BOARD + BUTTON_H)         # depth of the button axis (negative)
    g["BTN_TIP_Y"] = -(BOARD_W / 2 + BUTTON_STICKOUT)                # where the button's actuator starts
    g["ARC_START_Y"] = -(MODULE_W / 2 + SEAM + FRONT_MARGIN)         # where the apex arc leaves the front face
    g["BTN_EXIT_Y"] = ARC_START_Y - (OUTER_R ** 2 - (OUTER_R + BTN_Z) ** 2) ** 0.5   # axis meets the ridge
    g["PIN_HEAD_REST_Y"] = BTN_EXIT_Y - PIN_PROUD + PIN_HEAD_T       # head's inner face at rest
    g["PIN_LEN"] = abs(PIN_HEAD_REST_Y) - abs(BTN_TIP_Y)             # shaft length, tip to head
    g["PIN_FLOOR_Y"] = PIN_HEAD_REST_Y + PIN_OVERTRAVEL              # head bottoms here when pushed in
    g["PIN_RIB_Y"] = (-(FRAME_W / 2) + BTN_EXIT_Y) / 2               # middle of the TPU part of the tunnel
    g["LIP_RUN"] = (FRAME_L - MODULE_L) / 2 - SEAM + FRAME_FIT      # how far the lip overhangs the frame
    g["LIP_D"] = LIP_LAND + LIP_RUN                                # depth where the pocket reaches full width
    g["ROOF_FLAT"] = (POCKET_D - LIP_D) - 2 * ROOF_H               # flat strip left at the pocket's top end
    # Seam plane: where the screen opening's corners start to curve, so the main body's opening
    # and pocket are straight all the way to the top when it prints on end.
    g["CAP_T"] = SIDE_MARGIN + SEAM + BEZEL_R                      # cap thickness along the width
    g["SEAM_Y"] = USB_END * (BODY_WIDTH / 2 - CAP_T)               # world Y of the seam (ports side)


derive()


def board_xy(xb, yb):
    """Board-frame point -> module frame (x along the long side, y down the slope).

    Module frame: origin at the centre of the bezel face, +z out of the screen.
    Ports sit at the -x end, buttons and card slot on the -y (upper) edge.
    """
    return (BOARD_X0 + xb, BOARD_W / 2 - yb)


def board_depth(height):
    """Depth below the bezel face of something `height` above the board's component face."""
    return GLASS_RECESS + GLASS_TO_BOARD + height
