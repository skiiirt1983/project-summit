"""Test coupons, cut from the real model so they test the real fits.

Run:  python test_coupons.py
  -> ../coupons/coupons-material.3mf   four triangle slices: T1 T2 T3 (TPU for AMS), P1 (stone PLA)
     ../coupons/coupons-fit-petg.3mf   PETG fit tests: glass ring, seat, clip, mic plate, port block
     ../coupons/coupons-fit-body.3mf   the body-side halves of the seat and clip tests (body material)
     plus one STL per coupon there, and render inputs in ../out/coupons/
Every coupon is laid flat in its print orientation. Per-object slicer settings are
listed in ../COUPON-RESULTS.md; the 3MF carries shapes and names only.
"""
from pathlib import Path

from build123d import Box, Cylinder, Mesher, Pos, Rot, Text, export_stl, extrude

import params as p
import parts
import peak

OUT = Path(__file__).resolve().parent.parent / "coupons"      # committed: print files
SCENES = peak.OUT / "coupons"                                    # scratch: render inputs
ENGRAVE = 0.6


def engrave_top(part, text, size, at=None):
    """Sink `text` into the top face."""
    bb = part.bounding_box()
    x, y = at if at else (bb.center().X, bb.center().Y)
    return part - Pos(x, y, bb.max.Z - ENGRAVE) * extrude(Text(text, size), ENGRAVE + 0.1)


def lay_flat(part, flip=False):
    """Put the part on the bed (z = 0), optionally upside down."""
    if flip:
        part = Rot(180, 0, 0) * part
    bb = part.bounding_box()
    return Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * part


def material_slice(label):
    """A short slice of the real triangle outline, lying on its side."""
    solid = peak.shell(width=p.MATERIAL_SLICE_T, soft_edges=False)
    solid = lay_flat(Rot(-90, 0, 0) * solid)                 # profile flat on the bed
    return engrave_top(solid, label, 12, at=(46, 38))        # in the thick front wall


def body_local():
    """The body, moved into the module frame so coupons can be cut square."""
    world = peak.shell() - peak.face_plane().location * parts.body_cutter()
    return peak.face_plane().location.inverse() * world


def seat_coupons():
    """One corner of body + frame + module (the end away from the ports, lower side)."""
    x0, y0 = p.GLASS_L / 2 - 26, p.GLASS_W / 2 - 22
    depth = p.POCKET_D + p.BACK_SKIN + 2
    box = Pos(x0 + 30, y0 + 30, -depth / 2 + 1) * Box(60, 60, depth)
    body_box = Pos(x0 + 22, y0 + 18.5, -depth / 2 + 1) * Box(44, 37, depth)   # trims the rounded outside
    return {
        "SEAT-body": lay_flat(body_local() & body_box),
        "SEAT-frame": lay_flat(parts.frame() & box),
        "SEAT-bezel": lay_flat(parts.cup() & box, flip=True),
        "SEAT-cover": lay_flat(parts.cover() & box),
        "SEAT-plate": lay_flat(parts.plate() & box),
    }


def clip_coupons():
    """A straight piece of the lower long side around one clip."""
    x = p.CLIP_X[1]
    depth = p.POCKET_D + p.BACK_SKIN + 2
    y_in = p.GLASS_W / 2 - 6
    box = Pos(x, y_in + 15, -depth / 2 + 1) * Box(30, 30, depth)
    body_box = Pos(x, y_in + 9, -depth / 2 + 1) * Box(30, 18, depth)
    return {
        "CLIP-body": lay_flat(body_local() & body_box),
        "CLIP-frame": lay_flat(parts.frame() & box),
        "CLIP-bezel": lay_flat(parts.cup() & box, flip=True),
    }


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


def port_block():
    """The full tunnel a USB-C plug passes through, at the real port spacing.

    Left window = USB_WINDOW as modelled, right window = 1 mm larger each way.
    Lay it on the bench against the board's USB end and push the cables through.
    """
    tunnel = p.SIDE_MARGIN + p.SEAM + p.BEZEL_WALL - p.GLASS_CLEAR
    spacing = abs(p.USB_Y[1] - p.USB_Y[0])
    height = p.USB_WINDOW[1] + 6
    block = Pos(0, 0, height / 2) * Box(spacing + p.USB_WINDOW[0] + 12, tunnel, height)
    for i, extra in enumerate((0.0, 1.0)):
        x = (i - 0.5) * spacing
        block -= Pos(x, 0, height / 2) * Box(p.USB_WINDOW[0] + extra, tunnel + 1, p.USB_WINDOW[1] + extra)
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


if __name__ == "__main__":
    seat, clip = seat_coupons(), clip_coupons()
    write({name: material_slice(name) for name in ("T1", "T2", "T3", "P1")}, "coupons-material",
          lambda n: "#b9b4a8" if n == "P1" else "#8d8f8c", gap=4.0)
    petg = {"GLASS-ring": glass_ring()}
    petg.update({k: v for k, v in {**seat, **clip}.items() if not k.endswith("body")})
    petg.update({"MIC-plate": mic_plate(), "PORT-block": port_block()})
    write(petg, "coupons-fit-petg", lambda n: "#efe6d2" if "bezel" in n or "ring" in n else "#6a7078")
    write({k: v for k, v in {**seat, **clip}.items() if k.endswith("body")}, "coupons-fit-body",
          lambda n: "#8d8f8c")
