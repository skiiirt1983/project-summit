"""Question sheet for §8.5: how wide is the cream bezel?  -> ../out/q-bezel.png"""
import math
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import params as p
import peak

OPTIONS = [  # label, cream wall outside the glass (visible cream = wall + 1.35 lip)
    ("A  2.95 mm cream, same as LyricFlash", 1.6),
    ("B  4.5 mm cream", 3.15),
    ("C  6 mm cream, as in the target picture", 4.65),
]
fa = math.radians(p.FRONT_ANGLE)
VIEWS = [
    {"label": "Landscape", "dir": [-1.0, -0.75, 0.55], "portrait": False},
    {"label": "Straight on", "dir": [-math.sin(fa), 0, math.cos(fa)], "portrait": False, "ortho": True, "noground": True},
    {"label": "Portrait", "dir": [-1.0, 0.7, 0.6], "portrait": True, "flip": True},
]
here = Path(__file__).resolve().parent
sheets = []
for label, wall in OPTIONS:
    p.BEZEL_WALL = wall
    p.BEZEL_R = 6.7 + (wall - 1.6)
    p.derive()
    out = peak.OUT / f"q-bezel-{label[0]}"
    info = peak.export(out, VIEWS, cad_files=False)
    png = out / "views.png"
    subprocess.run(["node", "render.mjs", str(out / "scene.json"), str(png)], cwd=here / "render", check=True)
    w, d, h = info["size_mm"][1], info["size_mm"][0], info["size_mm"][2]
    sheets.append((f"{label}   (body {w:.0f} wide x {h:.0f} tall x {d:.0f} deep)", Image.open(png).convert("RGB")))

try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 44)
except OSError:
    font = ImageFont.load_default()
bar = 80
width = max(im.width for _, im in sheets) * 3 // 4          # only three of four columns are used
sheet = Image.new("RGB", (width, sum(im.height + bar for _, im in sheets)), "#222")
y = 0
for title, im in sheets:
    ImageDraw.Draw(sheet).text((24, y + 14), title, fill="white", font=font)
    sheet.paste(im.crop((0, 0, width, im.height)), (0, y + bar))
    y += im.height + bar
sheet.thumbnail((2400, 4000))
sheet.save(peak.OUT / "q-bezel.png")
print("wrote", peak.OUT / "q-bezel.png")
