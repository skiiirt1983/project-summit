"""Builds the one checklist of tests Paul still has to do, from a single source:

    ../REMAINING-TESTS.md     (in the repo, pictures in ../tests/)
    ../REMAINING-TESTS.docx   (the same thing as a Word file to fill in)

Run:  python test_pictures.py     (3D pictures, needs the CAD)
      python remaining_tests.py   (diagrams + both documents; needs pillow and python-docx)

Edit TESTS below when a test is finished or a new one is needed. Parameter names stay out of
the questions; they live in the "For Claude" table at the end.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import params as p

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
PICS = ROOT / "tests"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
BOX = "☐"

TPU = "TPU for AMS, 2 walls, 10 % gyroid, 0.2 mm layers, line width 0.42, supports off"
PETG = "PETG, 4 walls, 15 % gyroid, 0.2 mm layers, line width 0.42 (all types), Classic walls, supports off"
PETG_FINE = "PETG (cream if you have it), 4 walls, 15 % gyroid, 0.12 mm layers, line width 0.42 (all types), Classic walls, supports off"

GROUPS = {
    "A": "Reprints (pieces changed by your last results)",
    "B": "Tests not done yet",
    "C": "Needs the board (when it arrives)",
    "D": "Measurements (calipers)",
}

# Each test: id, title, picture, why, pieces [(piece, file, note)], settings, steps, questions.
# A question is (text, [tick-box options]); "___" in an option is a blank to write in.
TESTS = [
    dict(id="A1", title="Cap push-fit, third try", pic="A1-cap.png",
         why="The end cap pushes onto the body with pegs, no glue. Last time it was loose and wobbled, "
             "so the holes are now a little smaller. This checks the cap is snug, sits flat and still comes off by hand.",
         pieces=[("BODY-corner", "coupons/reprint-tpu.3mf (or BODY-corner.stl)", "print this one"),
                 ("CAP-corner", "already printed", "reuse it, it has not changed")],
         settings=TPU + ". It prints with the two holes facing up.",
         steps=["Push the two pegs of CAP-corner into the two holes of the new BODY-corner until the faces meet.",
                "Look at the gap all the way round, then try to rock and slide the cap sideways.",
                "Pull it off and push it back on ten times."],
         questions=[("How is the push-fit?", ["falls off", "loose", "right: snug, comes off by hand", "too tight", "won't go in"]),
                    ("Do the faces close up all the way round?", ["yes", "even gap of ___ mm", "uneven gap, ___ to ___ mm"]),
                    ("Does the cap wobble or shift sideways once on?", ["no", "a little", "yes"]),
                    ("Does the small groove at the join still look intended?", ["yes", "too faint", "too heavy"]),
                    ("Do the outside faces line up across the join?", ["flush", "step of ___ mm"]),
                    ("The round pocket with a pointed top on the outside of BODY-corner (for a foot pad): printed clean?", ["yes", "no"])]),
    dict(id="A2", title="Clip click, second try", pic="A2-clip.png",
         why="Four spring clips hold the screen module in the case. Last time the click was weak, close to none, "
             "so the bump on the clip is now bigger. This checks it clicks and holds without the clip breaking.",
         pieces=[("CLIP-frame", "coupons/reprint-petg.3mf (or CLIP-frame.stl)", "print this one"),
                 ("CLIP-bezel", "coupons/reprint-petg.3mf (or CLIP-bezel.stl)", "print this one"),
                 ("CLIP-body", "already printed", "reuse it")],
         settings=PETG + ". CLIP-bezel prints face down.",
         steps=["Push CLIP-frame into CLIP-body.",
                "Slide CLIP-bezel in from the front until it clicks.",
                "Pull it out and push it in ten times, then look at the little spring finger on the frame."],
         questions=[("Click and hold?", ["no click", "weak", "right", "too stiff"]),
                    ("The spring finger after ten pulls?", ["fine", "turned white", "snapped"]),
                    ("Did the finger print free (slots around it open)?", ["yes", "fused"])]),
    dict(id="A3", title="Carrier plate, one more try", pic="A3-snap.png",
         why="The back cover is done: the snap is right, it sits level and it opens. The plate was still a little tight on "
             "its step, so it is now 0.025 mm per side looser. It is screwed to the board anyway, so it only needs to drop in "
             "freely. Only this one piece needs printing again.",
         pieces=[("SNAP-plate", "coupons/reprint-petg.3mf (or SNAP-plate.stl)", "print this one; keep it for test C3"),
                 ("SNAP-bezel, SNAP-cover", "already printed", "reuse them")],
         settings=PETG + ".",
         steps=["Lay SNAP-bezel face down on the table.",
                "Drop the new SNAP-plate in. It should sit on the small step inside the walls.",
                "Press SNAP-cover in behind it, then turn it over and shake it."],
         questions=[("The plate on its step?", ["loose, slides about", "right: drops in, no binding", "tight", "won't go in"]),
                    ("Does the cover still snap and open as before?", ["yes", "no"])]),
    dict(id="A4", title="Button pins, second try", pic="A4-buttons.png",
         why="Last time the pins slid freely and the holes printed clean, but the heads were too small and too sunk in, and the pins fell out "
             "when the strip was tipped. Now: bigger heads that stand out more, a thin grip band on each pin that the soft body holds lightly "
             "(so they stay put but can still be pulled out), and a groove across the POWER head so you can find it by touch. "
             "Two pieces change: the TPU strip (its dimples are bigger) and the three pins. BTN-frame and BTN-cup stay as printed.",
         pieces=[("BTN-body", "coupons/buttons-tpu.3mf (or BTN-body.stl)", "TPU, new: bigger dimples. Prints standing up"),
                 ("BTN-pin-1, -2, -3", "coupons/buttons-petg.3mf (or BTN-pin-N.stl)", "PETG, new. Stand on their heads"),
                 ("BTN-frame, BTN-cup", "already printed", "reuse them")],
         settings="BTN-body: TPU for AMS settings from the table. Pins: 0.12 mm layers, 3 walls, 100 % infill. Everything with line width 0.42, Classic walls.",
         steps=["Stack the strip: BTN-cup (flat side towards the body), then BTN-frame, then the new BTN-body with its curved ridge at the far end. Line up the three pairs of holes.",
                "Push a pin in from the ridge end until it is through all three layers. Do the same for the other two. Notice how hard it is to push in.",
                "Press each pin with a fingertip, then let go.",
                "Hold the strip with the ridge pointing down and shake gently, then lay it on its side.",
                "Pull a pin out again with a fingernail."],
         questions=[("Is each pin easy to push in and press?", ["yes", "a little stiff", "too stiff, it sticks"]),
                    ("Is the head easy to find and press with a fingertip?", ["yes", "too small", "too sunk in", "too proud"]),
                    ("Do the pins stay in when you tip the strip with the ridge down?", ["yes, all stay", "one falls out", "all fall out"]),
                    ("Can you pull a pin out again with a fingernail?", ["yes, easily", "yes, but hard", "no"]),
                    ("Can you tell the POWER pin (BTN-pin-3, grooved head) by touch?", ["yes", "no"]),
                    ("Did the three dimples in BTN-body print clean (round, not blocked)?", ["yes", "no"])]),
    dict(id="A5", title="Glass ring, second try", pic="A5-glass.png",
         why="The first ring did not fit the real screen. The white adhesive you saw is on the back of the glass, facing the circuit board, so "
             "it is not the cause. The likeliest cause is the print: last time the line width was probably different, which changes how thick the "
             "walls come out and how big the pocket is. So this time there are two rings: A is the same design printed with the "
             "settings in the table at the top; B has 0.25 mm more room per side. If A fits, it was the print. If only B fits, the "
             "glass needs more room than the drawing says and I will enlarge the pocket. If neither fits, I need to know where it catches.",
         pieces=[("GLASS-ring", "coupons/petg-plate.3mf (or GLASS-ring.stl)", "PETG, 0.12 mm layers, face down. Same file as before: reprint it with the correct line width"),
                 ("GLASS-ring-B", "coupons/petg-plate.3mf (or GLASS-ring-B.stl)", "PETG, 0.12 mm layers, face down. New: bigger pocket")],
         settings=PETG_FINE + ". Elephant foot compensation 0.15 mm. Prints face down.",
         steps=["Lay a ring face down on a soft cloth. Lower the screen into it, glass down. Never press on the glass.",
                "Do the same with the other ring.",
                "Turn the one that fits over and look at the screen through the opening, switched on if you can."],
         questions=[("Which ring does the screen drop into?", ["A only", "B only", "both", "neither"]),
                    ("In the ring that fits, how much play is there?", ["rattles", "just right", "tight"]),
                    ("If neither fits: where does it catch?", ["corners", "long sides", "short sides", "all round", "not applicable"]),
                    ("Does the lip cover only the black border, not the picture?", ["yes", "covers the picture on the ___ side"]),
                    ("Does the black border look even enough behind the cream?", ["yes", "no"]),
                    ("With the board resting in the ring, is there a gap between lip and glass?", ["touching", "small gap", "large gap"])]),
    dict(id="B1", title="Seat: body, frame and bezel corner", pic="B1-seat.png",
         why="This is one corner of the real case: the soft body, the stiff frame inside it, and the cream bezel. It checks "
             "how the three fit and how the thin line between cream and body looks. Your results document had ticks for this "
             "test, but your message says you have not done it, so it is here to be sure. If the ticks were real, tick "
             "\"same as my results document\" and skip the rest.",
         pieces=[("SEAT-body", "already printed (coupons/coupons-fit-body.3mf)", "TPU"),
                 ("SEAT-frame", "coupons/SEAT-frame.stl", "PETG; print it if you have not"),
                 ("SEAT-bezel", "coupons/SEAT-bezel.stl", "PETG, face down; print it if you have not")],
         settings=PETG + " for the frame and bezel.",
         steps=["Push SEAT-frame into SEAT-body, under the sloped lip.",
                "Push SEAT-bezel into the frame until its face is level with the body.",
                "Look at the line between cream and body, and press on the body wall beside the frame."],
         questions=[("Already done?", ["same as my results document, skip the rest", "no, answers below"]),
                    ("Frame into body?", ["loose", "right", "too hard", "won't go under the lip"]),
                    ("Does the soft body wall bulge or flex where the frame sits?", ["no", "a little", "yes"]),
                    ("Bezel into frame?", ["loose", "right", "tight"]),
                    ("The line between cream and body?", ["good, even", "too wide", "too tight, rubbing"]),
                    ("Bezel face level with the body face?", ["level", "sticks out ___ mm", "sunk ___ mm"])]),
    dict(id="C1", title="Glass ring on the real screen", pic="C1-glass.png",
         why="The ring is the front 6 mm of the cream bezel. It checks the real glass drops in, that the bezel's lip hides "
             "only the black border and not the picture, and that nothing presses on the glass.",
         pieces=[("GLASS-ring", "coupons/GLASS-ring.stl", "print it if you have not")],
         settings=PETG_FINE + ". Prints face down.",
         steps=["Lay GLASS-ring face down on a soft cloth.",
                "Lower the screen into it, glass down. Never press on the glass.",
                "Turn it over, holding both, and look at the screen through the opening with the screen switched on if you can."],
         questions=[("Does the glass drop in?", ["falls in and rattles", "just right", "tight", "won't go"]),
                    ("Does the lip cover only the black border, not the picture?", ["yes", "covers the picture on the ___ side"]),
                    ("Does the black border look even enough behind the cream?", ["yes", "no"]),
                    ("With the board resting in the ring, is there a gap between lip and glass?", ["touching", "small gap", "large gap"])]),
    dict(id="C2", title="Port block on the real ports", pic="C2-port.png",
         why="Your cable fits the +1 tunnel, and that is now the size of the real openings. What is left is to check on the "
             "real board that both tunnels meet both USB-C ports at once, and that the plug goes fully home.",
         pieces=[("PORT-block", "already printed", "reuse it")],
         settings="Nothing to print.",
         steps=["Hold PORT-block against the end of the board where the two USB-C ports are, tunnels over the ports.",
                "Push your cable through the +1 tunnel into each port in turn."],
         questions=[("Do both tunnels line up with both ports at once?", ["yes", "off by ___ mm"]),
                    ("Does the plug go fully home through the +1 tunnel?", ["yes, both ports", "no"])]),
    dict(id="C3", title="Carrier plate on the board", pic="C3-plate.png",
         why="The board screws to the carrier plate through its brass standoffs. This checks the holes and the cut-out in the "
             "plate are where the real board needs them.",
         pieces=[("SNAP-plate", "already printed", "reuse it")],
         settings="Nothing to print.",
         steps=["Lay the board glass down on a soft cloth.",
                "Lay SNAP-plate on the brass standoffs at the end away from the USB-C ports, countersunk side up.",
                "Look down the two screw holes, and at the long pin header poking through the cut-out."],
         questions=[("Do the two screw holes sit over the two standoffs?", ["yes", "off by ___ mm"]),
                    ("Does the plate sit flat on the standoffs?", ["yes", "no, held up by ___"]),
                    ("Does the pin header clear the cut-out?", ["yes", "touches"])]),
    dict(id="C4", title="Read the board's printed labels", pic="D-board.png",
         why="A few things are not in Waveshare's drawing. The picture shows the back of the board with numbers; please read "
             "the tiny white labels next to each.",
         pieces=[], settings="Nothing to print.",
         steps=["Hold the board as in the picture: back towards you, USB-C ports on the left, buttons along the top."],
         questions=[("Buttons 1, 2, 3 (left to right): which is which?", ["1 = ___", "2 = ___", "3 = ___"]),
                    ("Does the board have extra buttons or switches that the drawing does not show?", ["no", "yes: ___"]),
                    ("Which USB-C port is labelled UART?", ["A (upper)", "B (lower)"]),
                    ("Sockets X, Y, Z: which is BAT (battery) and which is SPK (speaker)?", ["BAT = ___", "SPK = ___"]),
                    ("Mics M1 and M2: is there a small hole on top of the metal can?", ["yes", "no, plain top"])]),
    dict(id="C5", title="Buttons on the board, with the new pins", pic="D-board.png",
         why="The first board test passed (the holes line up, every pin clicks, you can switch the board back on through POWER). The pins now have a "
             "grip band, so this just checks they still return when you let go and do not hold a button down.",
         pieces=[("BTN-cup, the new BTN-pin-1, -2, -3", "from test A4", "reuse them")],
         settings="Nothing to print.",
         steps=["Do the same as before: BTN-cup about 3 mm from the buttons with the middle hole over BOOT, a pin through each hole until it touches its button.",
                "Press each pin, let go, and look and listen: does the board react once per press, and only while you press?",
                "Switch the board off, then use the POWER pin to switch it back on."],
         questions=[("Does each pin still click its button?", ["all three", "only ___", "none"]),
                    ("Does each pin come back when you let go (no button held down)?", ["yes", "no, ___ stays down"]),
                    ("Can you still switch the board back on through the POWER pin?", ["yes, short press", "yes, needs a hard press", "no"]),
                    ("When you measured the button distances \"from the USB-C end\", from where exactly?", ["the board's own edge (the green board)", "the tip of the USB-C connector, which sticks out", "not sure"])]),
    dict(id="D1", title="Battery thickness", pic="D-measure.png", needs="battery",
         why="The most important number. It sets how thick the front wall of the case is.",
         pieces=[], settings="",
         steps=["Lay the battery flat. Close the jaws across the thickest point, including the taped circuit board at the end. Just touching: do not squeeze."],
         questions=[("Thickness (T)", ["___ mm"])]),
    dict(id="D2", title="Battery length and width", pic="D-measure.png", needs="battery",
         why="Checks the battery fits its bay (106 x 70 mm now).",
         pieces=[], settings="",
         steps=["Measure the outer edges of the pouch, including the folded seam and the circuit-board end."],
         questions=[("Length (L)", ["___ mm"]), ("Width (W)", ["___ mm"])]),
    dict(id="D3", title="Screen depth to the pin header", pic="D-measure.png", needs="board",
         why="Waveshare's two files disagree (12.8 and 11.6 mm).",
         pieces=[], settings="",
         steps=["Lay the board glass down on a soft cloth. Use the depth rod from the table (the glass front) to the top of the long pin header."],
         questions=[("Depth (H)", ["___ mm"])]),
    dict(id="D4", title="Screen depth to a brass standoff", pic="D-measure.png", needs="board",
         why="Sets the small gap that keeps the bezel's lip off the glass. Should be 11.6 mm.",
         pieces=[], settings="",
         steps=["Same set-up. Depth rod from the table to the top of one of the four brass standoffs."],
         questions=[("Depth (S)", ["___ mm"])]),
    dict(id="D5", title="Battery wires", pic="D-measure.png", needs="battery",
         why="Decides which way round the battery goes and where its wires pass.",
         pieces=[], settings="",
         steps=["Note which edge the wires leave from. Measure from the pouch to the plug."],
         questions=[("Wires leave from", ["a short end", "a long side"]), ("Wire length", ["___ mm"])]),
    dict(id="D6", title="Accelerometer board", pic="D-measure.png", needs="accelerometer",
         why="So its seat can be made the right size; it has to sit rigid and square.",
         pieces=[], settings="",
         steps=["Measure the little board: length, width, total height including the tallest part and any pins, hole size, and the spacing between hole centres."],
         questions=[("Length x width", ["___ x ___ mm"]), ("Height", ["___ mm"]),
                    ("Hole size and spacing", ["hole ___ mm", "spacing ___ mm"])]),
    dict(id="D7", title="Glass outline", pic="D-measure.png", needs="board",
         why="Checks Waveshare's drawing (126.9 x 70.7 mm).",
         pieces=[], settings="",
         steps=["Across the glass: long side, then short side."],
         questions=[("Long side", ["___ mm"]), ("Short side", ["___ mm"])]),
    dict(id="D8", title="Speaker, only if you want one fitted", pic="D-measure.png", needs="speaker",
         why="No room is reserved for a speaker yet. If you want alarms and chimes from the case, I need its size to find it a home.",
         pieces=[], settings="",
         steps=["Measure across the speaker and its total depth."],
         questions=[("Fit a speaker?", ["yes", "no"]), ("Diameter and depth", ["___ mm across", "___ mm deep"])]),
    dict(id="D9", title="The buttons on the real board (the new revision may differ from the drawing)", pic="D-board.png", needs="board",
         why="Waveshare's drawing may not match the newest board. These numbers decide where the three pin tunnels must go.",
         pieces=[], settings="",
         steps=["Board back towards you, as in the picture, buttons along the top. Measure from the USB-C end edge of the board to the centre of each button, then how far each button sticks out beyond the top edge, and how high its centre is above the board."],
         questions=[("Distance from the USB-C end to button 1, 2, 3 (centres)", ["1: ___ mm", "2: ___ mm", "3: ___ mm"]),
                    ("How far each button sticks out beyond the board's top edge", ["___ mm"]),
                    ("Height of the button centre above the board", ["___ mm"]),
                    ("Any extra button, or a button on a different edge? (drawing: three on the top edge)", ["no", "yes: ___"])]),
]

# Finished (kept in the source for reference, not listed): A1 cap, A2 clip, A3 back cover, B1 seat, and with the real
# board (2026-11-20) C1 (replaced by A5), C2, C3, C4, D3, D4, D7, D8, D9.
FINISHED = {"A1", "A2", "A3", "A5", "B1", "C1", "C2", "C3", "C4", "D3", "D4", "D7", "D8", "D9"}
TESTS = [t for t in TESTS if t["id"] not in FINISHED]

# For Claude: what each answer changes. Kept out of the questions on purpose.
FOR_CLAUDE = [
    ("A4", "ease / stiff", "stiff/sticks -> PIN_RIB_D -0.1 (now 3.2) or TUNNEL_D +0.1; too loose later -> PIN_RIB_D +0.1"),
    ("A4", "head", "too small -> PIN_HEAD_D (now 8.0); too sunk/proud -> PIN_PROUD (now 1.2); CBORE_D follows (8.6)"),
    ("A4", "stay in", "fall out -> PIN_RIB_D +0.1, PIN_RIB_W +0.5, or a retaining lip in the dimple"),
    ("A4", "pull out", "hard -> PIN_RIB_D -0.1; no -> PIN_PROUD +0.4"),
    ("A4", "POWER groove", "no -> POWER_BAR deeper or longer"),
    ("A4", "dimples clean", "no -> TUNNEL_D +0.2 / CBORE_D +0.2 (teardrop in the TPU body)"),
    ("C5", "click / return / hard press", "no click -> BTN_TIP_Y or BUTTON_HOLE_D; stuck -> PIN_RIB_D -0.2 (the grip band holds the pin down); POWER no -> PIN_OVERTRAVEL"),
    ("C5", "measured from", "connector tip -> BUTTONS_X shift +0.6 (now +1.26, the midpoint); board edge -> +1.9"),
    ("D1", "T", "BATT_T (drives FRONT_T)"),
    ("D2", "L, W", "BATT_L, BATT_W; decides the speaker (decisions.md)"),
    ("D5", "wires", "battery orientation, WIRE_NOTCH"),
    ("D6", "accelerometer", "ACC_L, ACC_W, ACC_T, seat on the plate"),
]


# ---------------------------------------------------------------------------------------------
# Diagrams
# ---------------------------------------------------------------------------------------------
def _fonts(size):
    return ImageFont.truetype(FONT, size), ImageFont.truetype(BOLD, size)


def board_diagram():
    """Back of the board, to scale, from the Waveshare drawing, with numbered call-outs."""
    s, pad = 8, 70
    w, h = int(p.BOARD_L * s) + 2 * pad, int(p.BOARD_W * s) + 2 * pad
    im = Image.new("RGB", (w, h), "white")
    d = ImageDraw.Draw(im)
    font, bold = _fonts(22)

    def xy(xb, yb):                                   # board frame (y up) -> pixels
        return pad + xb * s, pad + (p.BOARD_W - yb) * s

    d.rectangle([*xy(0, p.BOARD_W), *xy(p.BOARD_L, 0)], fill="#2f6b46", outline="#143524", width=3)
    for i, yb in enumerate(sorted(p.USB_Y, reverse=True)):            # upper first
        x0, y0 = xy(-1.3, yb + 4.8)
        x1, y1 = xy(6.4, yb - 4.8)
        d.rectangle([x0, y0, x1, y1], fill="#c9ccd1", outline="#555", width=2)
        d.text((x0 - 34, (y0 + y1) / 2 - 12), "AB"[i], fill="#b00020", font=bold)
    for i, xb in enumerate(p.BUTTONS_X):
        x0, y0 = xy(xb - 2.3, 65.0)
        x1, y1 = xy(xb + 2.3, 61.3)
        d.rectangle([x0, y0, x1, y1], fill="#e8e8e8", outline="#555", width=2)
        d.text(((x0 + x1) / 2 - 7, y0 - 32), str(i + 1), fill="#b00020", font=bold)
    x0, y0 = xy(p.SD_X[0], 64.5)
    x1, y1 = xy(p.SD_X[1], 49.5)
    d.rectangle([x0, y0, x1, y1], fill="#9aa0a8", outline="#555", width=2)
    d.text((x0 + 6, y0 + 40), "card slot", fill="white", font=font)
    for i, (xb, yb) in enumerate(sorted(p.MICS, key=lambda m: -m[1])):
        cx, cy = xy(xb, yb)
        d.rectangle([cx - 11, cy - 14, cx + 11, cy + 14], fill="#d9c27a", outline="#555", width=2)
        d.text((cx + 18, cy - 12), f"M{i + 1}", fill="#ffd9d9", font=bold)
    x0, y0 = xy(p.HEADER_BOX[0], p.HEADER_BOX[3])
    x1, y1 = xy(p.HEADER_BOX[2], p.HEADER_BOX[1])
    d.rectangle([x0, y0, x1, y1], fill="#222", outline="#000")
    d.text((x0 + 10, y0 - 30), "40-pin header", fill="white", font=font)
    sockets = {"X": (30.85, 1.02, 38.55, 6.22), "Y": p.J5_BOX, "Z": (112.1, 13.4, 117.1, 16.8)}
    for name, (ax, ay, bx, by) in sockets.items():
        x0, y0 = xy(ax, by)
        x1, y1 = xy(bx, ay)
        d.rectangle([x0, y0, x1, y1], fill="#f2efe6", outline="#555", width=2)
        ty = y0 - 30 if name == "X" else y1 + 6
        d.text(((x0 + x1) / 2 - 8, ty), name, fill="#ffd9d9", font=bold)
    for xb, yb in p.STANDOFFS:
        cx, cy = xy(xb, yb)
        d.ellipse([cx - 20, cy - 20, cx + 20, cy + 20], fill="#c8a24a", outline="#5a4410", width=2)
    d.text((pad, 14), "Back of the board (parts facing you). USB-C ports on the left, buttons along the top.",
           fill="#222", font=font)
    d.text((pad, h - 44), "Gold circles = brass standoffs.  Positions from the Waveshare drawing.", fill="#555", font=font)
    im.save(PICS / "D-board.png")


def measure_diagram():
    """Where the calipers go: battery (T, L, W) and the screen stack (H, S)."""
    im = Image.new("RGB", (1280, 560), "white")
    d = ImageDraw.Draw(im)
    font, bold = _fonts(22)

    def arrow(x0, y0, x1, y1, label, lx, ly):
        d.line([x0, y0, x1, y1], fill="#b00020", width=3)
        for (ax, ay), (bx, by) in (((x0, y0), (x1, y1)), ((x1, y1), (x0, y0))):
            dx, dy = (bx - ax), (by - ay)
            n = (dx * dx + dy * dy) ** 0.5
            ux, uy = dx / n, dy / n
            d.polygon([(ax, ay), (ax + 12 * ux - 6 * uy, ay + 12 * uy + 6 * ux),
                       (ax + 12 * ux + 6 * uy, ay + 12 * uy - 6 * ux)], fill="#b00020")
        d.text((lx, ly), label, fill="#b00020", font=bold)

    # Battery, seen from above and from the side.
    d.text((40, 20), "Battery", fill="#222", font=bold)
    d.rounded_rectangle([60, 80, 480, 320], radius=14, fill="#3b6ea8", outline="#1d3a5c", width=3)
    d.rectangle([440, 100, 480, 300], fill="#d9b04a", outline="#5a4410", width=2)
    d.line([480, 180, 560, 180], fill="#c22", width=5)
    d.line([480, 210, 560, 210], fill="#222", width=5)
    d.text((300, 170), "taped circuit\nboard end  >", fill="white", font=font)
    arrow(60, 350, 480, 350, "L", 262, 356)
    arrow(30, 80, 30, 320, "W", 2, 186)
    d.rounded_rectangle([60, 430, 480, 490], radius=10, fill="#3b6ea8", outline="#1d3a5c", width=3)
    arrow(510, 430, 510, 490, "T", 530, 446)

    # Screen stack, glass down on the table.
    d.text((700, 20), "Board, glass down on a soft cloth (side view)", fill="#222", font=bold)
    base = 470
    d.line([680, base, 1250, base], fill="#888", width=3)
    d.rectangle([700, base - 14, 1230, base], fill="#111")                          # glass
    d.rectangle([720, base - 60, 1205, base - 14], fill="#7d8791")                  # display
    d.rectangle([720, base - 100, 1205, base - 86], fill="#2f6b46")                 # board
    d.rectangle([760, base - 86, 1160, base - 60], fill="#d8d8d8")                  # foam
    d.rectangle([735, base - 150, 765, base - 100], fill="#c8a24a", outline="#5a4410")   # standoff
    d.rectangle([1150, base - 150, 1180, base - 100], fill="#c8a24a", outline="#5a4410")
    d.rectangle([900, base - 168, 1120, base - 100], fill="#222")                   # header
    d.text((905, base - 205), "pin header", fill="#222", font=font)
    d.text((690, base - 185), "standoff", fill="#222", font=font)
    arrow(1010, base, 1010, base - 168, "H", 1020, base - 60)
    arrow(750, base, 750, base - 150, "S", 700, base - 60)
    d.text((700, base + 16), "table = front of the glass", fill="#555", font=font)
    im.save(PICS / "D-measure.png")


# ---------------------------------------------------------------------------------------------
# Writers
# ---------------------------------------------------------------------------------------------
def summary():
    counts = {g: sum(1 for t in TESTS if t["id"][0] == g) for g in GROUPS}
    return "Left to do: " + " · ".join(f"{g}. {GROUPS[g].split(' (')[0]}: {n}" for g, n in counts.items())


PRINT_SETTINGS = [
    ("BTN-pin-1, -2, -3 (PETG)", "0.4 mm nozzle. 0.12 mm layers. Line width 0.42 mm for every line type. Wall generator Classic. 3 walls, 100 % infill (they are only 2.6 mm thick). "
                                 "Supports off. They stand on their heads. Elephant foot compensation 0.15 mm."),
    ("BTN-body (TPU for AMS)", "TPU for AMS profile speeds. 0.2 mm layers. Line width 0.42 mm. 2 walls, 10 % gyroid. Supports off. Elephant foot compensation 0.15 mm. Filament dried. Prints standing up."),
    ("Where the settings are", "Bambu Studio: Process, tab Quality, 'Line width'; tab Strength, 'Wall generator'. Set both before slicing. "
                               "Classic keeps every wall exactly 0.42 mm; the other generator (Arachne) squeezes wall widths to fit thin features, "
                               "which changes how a lip or a pocket comes out."),
]

INTRO = ("Only what is still open. Finished tests are not here (the record of those is COUPON-RESULTS.md). "
         "Tick a box or write in a blank. Never press on the screen glass.")
GROUP_NOTES = {
    "A": "Two things to print: the TPU strip (coupons/buttons-tpu.3mf) and the three pins (coupons/buttons-petg.3mf). Glass ring, cap, clip, seat and back-cover tests are finished.",
    "B": "Nothing left here. The seat test is finished, and the T1/T2/T3 material test was answered long ago (TPU for AMS, T1, no firm zone, 13 mm legs right).",
    "C": "Needs the new pins from A4 and the real board. The plate, ports, labels and the first button test are done.",
    "D": "Calipers, in mm, one decimal. D1, D2 and D5 need the battery, D6 the accelerometer.",
}


def write_markdown():
    out = ["# Remaining tests", "", f"**{summary()}**", "", INTRO, "",
           "## Print settings (use these for every remaining piece)", "", "| Piece | Settings |", "|---|---|"]
    out += [f"| {a} | {b} |" for a, b in PRINT_SETTINGS] + [""]
    for g, name in GROUPS.items():
        out += [f"## {g}. {name}", "", GROUP_NOTES[g], ""]
        shown = set()
        for t in (t for t in TESTS if t["id"][0] == g):
            out += [f"### {t['id']}. {t['title']}", ""]
            if t["pic"] not in shown or g != "D":
                out += [f"![{t['id']}](tests/{t['pic']})", ""]
                shown.add(t["pic"])
            out += [f"**What you're testing and why:** {t['why']}", ""]
            if t["pieces"]:
                out += ["| Piece | Where it is | Note |", "|---|---|---|"]
                out += [f"| **{a}** | {b} | {c} |" for a, b, c in t["pieces"]] + [""]
            if t["settings"]:
                out += [f"**Print settings:** {t['settings']}", ""]
            out += ["**What to do:**", ""] + [f"{i}. {s}" for i, s in enumerate(t["steps"], 1)] + [""]
            out += ["| Question | Answer |", "|---|---|"]
            out += [f"| {q} | {'  '.join(BOX + ' ' + o for o in opts)} |" for q, opts in t["questions"]] + [""]
    out += ["---", "", "<details><summary>For Claude: what each answer changes (ignore this)</summary>", "",
            "| Test | Answer | Changes in cad/params.py |", "|---|---|---|"]
    out += [f"| {a} | {b} | `{c}` |" for a, b, c in FOR_CLAUDE] + ["", "</details>", ""]
    (ROOT / "REMAINING-TESTS.md").write_text("\n".join(out))


def write_docx():
    from docx import Document
    from docx.enum.text import WD_BREAK
    from docx.shared import Inches, Pt, RGBColor

    doc = Document()
    for section in doc.sections:
        section.left_margin = section.right_margin = Inches(0.8)
        section.top_margin = section.bottom_margin = Inches(0.7)
    doc.styles["Normal"].font.size = Pt(10.5)
    doc.add_heading("SLOPEFORM Home case: remaining tests", 0)
    doc.add_paragraph().add_run(summary()).bold = True
    doc.add_paragraph(INTRO)
    doc.add_heading("Print settings (use these for every remaining piece)", 1)
    table = doc.add_table(rows=1, cols=2)
    table.style = "Light Grid Accent 1"
    table.rows[0].cells[0].text, table.rows[0].cells[1].text = "Piece", "Settings"
    for a_, b_ in PRINT_SETTINGS:
        cells = table.add_row().cells
        cells[0].text, cells[1].text = a_, b_
    doc.add_paragraph()
    for g, name in GROUPS.items():
        if g != "A":
            doc.add_paragraph().add_run().add_break(WD_BREAK.PAGE)
        doc.add_heading(f"{g}. {name}", 1)
        doc.add_paragraph(GROUP_NOTES[g])
        shown = set()
        for index, t in enumerate(t for t in TESTS if t["id"][0] == g):
            if g != "D" and index:                              # one test per page, picture and boxes together
                doc.add_paragraph().add_run().add_break(WD_BREAK.PAGE)
            doc.add_heading(f"{t['id']}. {t['title']}", 2)
            if t["pic"] not in shown or g != "D":
                doc.add_picture(str(PICS / t["pic"]), width=Inches(5.2 if g != "D" else 6.0))
                shown.add(t["pic"])
            para = doc.add_paragraph()
            para.add_run("What you're testing and why: ").bold = True
            para.add_run(t["why"])
            if t["pieces"]:
                table = doc.add_table(rows=1, cols=3)
                table.style = "Light Grid Accent 1"
                for cell, text in zip(table.rows[0].cells, ("Piece", "Where it is", "Note")):
                    cell.text = text
                for row in t["pieces"]:
                    for cell, text in zip(table.add_row().cells, row):
                        cell.text = text
            if t["settings"]:
                para = doc.add_paragraph()
                para.add_run("Print settings: ").bold = True
                para.add_run(t["settings"])
            doc.add_paragraph().add_run("What to do:").bold = True
            for number, step in enumerate(t["steps"], 1):      # typed numbers: Word's own list would
                doc.add_paragraph(f"{number}. {step}")          # keep counting from the previous test
            table = doc.add_table(rows=1, cols=2)
            table.style = "Light Grid Accent 1"
            table.rows[0].cells[0].text, table.rows[0].cells[1].text = "Question", "Answer (tick or fill in)"
            for q, opts in t["questions"]:
                cells = table.add_row().cells
                cells[0].text = q
                cells[1].text = "    ".join(f"{BOX} {o}" for o in opts)
            doc.add_paragraph()
    doc.add_paragraph().add_run().add_break(WD_BREAK.PAGE)
    doc.add_heading("For Claude only (you can ignore this page)", 2)
    table = doc.add_table(rows=1, cols=3)
    for cell, text in zip(table.rows[0].cells, ("Test", "Answer", "Changes")):
        cell.text = text
    for row in FOR_CLAUDE:
        for cell, text in zip(table.add_row().cells, row):
            cell.text = text
    for row in table.rows:
        for cell in row.cells:
            for para in cell.paragraphs:
                for run in para.runs:
                    run.font.size = Pt(7.5)
                    run.font.color.rgb = RGBColor(0x88, 0x88, 0x88)
    doc.save(ROOT / "REMAINING-TESTS.docx")


if __name__ == "__main__":
    PICS.mkdir(exist_ok=True)
    board_diagram()
    measure_diagram()
    write_markdown()
    write_docx()
    print(summary())
