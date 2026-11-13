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
GLASS_TO_BOARD = 7.6       # drawing (STEP): glass front -> board component face
STANDOFF_H = 4.0           # drawing: M2.5 threaded standoffs above the board
STANDOFFS = ((3.25, 3.75), (115.25, 3.75), (3.25, 60.75), (115.25, 60.75))  # drawing
MODULE_DEPTH = 12.8        # drawing (STEP): glass front -> top of 40-pin header.
                           # The 2D drawing says 11.6; the larger one is used.  # PLACEHOLDER — measure
HEADER_BOX = (54.55, 1.12, 105.85, 7.72)   # drawing (STEP): 40-pin header, 5.2 tall
J5_BOX = (26.31, 57.93, 32.07, 62.78)      # drawing (STEP): 2-pin socket, 4.3 tall
MICS = ((14.55, 2.37), (14.55, 62.17))     # drawing (STEP): mic centres; they face the back
BUTTONS_X = (72.76, 84.44, 96.12)          # drawing: three side-push buttons on the y = 64.5 edge
BUTTON_H = 0.7             # drawing (STEP): button centre above the board
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
BEZEL_WALL = 3.15          # cream wall outside the glass edge. Visible cream = BEZEL_WALL +
                           # LIP_OVERLAP = 4.5 (option B; default, Paul to confirm. LyricFlash is 2.95)
BEZEL_R = 8.25             # outer corner radius (LyricFlash 6.7 + the extra bezel width)
GLASS_CLEAR = 0.30         # gap glass edge -> pocket wall, per side  # COUPON: tune after test print (GLASS RING)

# =============================================================================
# Screen module: cream cup (bezel + walls), carrier plate, back cover
# =============================================================================
LEDGE = 1.0                # step in the cup wall that the carrier plate seats against
PLATE_T = 2.0              # carrier plate; screws to the board's four M2.5 standoffs
PLATE_CLEAR = 0.20         # plate -> cup wall, per side  # COUPON: tune after test print (SEAT)
SCREW_D = 2.9              # M2.5 clearance hole
SCREW_HEAD_D = 5.2         # M2.5 countersunk head
COVER_FLOOR = 1.68         # back cover floor (4 lines at 0.42)
COVER_RIM = 1.26           # back cover rim (3 lines)
COVER_CLEAR = 0.20         # back cover -> cup wall, per side  # COUPON: tune after test print (SEAT)
SNAP_BUMP = 0.40           # back-cover snap ridge height  # COUPON: tune after test print (SEAT)
SNAP_LEN = 28.0            # back-cover snap ridge length
WIRE_NOTCH = (14.0, 6.0)   # PLACEHOLDER — battery + accelerometer wires through the plate
BUTTON_PIN_D = 1.8         # paperclip hole over each button (default, Paul to confirm)
SD_SLOT = (18.5, 3.6)      # card slot window in the cup wall
MIC_HOLE_D = 2.0           # measured (LyricFlash)  # COUPON: tune after test print (MIC)
MIC_WALL = 1.6             # measured (LyricFlash): wall thickness at the hole (= COVER_FLOOR, nearly)
MIC_CLEAR_D = 4.0          # larger pass-through holes in plate, frame and body skin
MIC_EXIT_Y = 11.0          # where the sound leaves the back, measured from the centre line.
                           # The mics sit at +/-29.9, behind the solid corners of the triangle, so
                           # the exit holes are moved in to where the hollow is (flat is +/-15.6).
USB_WINDOW = (12.5, 7.0)   # opening for a cable plug (along the edge, through the thickness)
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
SEAM = 0.30                # visible gap between cream bezel and body, per side
                           # COUPON: tune after test print (SEAT)
PUSH_HOLE_D = 22.0         # finger hole behind the module, to push it out for service
CLIP_X = (-35.0, 35.0)     # clip positions along each long side
CLIP_W = 8.0               # spring finger width
CLIP_LEN = 14.0            # spring finger length  # COUPON: tune after test print (CLIP)
CLIP_BUMP = 0.60           # how far the clip bump bites into the module  # COUPON: tune after test print (CLIP)
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
