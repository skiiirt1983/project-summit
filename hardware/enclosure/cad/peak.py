"""The full Peak at true size.

Run:  python peak.py   -> STEP / STL / 3MF, modifier mesh and scene files in ../out/
World frame: X = front to back, Y = across the width, Z = up. Desk is Z = 0.
"""
import json
import math
from pathlib import Path

from build123d import (Axis, Cone, Cylinder, chamfer, Mesher, Plane, Polygon, Pos, Rot, Vector,
                       export_step, export_stl, extrude, fillet)

import params as p
import parts

OUT = Path(__file__).resolve().parent.parent / "out"

COLOURS = {"body": "#8d8f8c", "frame": "#3a3d40", "bezel": "#efe6d2", "plate": "#5b6470",
           "cover": "#4a4f55", "pads": "#1f6f6b", "glass": "#0b0b0d", "view": "#1d2b3a",
           "display": "#1b3a2a", "battery": "#3b6ea8", "accelerometer": "#8a3fa0"}
PRINTED = ("body", "frame", "bezel", "plate", "cover", "pads")


def _line_x(p0, d0, p1, d1):
    """Intersection of two 2D lines given as point + direction."""
    det = d0[0] * -d1[1] + d1[0] * d0[1]
    t = ((p1[0] - p0[0]) * -d1[1] + d1[0] * (p1[1] - p0[1])) / det
    return (p0[0] + t * d0[0], p0[1] + t * d0[1])


def profile():
    """Outer and inner triangle corners in the X-Z plane, plus the module centre."""
    fa, ba = math.radians(p.FRONT_ANGLE), math.radians(p.BACK_ANGLE)
    apex_angle = math.pi - fa - ba
    # Rounded corners eat into the flat front face; keep enough flat for the module.
    eat_base = p.OUTER_R / math.tan(fa / 2)
    eat_apex = p.OUTER_R / math.tan(apex_angle / 2)
    flat = p.MODULE_W + 2 * (p.SEAM + p.FRONT_MARGIN)
    slope = eat_base + flat + eat_apex
    a = (0.0, 0.0)
    apex = (slope * math.cos(fa), slope * math.sin(fa))
    b = (apex[0] + apex[1] / math.tan(ba), 0.0)
    # Inner opening: front wall FRONT_T thick, back leg and bottom LEG_T thick.
    df, db = (math.cos(fa), math.sin(fa)), (-math.cos(ba), math.sin(ba))
    nf, nb = (math.sin(fa), -math.cos(fa)), (-math.sin(ba), -math.cos(ba))
    pf = (nf[0] * p.FRONT_T, nf[1] * p.FRONT_T)
    pb = (b[0] + nb[0] * p.LEG_T, nb[1] * p.LEG_T)
    inner = [_line_x(pf, df, (0, p.LEG_T), (1, 0)), _line_x(pf, df, pb, db),
             _line_x(pb, db, (0, p.LEG_T), (1, 0))]
    s_c = eat_base + flat / 2
    centre = (s_c * df[0], s_c * df[1])
    return [a, apex, b], inner, centre


def face_plane():
    """The bezel face: module frame -> world."""
    _, _, c = profile()
    fa = math.radians(p.FRONT_ANGLE)
    normal = Vector(-math.sin(fa), 0, math.cos(fa))
    return Plane(origin=(c[0], 0, c[1]), x_dir=(0, 1, 0), z_dir=normal)


def _prism(points, radius, width):
    """Rounded triangle (X-Z profile) as a solid spanning `width` along Y."""
    face = fillet(Polygon(*points, align=None).vertices(), radius)
    return extrude(face, width / 2, both=True).rotate(Axis.X, 90)   # sketch Y becomes world Z


def _end_face(part, sign, width):
    """The flat face on one open end (+1 or -1 along Y)."""
    faces = [f for f in part.faces() if f.bounding_box().size.Y < 1e-6
             and abs(f.center().Y - sign * width / 2) < 1e-6]
    return max(faces, key=lambda f: f.area)


def _round_loop(part, edges, radius, sign, width, on_bed):
    """Round a loop of end edges. On the bed end: 45 degree chamfer first, then the round,
    so the surface never leans out more than 45 degrees from the bed."""
    if not on_bed:
        return fillet(edges, radius)
    drop = radius * (2 - math.sqrt(2))                 # chamfer size that meets the round at 45 degrees
    part = chamfer(edges, drop)
    upper = [e for e in part.edges() if e.bounding_box().size.Y < 1e-6
             and abs(e.center().Y - sign * (width / 2 - drop)) < 1e-4]
    return fillet(upper, radius)


def shell(width=None, soft_edges=True, rounded=None):
    """The hollow triangle on its own, with no pocket."""
    width = width or p.BODY_WIDTH
    rounded = p.ROUND_ALL if rounded is None else rounded
    outer, inner, _ = profile()
    solid = _prism(outer, p.OUTER_R, width)
    hole = _prism(inner, p.INNER_R, width + 2)
    if not soft_edges:
        return solid - hole
    if rounded:
        r_out = r_in = (p.LEG_T - p.EDGE_FLAT) / 2     # near full round on the 13 mm legs
    else:
        r_out, r_in = p.EDGE_R, p.EDGE_R_IN
    for sign in (1, -1):
        on_bed = rounded and p.BED_CHAMFER and sign == p.BED_END
        solid = _round_loop(solid, _end_face(solid, sign, width).outer_wire().edges(),
                            r_out, sign, width, on_bed)
    solid -= hole
    for sign in (1, -1):
        on_bed = rounded and p.BED_CHAMFER and sign == p.BED_END
        solid = _round_loop(solid, _end_face(solid, sign, width).inner_wires()[0].edges(),
                            r_in, sign, width, on_bed)
    return solid


