"""Test coupons, cut from the real model so they test the real fits.

Run:  python test_coupons.py          -> the pieces still to print:
     (nothing: all pieces that need no board are printed and finished)
      python test_coupons.py --all    -> also rebuilds the pieces already issued (material slices,
     SEAT, END-bed, CAP-corner, GLASS-ring, MIC-plate, PORT-block). Only after a design change:
     the files in ../coupons/ are the ones Paul printed.
One STL per piece is written next to the 3MF. Every piece lies in its print orientation.
The 3MF carries shapes and names only; slicer settings are in ../REMAINING-TESTS.md.
"""
import math
from pathlib import Path

from build123d import (Box, Cylinder, Face, Mesher, Pos, Rot, Text, chamfer, export_stl,
                       extrude, offset)

import params as p
import parts
import peak

OUT = Path(__file__).resolve().parent.parent / "coupons"      # committed: print files
SCENES = peak.OUT / "coupons"                                    # scratch: render inputs
ENGRAVE = 0.6


def engrave_top(part, text, size, at=None, angle=0):
    """Sink `text` into the top face."""
    bb = part.bounding_box()
    x, y = at if at else (bb.center().X, bb.center().Y)
    letters = Rot(0, 0, angle) * extrude(Text(text, size), ENGRAVE + 0.1)
    return part - Pos(x, y, bb.max.Z - ENGRAVE) * letters


def lay_flat(part, flip=False):
    """Put the part on the bed (z = 0), optionally upside down."""
    if flip:
        part = Rot(180, 0, 0) * part
    bb = part.bounding_box()
    return Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * part


def plate_chamfer(part):
    """45 degree chamfer on the edges that touch the print plate (TPU parts).

    Works loop by loop: the outline first, then each opening. A loop that can't take the
    chamfer (too thin a wall next to it) is left as it is and reported.
    """
    def bottom():
        faces = [f for f in part.faces() if f.bounding_box().max.Z < 1e-4]
        return max(faces, key=lambda f: f.area)

    count = 1 + len(bottom().inner_wires())
    skipped = 0
    for i in reversed(range(count)):                      # openings first, outline last
        face = bottom()
        wires = [face.outer_wire()] + sorted(face.inner_wires(), key=lambda w: -w.length)
        if i >= len(wires):
            continue
        done = None
        # Whole loop first; if that fails, only its long edges (short ones sit in pockets and slots).
        for edges in (wires[i].edges(), [e for e in wires[i].edges() if e.length > 6]):
            try:
                trial = chamfer(edges, p.PLATE_CHAMFER)
                if trial.is_valid and len(trial.solids()) == 1:
                    done = trial
                    break
            except Exception:
                continue
        if done is None and i == 0:
            # Last resort for the outline: trim with a block whose foot tapers in at 45 degrees.
            c = p.PLATE_CHAMFER
            foot = extrude(offset(Face(face.outer_wire()), -c), c, taper=-45)
            trial = part & (foot + Pos(0, 0, c) * extrude(Face(face.outer_wire()), 400))
            if trial.is_valid and len(trial.solids()) == 1 and trial.volume < part.volume:
                done = trial
        if done is not None:
            part = done
            continue
        skipped += 1
    if skipped:
        print(f"   plate chamfer: {skipped} of {count} loops left square")
    return part


def end_slices():
    """END-bed: the real body's first 12 mm off the plate, in its real print orientation
    (standing on the portrait end): the 45 degree chamfer blended into the round, pad recesses.
    (END-top was withdrawn on 2026-11-13: that end is now a separate cap.)
    """
    upright = lay_flat(Rot(-90, 0, 0) * peak.body())        # +Y (portrait end) down on the plate
    return {"END-bed": lay_flat(upright & (Pos(0, 0, 6) * Box(400, 400, 12)))}


def cap_raw():
    """CAP-corner and BODY-corner as they sit in the real case (world frame)."""
    spots = peak.peg_spots()
    main, cap = peak.split(peak.body(), [spots[1], spots[2]] + [q for i, q in enumerate(spots) if i not in (1, 2)])
    corner = Pos(114, 0, 23) * Box(72, 400, 50)             # profile X 78..150, Z -2..48
    below = Pos(0, p.SEAM_Y - p.USB_END * 10, 0) * Box(600, 20, 600)
    return {"CAP-corner": cap & corner, "BODY-corner": main & corner & below}


def cap_corner():
    """The back-bottom corner of the port-end cap and of the body under it: two pegs, the seam
    groove, and both parts in the way they will really print.

    CAP-corner prints with the port end face on the plate, pegs up.
    BODY-corner is the 20 mm of body below the seam, seam face up with its two peg holes.
    """
    raw = cap_raw()
    return {"CAP-corner": lay_flat(Rot(-p.USB_END * 90, 0, 0) * raw["CAP-corner"]),
            "BODY-corner": plate_chamfer(lay_flat(Rot(p.USB_END * 90, 0, 0) * raw["BODY-corner"]))}


