"""Preview for Paul: current body vs the rounded version -> ../out/q-round.png, q-round-detail.png"""
import subprocess
from pathlib import Path

from build123d import Box, Pos
from PIL import Image, ImageDraw, ImageFont

import params as p
import peak

here = Path(__file__).resolve().parent
OUT = peak.OUT / "q-round"
VIEWS = [
    {"label": "Landscape", "dir": [-1.0, -0.75, 0.55]},
    {"label": "End close-up (port end)", "dir": [-0.45, -1.0, 0.3]},
    {"label": "From behind (bed end, pads)", "dir": [1.0, 0.75, 0.55]},
    {"label": "Portrait", "dir": [-1.0, 0.7, 0.6], "portrait": True, "flip": True},
]
DETAIL = [{"label": "Slice through the bottom leg.  Left: port end (top when printing).  Right: bed end",
           "dir": [1, 0, 0], "ortho": True, "noground": True},
          {"label": "Bed end, seen from below-behind", "dir": [0.9, 0.8, -0.25], "noground": True}]


def render(scene, png):
    subprocess.run(["node", "render.mjs", str(scene), str(png)], cwd=here / "render", check=True)


def sheet(rows, path):
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 44)
    bar, width = 80, max(im.width for _, im in rows)
    out = Image.new("RGB", (width, sum(im.height + bar for _, im in rows)), "#222")
    y = 0
    for title, im in rows:
        ImageDraw.Draw(out).text((24, y + 14), title, fill="white", font=font)
        out.paste(im, (0, y + bar))
        y += im.height + bar
    out.thumbnail((2400, 4000))
    out.save(path)
    print("wrote", path)


built = peak.build()
extras = {k: built[k] for k in ("bezel", "glass", "view", "pads")}
half = p.BODY_WIDTH / 2
slab = Pos(70, 0, 6.5) * Box(3, 400, 15)                  # 3 mm slice across the bottom leg


def leg_ends(shape):
    """The two ends of the bottom-leg slice, pushed together so both fit one close-up."""
    cut = shape & slab
    left = cut & (Pos(70, -half + 9, 0) * Box(50, 18.2, 50))
    right = cut & (Pos(70, half - 9, 0) * Box(50, 18.2, 50))
    return left.moved(Pos(0, half - 22, 0)) + right.moved(Pos(0, -(half - 22), 0))
rows, detail = [], []
for title, rounded in (("NOW", False), ("ROUNDED: near full round inside and out; screen opening stays crisp", True)):
    shape = peak.body(rounded=rounded)
    tag = "round" if rounded else "now"
    peak.write_scene(OUT / tag, {"body": shape, **extras}, views=VIEWS)
    render(OUT / tag / "scene.json", OUT / f"{tag}.png")
    rows.append((title, Image.open(OUT / f"{tag}.png").convert("RGB")))
    peak.write_scene(OUT / f"{tag}-cut", {"body": leg_ends(shape)}, views=DETAIL[:1])
    peak.write_scene(OUT / f"{tag}-end", {"body": shape, "pads": built["pads"]}, views=DETAIL[1:])
    render(OUT / f"{tag}-end" / "scene.json", OUT / f"{tag}-end.png")
    render(OUT / f"{tag}-cut" / "scene.json", OUT / f"{tag}-cut.png")
    a, b = (Image.open(OUT / f"{tag}-{k}.png").convert("RGB") for k in ("cut", "end"))
    both = Image.new("RGB", (a.width + b.width, max(a.height, b.height)), "white")
    both.paste(a, (0, 0))
    both.paste(b, (a.width, 0))
    detail.append((title, both))
    print(tag, "valid", shape.is_valid, "volume cm3", round(shape.volume / 1000, 1))
sheet(rows, peak.OUT / "q-round.png")
sheet(detail, peak.OUT / "q-round-detail.png")
