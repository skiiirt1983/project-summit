"""Pictures of the test cradle -> cradle-views.png (landscape, buttons side, underside, pulled apart)."""
import json
import subprocess
from pathlib import Path

from build123d import Pos, export_stl

import cradle as c

HERE = Path(__file__).resolve().parent
WORK = HERE / "_render"
RENDER = HERE.parent / "cad" / "render"


def scene(name, parts, views, offsets=None, hide=()):
    WORK.mkdir(exist_ok=True)
    listed = []
    for n, shape in parts.items():
        if n in hide:
            continue
        if offsets and n in offsets:
            shape = shape.moved(Pos(*offsets[n]))
        export_stl(shape, str(WORK / f"{name}-{n}.stl"), tolerance=0.03, angular_tolerance=0.15)
        listed.append({"file": f"{name}-{n}.stl", "name": n, "color": c.COLOURS.get(n, "#999")})
    (WORK / f"{name}.json").write_text(json.dumps({"parts": listed, "views": views}))
    subprocess.run(["node", "render.mjs", str(WORK / f"{name}.json"), str(WORK / f"{name}.png")], cwd=RENDER, check=True,
                   capture_output=True)


built = c.build()
stack = c.stand_ins()
everything = {"tray": built["tray"], "ring": built["ring"], **stack}
scene("assembled", everything, [
    {"label": "Assembled, from the front", "dir": [-0.7, -1.0, 0.9]},
    {"label": "Buttons and microSD side (open at the top, under the bezel)", "dir": [0.0, -1.0, 0.45]},
])
scene("open", {k: v for k, v in everything.items() if k != "ring"}, [
    {"label": "Bezel off: board stack in the tray", "dir": [-0.7, -1.0, 1.0], "noground": True},
    {"label": "USB-C end (both ports in one notch)", "dir": [-1.0, -0.3, 0.5], "noground": True},
])
scene("under", {"tray": built["tray"]}, [
    {"label": "Underside: countersunk screws, header / speaker / battery / RTC / camera windows, two mic holes",
     "dir": [0.0, -0.02, -1.0], "ortho": True, "noground": True}])
scene("apart", everything, [
    {"label": "Pulled apart: bezel ring, glass, board, tray", "dir": [-0.9, -1.0, 0.7], "noground": True}],
    offsets={"ring": (0, 0, 34), "glass": (0, 0, 20), "board": (0, 0, 8), "components": (0, 0, 8), "standoffs": (0, 0, 8)})
from PIL import Image, ImageDraw, ImageFont
imgs = [Image.open(WORK / f"{n}.png").convert("RGB") for n in ("assembled", "open", "under", "apart")]
w = max(i.width for i in imgs)
sheet = Image.new("RGB", (w, sum(i.height for i in imgs)), "white")
y = 0
for i in imgs:
    sheet.paste(i, (0, y))
    y += i.height
sheet.thumbnail((2600, 5200))
sheet.save(HERE / "cradle-views.png")
print("wrote", HERE / "cradle-views.png", sheet.size)
