"""The Peak at true size: body + screen module (outer shapes only, milestone 2).

Run:  python peak.py   -> writes STEP / STL / 3MF and scene.json to ../out/
Frame: X = front to back, Y = across the width, Z = up. Desk is Z = 0.
"""
import json
import math
from pathlib import Path

from build123d import (Axis, Box, BuildPart, BuildSketch, Mesher, Mode, Plane,
                       Polygon, Pos, RectangleRounded, Vector, add, export_step,
                       export_stl, extrude, fillet)

import params as p

OUT = Path(__file__).resolve().parent.parent / "out"


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


def _prism(points, radius):
    """Rounded triangle (X-Z profile) as a solid spanning the body width along Y."""
    face = fillet(Polygon(*points, align=None).vertices(), radius)
    solid = extrude(face, p.BODY_WIDTH / 2, both=True)
    return solid.rotate(Axis.X, 90)          # sketch Y becomes world Z


def build():
    outer, inner, c = profile()
    fa = math.radians(p.FRONT_ANGLE)
    normal = Vector(-math.sin(fa), 0, math.cos(fa))          # out of the front face
    face = Plane(origin=(c[0], 0, c[1]), x_dir=(0, 1, 0), z_dir=normal)
    depth = p.MODULE_DEPTH + p.GLASS_RECESS

    def soften(part, radius):
        """Round the edges on the two open ends."""
        ends = [e for e in part.edges() if e.bounding_box().size.Y < 1e-6
                and abs(abs(e.center().Y) - p.BODY_WIDTH / 2) < 1e-6]
        return fillet(ends, radius)

    shell = soften(soften(_prism(outer, p.OUTER_R), p.EDGE_R)
                   - _prism(inner, p.INNER_R), p.EDGE_R_IN)
    with BuildPart() as body:
        add(shell)
        with BuildSketch(face):                               # seat for the module
            RectangleRounded(p.MODULE_L + 2 * p.SEAM, p.MODULE_W + 2 * p.SEAM,
                             p.BEZEL_R + p.SEAM)
        extrude(amount=-depth, mode=Mode.SUBTRACT)

    with BuildPart() as bezel:
        with BuildSketch(face):
            RectangleRounded(p.MODULE_L, p.MODULE_W, p.BEZEL_R)
            RectangleRounded(p.GLASS_L, p.GLASS_W, p.GLASS_R, mode=Mode.SUBTRACT)
        extrude(amount=-depth)

    with BuildPart() as glass:
        with BuildSketch(face.offset(-p.GLASS_RECESS)):
            RectangleRounded(p.GLASS_L, p.GLASS_W, p.GLASS_R)
        extrude(amount=-1.2)

    with BuildPart() as view:                                  # lit screen area, render only
        with BuildSketch(face.offset(-p.GLASS_RECESS + 0.15)):
            RectangleRounded(p.VIEW_L, p.VIEW_W, 0.5)
        extrude(amount=-0.2)

    with BuildPart() as inner:                                 # stand-in for LCD + board, render only
        with BuildSketch(face.offset(-p.GLASS_RECESS - 1.2)):
            RectangleRounded(p.GLASS_L, p.GLASS_W, p.GLASS_R)
        extrude(amount=-(p.MODULE_DEPTH - 1.2))

    # Placeholder battery block behind the board, to check it fits inside the front wall.
    batt_plane = face.offset(-(p.MODULE_DEPTH + p.BATT_BOARD_GAP))
    with BuildPart() as batt:
        with BuildSketch(batt_plane):
            RectangleRounded(p.BATT_L, p.BATT_W, 2)
        extrude(amount=-p.BATT_T)

    # Windows for the two USB-C plugs, through one end face of the body.
    body_part = body.part
    reach = p.BODY_WIDTH / 2 - p.GLASS_L / 2 + 4
    for off in p.USB_OFFSETS:
        body_part -= face * Pos(p.USB_END * (p.BODY_WIDTH / 2 - reach / 2 + 1), off, -p.USB_DEPTH) * Box(
            reach + 2, p.USB_WINDOW[0], p.USB_WINDOW[1])
    bezel_part = bezel.part
    for off in p.USB_OFFSETS:
        bezel_part -= face * Pos(p.USB_END * (p.GLASS_L / 2 + p.BEZEL_W / 2), off, -p.USB_DEPTH) * Box(
            p.BEZEL_W + 2, p.USB_WINDOW[0], p.USB_WINDOW[1])

    return {"body": body_part, "bezel": bezel_part, "glass": glass.part,
            "view": view.part, "inner": inner.part, "battery": batt.part}


COLOURS = {"body": "#8d8f8c", "bezel": "#efe6d2", "glass": "#0b0b0d",
           "view": "#1d2b3a", "inner": "#15171a", "battery": "#3b6ea8"}

if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    parts = build()
    mesher = Mesher()
    for name, part in parts.items():
        export_stl(part, str(OUT / f"{name}.stl"), tolerance=0.02, angular_tolerance=0.1)
        if name not in ("view", "inner", "battery"):
            export_step(part, str(OUT / f"{name}.step"))
            part.label = name
            mesher.add_shape(part)
    mesher.write(str(OUT / "peak.3mf"))
    bb = parts["body"].bounding_box()
    info = {"size_mm": [round(v, 1) for v in bb.size], "front_wall_mm": round(p.FRONT_T, 1),
            "parts": [{"file": f"{n}.stl", "color": COLOURS[n], "name": n} for n in parts]}
    (OUT / "scene.json").write_text(json.dumps(info, indent=1))
    print(info["size_mm"], "front wall", info["front_wall_mm"])
