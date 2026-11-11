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

# --- Battery (8000 mAh LiPo) ----------------------------------------------
BATT_L = 110.0             # PLACEHOLDER — measure
BATT_W = 60.0              # PLACEHOLDER — measure
BATT_T = 12.0              # PLACEHOLDER — measure
BATT_SWELL = 1.0           # brief §4: never clamp the pouch
BATT_BOARD_GAP = 2.0       # ARCHITECTURE.md: air gap battery <-> board

# --- Bezel (to be copied from LyricFlash) ---------------------------------
BEZEL_W = 3.0              # PLACEHOLDER — measure (lyricflash)
BEZEL_R = GLASS_R + BEZEL_W
GLASS_RECESS = 0.0         # PLACEHOLDER — measure (lyricflash screen depth)
SEAM = 0.3                 # gap between module and body, per side (coupon)

# --- Peak body ---------------------------------------------------------------
FRONT_ANGLE = 60.0         # decided 2026-11-11
BACK_ANGLE = 60.0          # PLACEHOLDER — take from ref/peak-final-60.png
LEG_T = 9.0                # back leg + bottom thickness (sketch value)
OUTER_R = 16.0             # outer corner radius (sketch value)
INNER_R = 8.0              # PLACEHOLDER — take from ref/peak-final-60.png
EDGE_R = 3.0               # PLACEHOLDER — soft edge on the two open ends
BACK_WALL = 2.1            # wall behind the battery (5 lines at 0.42)
FRONT_MARGIN = 5.0         # PLACEHOLDER — body showing above/below the bezel
SIDE_MARGIN = 5.0          # PLACEHOLDER — body showing left/right of the bezel

# --- Derived -------------------------------------------------------------------
MODULE_L = GLASS_L + 2 * BEZEL_W          # module face, long side
MODULE_W = GLASS_W + 2 * BEZEL_W          # module face, short side
FRONT_T = (MODULE_DEPTH + BATT_BOARD_GAP + BATT_T + BATT_SWELL + BACK_WALL)
BODY_WIDTH = MODULE_L + 2 * (SEAM + SIDE_MARGIN)