def body(rounded=None):
    """Shell with the pocket, tunnels, holes and pad recesses cut in."""
    rounded = p.ROUND_ALL if rounded is None else rounded
    loc = face_plane().location
    raw = shell(rounded=rounded)
    cut = raw - loc * parts.body_cutter()
    # Soften the mouths the cuts leave behind: USB openings always, holes in the hollow when rounded.
    before = {tuple(round(v, 2) for v in e.center()) for e in raw.edges()}
    local = loc.inverse()
    usb, holes = [], []
    for e in cut.edges():
        c = e.center()
        if tuple(round(v, 2) for v in c) in before:
            continue
        depth = -(local * Pos(*c)).position.Z          # how far behind the screen face
        if e.bounding_box().size.Y < 1e-6 and abs(abs(c.Y) - p.BODY_WIDTH / 2) < 1e-4:
            usb.append(e)
        elif abs(depth - p.FRONT_T) < 0.05:
            holes.append(e)
    cut = fillet(usb, p.USB_MOUTH_R)
    if rounded:
        cut = fillet([e for e in cut.edges() if any((e.center() - h.center()).length < 1e-4 for h in holes)],
                     p.SMALL_R)
    _, recess = pads_and_recesses()
    return cut - recess


def pad_spots():
    """Pad centres. Landscape pads under the bottom, portrait pads on the +Y end."""
    outer, _, _ = profile()
    base = outer[2][0]
    eat = p.OUTER_R / math.tan(math.radians(p.FRONT_ANGLE) / 2)
    y = p.BODY_WIDTH / 2 - 25
    landscape = [(eat + 7, s * y) for s in (-1, 1)] + [(base - eat - 7, s * y) for s in (-1, 1)]
    apex = outer[1]
    portrait = [(26.0, 15.0), (apex[0], apex[1] - 2 * p.OUTER_R), (base - 24.7, 14.25)]
    return landscape, portrait


def pads_and_recesses():
    landscape, portrait = pad_spots()
    pads = recess = None
    for x, y in landscape:
        pad = Pos(x, y, p.PAD_RECESS - p.PAD_T / 2) * Cylinder(p.PAD_D / 2, p.PAD_T)
        cut = Pos(x, y, p.PAD_RECESS / 2 - 0.5) * Cylinder((p.PAD_D + p.PAD_FIT) / 2, p.PAD_RECESS + 1)
        pads = pad if pads is None else pads + pad
        recess = cut if recess is None else recess + cut
    end = p.BODY_WIDTH / 2
    for x, z in portrait:
        pads += Pos(x, end - p.PAD_RECESS + p.PAD_T / 2, z) * Rot(90, 0, 0) * Cylinder(p.PAD_D_END / 2, p.PAD_T)
        recess += Pos(x, end - p.PAD_RECESS / 2 + 0.5, z) * Rot(90, 0, 0) * Cylinder(
            (p.PAD_D_END + p.PAD_FIT) / 2, p.PAD_RECESS + 1)
        r = (p.PAD_D_END + p.PAD_FIT) / 2                # these recesses open onto the print plate:
        recess += Pos(x, end - p.PAD_RECESS - r / 2 + 0.01, z) * Rot(90, 0, 0) * Cone(r, 0.1, r)   # cone, not a flat ceiling
    return pads, recess


def build():
    """All parts in the world frame, keyed by name."""
    loc = face_plane().location
    pads, _ = pads_and_recesses()
    out = {
        "body": body(),
        "frame": loc * parts.frame(),
        "bezel": loc * parts.cup(),
        "plate": loc * parts.plate(),
        "cover": loc * parts.cover(),
        "pads": pads,
        "glass": loc * parts.glass(),
        "view": loc * parts.view(),
        "display": loc * parts.display(),
        "battery": loc * parts.battery(),
        "accelerometer": loc * parts.accelerometer(),
    }
    return out


def write_scene(out, parts_by_name, name="scene.json", views=None, offsets=None, hide=()):
    """Write STLs + a scene file for render/render.mjs. `offsets` moves parts (exploded view)."""
    out.mkdir(parents=True, exist_ok=True)
    listed = []
    for part_name, part in parts_by_name.items():
        if part_name in hide:
            continue
        shape = part.moved(Pos(*offsets[part_name])) if offsets and part_name in offsets else part
        stem = f"{Path(name).stem}-{part_name}" if offsets else part_name
        export_stl(shape, str(out / f"{stem}.stl"), tolerance=0.03, angular_tolerance=0.15)
        listed.append({"file": f"{stem}.stl", "color": COLOURS.get(part_name, "#999"), "name": part_name})
    scene = {"parts": listed}
    if views:
        scene["views"] = views
    (out / name).write_text(json.dumps(scene, indent=1))


def export(out=OUT):
    built = build()
    write_scene(out, built)
    mesher = Mesher()
    for name in PRINTED:
        export_step(built[name], str(out / f"{name}.step"))
        built[name].label = name
        mesher.add_shape(built[name])
    mesher.write(str(out / "peak.3mf"))
    export_stl(face_plane().location * parts.firm_zone(), str(out / "modifier-firm-zone.stl"))
    return built


if __name__ == "__main__":
    built = export()
    size = built["body"].bounding_box().size
    print("body", [round(v, 1) for v in size], "front wall", round(p.FRONT_T, 1), "module depth", round(p.MOD_D, 1))
    for name in PRINTED:
        print(f"  {name:6s} valid={built[name].is_valid} volume={built[name].volume / 1000:.1f} cm3")
