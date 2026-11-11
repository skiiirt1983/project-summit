"""Every dimension and clearance for the SLOPEFORM Home enclosure (mm, degrees).

Values with a source tag come from ../dimensions.md. Anything tagged
PLACEHOLDER is a guess for early renders only and must be measured or decided
before parts are printed.
"""

# --- Screen module: Waveshare ESP32-P4-WIFI6-Touch-LCD-5 ------------------
GLASS_L = 126.9            # drawing
GLASS_W = 70.7             # drawing
GLASS_R = 3.0              # drawing
VIEW_L = 110.32            # drawing (visible screen area)
VIEW_W = 62.28             # drawing
BOARD_L = 118.5            # drawing
BOARD_W = 64.5             # drawing
MODULE_DEPTH = 12.8        # STEP, glass front -> top of 40-pin header.
                           # Drawing says 11.6.  # PLACEHOLDER — measure

# USB-C ports, on one short end of the board (dimensions.md)
USB_OFFSETS = (-8.99, 8.96)   # STEP: port centres from the glass centre line, along the short side
USB_END = -1                  # which end of the body the ports face: -1 = right, seen from the front
                              # (puts buttons + card slot on the top edge). Paul to confirm.
USB_DEPTH = 9.2               # STEP: glass front -> port centre
USB_WINDOW = (12.5, 7.0)      # PLACEHOLDER — opening for a cable plug; test on a port coupon

# --- Battery (8000 mAh LiPo) ----------------------------------------------
BATT_L = 110.0             # PLACEHOLDER — measure
BATT_W = 60.0              # PLACEHOLDER — measure
BATT_T = 12.0              # PLACEHOLDER — measure
BATT_SWELL = 1.0           # brief §4: never clamp the pouch
BATT_BOARD_GAP = 2.0       # ARCHITECTURE.md: air gap battery <-> board

# --- Bezel, copied from LyricFlash (dimensions.md) ------------------------------
LIP_T = 1.0                # lyricflash: thickness of the lip over the glass
LIP_OVERLAP = 1.35         # lyricflash: how far the lip covers the glass edge
LIP_GAP = 0.25             # lyricflash: air between lip and glass
GLASS_RECESS = LIP_T + LIP_GAP   # screen depth below the bezel face
BEZEL_WALL = 1.6           # lyricflash: cream wall outside the glass edge.
                           # Visible cream = BEZEL_WALL + LIP_OVERLAP = 2.95 (as LyricFlash)
BEZEL_R = 6.7              # lyricflash: outer corner radius. Paul to confirm scaling (§8.5)
MIC_HOLE_D = 2.0           # lyricflash: straight hole, no chamfer
MIC_WALL = 1.6             # lyricflash: wall thickness at the hole
SEAM = 0.3                 # gap between module and body, per side (coupon)

# --- Peak body ---------------------------------------------------------------
FRONT_ANGLE = 60.0         # decided 2026-11-11
BACK_ANGLE = 60.0          # read off ref/peak-final-60.png (side view)
LEG_T = 13.0               # back leg + bottom. Read off ref/peak-final-60.png;
                           # the brief's sketch value was ~9. Paul to confirm.
OUTER_R = 16.0             # outer corner radius (sketch value)
INNER_R = 12.0             # read off ref/peak-final-60.png
EDGE_R = 7.0               # soft outer edge on the two open ends (from ref image)
EDGE_R_IN = 4.0            # soft edge into the opening (from ref image)
BACK_WALL = 2.1            # wall behind the battery (5 lines at 0.42)
FRONT_MARGIN = 5.0         # PLACEHOLDER — body showing above/below the bezel
SIDE_MARGIN = 11.0         # body left/right of the bezel; gives ~156 wide as in ref

# --- Derived -------------------------------------------------------------------
def derive():
    """Recompute values that follow from the ones above (call again after changing any)."""
    g = globals()
    g["MODULE_L"] = GLASS_L + 2 * BEZEL_WALL          # module face, long side
    g["MODULE_W"] = GLASS_W + 2 * BEZEL_WALL          # module face, short side
    g["OPEN_L"] = GLASS_L - 2 * LIP_OVERLAP           # opening in the bezel
    g["OPEN_W"] = GLASS_W - 2 * LIP_OVERLAP
    g["OPEN_R"] = max(GLASS_R - LIP_OVERLAP, 0.5)
    g["FRONT_T"] = (GLASS_RECESS + MODULE_DEPTH + BATT_BOARD_GAP + BATT_T
                    + BATT_SWELL + BACK_WALL)
    g["BODY_WIDTH"] = MODULE_L + 2 * (SEAM + SIDE_MARGIN)


derive()
