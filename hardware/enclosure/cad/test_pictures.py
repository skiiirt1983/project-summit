"""Small pictures for REMAINING-TESTS: each test's pieces, apart and put together.

Run:  python test_pictures.py   -> ../tests/<test>.png
"""
import json
import subprocess
from pathlib import Path

from build123d import Pos, Rot, export_stl
from PIL import Image, ImageDraw, ImageFont

import params as p
import parts
import test_coupons as tc

here = Path(__file__).resolve().parent
OUT = here.parent / "tests"
WORK = here.parent / "out" / "test-pictures"
TPU, FRAME, CREAM, PLATE, COVER, BOARD, GLASS = "#8d8f8c", "#3a3d40", "#efe6d2", "#5b7fa6", "#c98a3c", "#2f6b46", "#101114"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def render(name, pieces, view):
    """pieces: list of (label, colour, shape). Returns a PIL image."""
    folder = WORK / name
    folder.mkdir(parents=True, exist_ok=True)
    scene = []
    for i, (label, colour, shape) in enumerate(pieces):
        export_stl(shape, str(folder / f"{i}.stl"), tolerance=0.03, angular_tolerance=0.15)
        scene.append({"file": f"{i}.stl", "name": label, "color": colour})
    (folder / "scene.json").write_text(json.dumps({"parts": scene, "views": [view]}))
    subprocess.run(["node", "render.mjs", str(folder / "scene.json"), str(folder / "view.png")],
                   cwd=here / "render", check=True, capture_output=True)
    return Image.open(folder / "view.png").convert("RGB")


ACTIVE = {"A4-buttons"}          # pictures for tests still open; finished tests are not redrawn


def picture(name, pieces, apart, direction, flip=False, zoom=1.0):
    if name not in ACTIVE:
        return
    """Left: pieces apart (each moved by its `apart` offset). Right: put together. Legend below."""
    turn = Rot(180, 0, 0) if flip else Pos(0, 0, 0)
    together = [(lab, col, turn * shp) for lab, col, shp in pieces]
    spread = [(lab, col, turn * (Pos(*apart[lab]) * shp)) for lab, col, shp in pieces]
    view = {"label": "", "dir": direction, "noground": True}
    if zoom != 1.0:
        view["zoom"] = zoom
    a, b = render(name + "-apart", spread, view), render(name + "-together", together, view)
    w, h = 640, 512
    a, b = a.resize((w, h)), b.resize((w, h))
    legend_h = 44
    sheet = Image.new("RGB", (2 * w, h + legend_h + 34), "white")
    sheet.paste(a, (0, 34))
    sheet.paste(b, (w, 34))
    d = ImageDraw.Draw(sheet)
    bold, font = ImageFont.truetype(BOLD, 20), ImageFont.truetype(FONT, 18)
    d.text((14, 8), "The pieces", fill="#222", font=bold)
    d.text((w + 14, 8), "Put together", fill="#222", font=bold)
    x = 14
    for lab, col, _ in pieces:
        d.rectangle([x, h + 46, x + 22, h + 68], fill=col, outline="#444")
        d.text((x + 30, h + 46), lab, fill="#222", font=font)
        x += 30 + int(d.textlength(lab, font=font)) + 26
    OUT.mkdir(exist_ok=True)
    sheet.save(OUT / f"{name}.png")
    print("wrote", OUT / f"{name}.png")


def screen_stand_in():
    """Glass + display + board as two blocks, in the module frame."""
    return parts.glass(), parts.display()