def material_slice(label):
    """A short slice of the real triangle outline, lying on its side."""
    solid = Rot(-90, 0, 0) * peak.shell(width=p.MATERIAL_SLICE_T, soft_edges=False)   # profile flat on the bed
    corner = solid.bounding_box().min
    solid = lay_flat(solid)
    # Label in the middle of the thick front wall, running up the slope.
    fa = math.radians(p.FRONT_ANGLE)
    _, _, centre = peak.profile()
    x = centre[0] + math.sin(fa) * p.FRONT_T / 2 - corner.X
    y = centre[1] - math.cos(fa) * p.FRONT_T / 2 - corner.Y
    return engrave_top(solid, label, 14, at=(x, y), angle=p.FRONT_ANGLE)


def body_local():
    """The body, moved into the module frame so coupons can be cut square."""
    # Square ends: no round may touch the plate (it sagged in TPU); plate_chamfer() adds the 45 degree edge.
    world = peak.shell(soft_edges=False) - peak.face_plane().location * parts.body_cutter()
    return peak.face_plane().location.inverse() * world


def seat_raw():
    """One corner of body + frame + bezel as they sit together (module frame): the end away
    from the ports, lower side."""
    x0, y0 = p.GLASS_L / 2 - 26, p.GLASS_W / 2 - 22
    depth = p.POCKET_D + p.BACK_SKIN + 2
    box = Pos(x0 + 30, y0 + 30, -depth / 2 + 1) * Box(60, 60, depth)
    body_box = Pos(x0 + 22, y0 + 18.5, -depth / 2 + 1) * Box(44, 37, depth)   # trims the rounded outside
    return {"SEAT-body": body_local() & body_box, "SEAT-frame": parts.frame() & box,
            "SEAT-bezel": parts.cup() & box}


def seat_coupons():
    raw = seat_raw()
    return {"SEAT-body": plate_chamfer(lay_flat(raw["SEAT-body"])),
            "SEAT-frame": lay_flat(raw["SEAT-frame"]),
            "SEAT-bezel": lay_flat(raw["SEAT-bezel"], flip=True)}


def clip_raw():
    """A straight piece of the lower long side around one clip (module frame)."""
    x = p.CLIP_X[1]
    depth = p.POCKET_D + p.BACK_SKIN + 2
    y_in = p.GLASS_W / 2 - 6
    box = Pos(x, y_in + 15, -depth / 2 + 1) * Box(30, 30, depth)
    body_box = Pos(x, y_in + 9, -depth / 2 + 1) * Box(30, 18, depth)
    return {"CLIP-body": body_local() & body_box, "CLIP-frame": parts.frame() & box,
            "CLIP-bezel": parts.cup() & box}


def clip_coupons():
    raw = clip_raw()
    return {"CLIP-body": lay_flat(raw["CLIP-body"]),
            "CLIP-frame": lay_flat(raw["CLIP-frame"]),
            "CLIP-bezel": lay_flat(raw["CLIP-bezel"], flip=True)}


def snap_raw():
    """The whole end of the screen module away from the ports, full width: bezel, carrier plate
    and back cover (module frame). Full width, because the cover is held between the two long
    walls; a corner piece cannot hold it."""
    x_from = 22.0
    depth = p.MOD_D + 2
    box = Pos(x_from + 40, 0, -depth / 2 + 1) * Box(80, 200, depth)
    return {"SNAP-bezel": parts.cup() & box, "SNAP-plate": parts.plate() & box,
            "SNAP-cover": parts.cover(test_slot_x=x_from + 22.0) & box}


def snap_coupons():
    raw = snap_raw()
    return {"SNAP-bezel": lay_flat(raw["SNAP-bezel"], flip=True),
            "SNAP-plate": lay_flat(raw["SNAP-plate"]),
            "SNAP-cover": lay_flat(raw["SNAP-cover"])}


def glass_ring():
    """The front 6 mm of the bezel, all the way round: checks the glass fit and the lip."""
    ring = parts.cup() & (Pos(0, 0, -3) * Box(400, 400, 6))
    return lay_flat(ring, flip=True)                         # prints face down, like the real bezel


def mic_plate():
    """Three holes through the LyricFlash wall thickness: 0.2 under, nominal, 0.2 over."""
    plate = Pos(0, 0, p.MIC_WALL / 2) * Box(42, 16, p.MIC_WALL)
    sizes = (p.MIC_HOLE_D - 0.2, p.MIC_HOLE_D, p.MIC_HOLE_D + 0.2)
    for i, d in enumerate(sizes):
        x = -13 + 13 * i
        plate -= Pos(x, 3, p.MIC_WALL / 2) * Cylinder(d / 2, p.MIC_WALL + 1)
        plate -= Pos(x, -3.5, p.MIC_WALL - 0.4) * extrude(Text(f"{d:.1f}", 4.5), 0.5)
    return plate