if __name__ == "__main__":
    cap = tc.cap_raw()
    picture("A1-cap", [("BODY-corner (new)", TPU, cap["BODY-corner"]), ("CAP-corner (already printed)", "#9fb48a", cap["CAP-corner"])],
            {"BODY-corner (new)": (0, 0, 0), "CAP-corner (already printed)": (0, p.USB_END * 24, 0)}, [0.6, -1.0, 0.55])

    clip = tc.clip_raw()
    picture("A2-clip", [("CLIP-body (already printed)", TPU, clip["CLIP-body"]), ("CLIP-frame (new)", FRAME, clip["CLIP-frame"]),
                        ("CLIP-bezel (new)", CREAM, clip["CLIP-bezel"])],
            {"CLIP-body (already printed)": (0, 0, 0), "CLIP-frame (new)": (0, 0, 28), "CLIP-bezel (new)": (0, 0, 62)},
            [-0.7, -1.0, 0.5])

    snap = tc.snap_raw()
    picture("A3-snap", [("SNAP-bezel", CREAM, snap["SNAP-bezel"]), ("SNAP-plate", PLATE, snap["SNAP-plate"]),
                        ("SNAP-cover", COVER, snap["SNAP-cover"])],
            {"SNAP-bezel": (0, 0, 0), "SNAP-plate": (0, 0, -26), "SNAP-cover": (0, 0, -52)},
            [-0.8, 1.0, 0.7], flip=True)

    seat = tc.seat_raw()
    picture("B1-seat", [("SEAT-body", TPU, seat["SEAT-body"]), ("SEAT-frame", FRAME, seat["SEAT-frame"]),
                        ("SEAT-bezel", CREAM, seat["SEAT-bezel"])],
            {"SEAT-body": (0, 0, 0), "SEAT-frame": (0, 0, 34), "SEAT-bezel": (0, 0, 70)}, [-0.7, -1.0, 0.55])

    with tc.frozen_v1():                                         # same positions as the printed strip
        btn = tc.button_raw()
    pins = {"BTN-body (TPU)": ("#8d8f8c", btn["BTN-body"]), "BTN-frame": (FRAME, btn["BTN-frame"]),
            "BTN-cup": (CREAM, btn["BTN-cup"]), "BTN-pin (3 band sizes)": ("#d8452f", btn["BTN-pins"])}
    picture("A4-buttons", [(k, c, s) for k, (c, s) in pins.items()],
            {"BTN-body (TPU)": (0, 0, 0), "BTN-frame": (0, 34, 0), "BTN-cup": (0, 54, 0), "BTN-pin (3 band sizes)": (0, -26, 0)},
            [-0.6, -0.9, 0.7], flip=False)

    glass, board = screen_stand_in()
    ring = parts.cup() & (Pos(0, 0, -3) * tc.Box(400, 400, 6))
    picture("A5-glass", [("GLASS-ring (and B, slightly larger)", CREAM, ring), ("screen glass", GLASS, glass), ("the board behind it", BOARD, board)],
            {"GLASS-ring (and B, slightly larger)": (0, 0, 0), "screen glass": (0, 0, -30), "the board behind it": (0, 0, -30)},
            [-0.6, -1.0, 0.75], flip=True)

    tunnel = p.SIDE_MARGIN + p.SEAM + p.BEZEL_WALL - p.GLASS_CLEAR
    height = tc.PORT_BLOCK_BASE[1] + 6
    block = Pos(-p.GLASS_L / 2 - tunnel / 2, 0, -p.board_depth(p.USB_H) - height / 2) * Rot(0, 0, 90) * tc.port_block()
    picture("C2-port", [("PORT-block (already printed)", PLATE, block), ("screen glass", GLASS, glass), ("the board", BOARD, board)],
            {"PORT-block (already printed)": (-26, 0, 0), "screen glass": (0, 0, 0), "the board": (0, 0, 0)},
            [-1.0, -0.8, 0.6], flip=True, zoom=1.0)

    picture("C3-plate", [("SNAP-plate", PLATE, snap["SNAP-plate"]), ("screen glass", GLASS, glass), ("the board", BOARD, board)],
            {"SNAP-plate": (0, 0, -24), "screen glass": (0, 0, 0), "the board": (0, 0, 0)},
            [0.8, -1.0, 0.7], flip=True)