PORT_BLOCK_BASE = (12.5, 7.0)     # the block's "+0" tunnel. Fixed, so the printed block stays valid:
                                  # "+1" (13.5 x 8.0) is the size Paul's cable needed and is now USB_WINDOW.


def port_block():
    """The full tunnel a USB-C plug passes through, at the real port spacing.

    "+0" = 12.5 x 7.0, "+1" = 13.5 x 8.0 (now the real opening).
    Hold it against the board's USB end and push the cables through.
    """
    tunnel = p.SIDE_MARGIN + p.SEAM + p.BEZEL_WALL - p.GLASS_CLEAR
    spacing = abs(p.USB_Y[1] - p.USB_Y[0])
    height = PORT_BLOCK_BASE[1] + 6
    block = Pos(0, 0, height / 2) * Box(spacing + PORT_BLOCK_BASE[0] + 12, tunnel, height)
    for i, extra in enumerate((0.0, 1.0)):
        x = (i - 0.5) * spacing
        block -= Pos(x, 0, height / 2) * Rot(0, 0, 90) * parts.usb_tunnel(tunnel + 1, size=(
            PORT_BLOCK_BASE[0] + extra, PORT_BLOCK_BASE[1] + extra))
    block -= Pos(-spacing / 2, 0, height - 0.4) * extrude(Text("+0", 4), 0.5)
    block -= Pos(spacing / 2, 0, height - 0.4) * extrude(Text("+1", 4), 0.5)
    return block


def arrange(named, gap=6.0, max_width=240.0):
    """Lay parts out in rows on the bed."""
    placed, x, y, row_h = {}, 0.0, 0.0, 0.0
    for name, part in named.items():
        size = part.bounding_box().size
        if x > 0 and x + size.X > max_width:
            x, y, row_h = 0.0, y + row_h + gap, 0.0
        placed[name] = Pos(x, y, 0) * part
        x += size.X + gap
        row_h = max(row_h, size.Y)
    return placed


def write(named, stem, colours, gap=6.0):
    OUT.mkdir(parents=True, exist_ok=True)
    SCENES.mkdir(parents=True, exist_ok=True)
    placed = arrange(named, gap=gap, max_width=250.0)
    mesher = Mesher()
    scene = []
    for name, part in placed.items():
        part.label = name
        mesher.add_shape(part)
        export_stl(named[name], str(OUT / f"{name}.stl"), tolerance=0.02, angular_tolerance=0.1)
        export_stl(part, str(SCENES / f"{stem}-{name}.stl"), tolerance=0.03, angular_tolerance=0.15)
        scene.append({"file": f"{stem}-{name}.stl", "name": name, "color": colours(name)})
    mesher.write(str(OUT / f"{stem}.3mf"))
    import json
    (SCENES / f"{stem}.json").write_text(json.dumps({"parts": scene, "views": [
        {"label": stem, "dir": [-0.25, -0.8, 1.0]}]}, indent=1))
    span = max(max(q.bounding_box().max.X, q.bounding_box().max.Y) for q in placed.values())
    print(f"{stem}: {len(placed)} objects, plate use {span:.0f} mm,",
          "all valid" if all(q.is_valid for q in placed.values()) else "INVALID PART")


TPU, PETG = "#8d8f8c", "#6a7078"


def petg_colour(name):
    return "#efe6d2" if "bezel" in name or "ring" in name else PETG


if __name__ == "__main__":
    import sys
    seat, clip, snap, cap = seat_coupons(), clip_coupons(), snap_coupons(), cap_corner()
    # What Paul still has to print (see ../REMAINING-TESTS.md, group A).
    # Cap and clip tests passed on the second print (BODY-corner, CLIP-frame, CLIP-bezel): nothing to reissue.
    # Nothing to reissue: every fit test that does not need the board is finished.
    snap["SNAP-plate"]                                          # (SNAP-plate.stl in ../coupons/ is the printed 0.175 one)
    if "--all" in sys.argv:
        # Pieces already issued and unchanged. Rebuilding them is only needed after a design change;
        # the files in ../coupons/ are the ones Paul printed.
        write({name: material_slice(name) for name in ("T1", "T2", "T3")}, "coupons-material", lambda n: TPU, gap=4.0)
        write({"SEAT-body": seat["SEAT-body"], "CLIP-body": clip["CLIP-body"], "CAP-corner": cap["CAP-corner"],
               **end_slices()}, "coupons-fit-body", lambda n: TPU)
        write({"GLASS-ring": glass_ring(), "SEAT-frame": seat["SEAT-frame"], "SEAT-bezel": seat["SEAT-bezel"],
               "MIC-plate": mic_plate(), "PORT-block": port_block()}, "coupons-fit-petg", petg_colour)
