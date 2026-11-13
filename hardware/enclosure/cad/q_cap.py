"""Preview for Paul: the port-end cap, on and off -> ../out/q-cap.png"""
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import params as p

p.CAP = True
p.derive()
import peak  # noqa: E402  (after the switch)

here = Path(__file__).resolve().parent
OUT = peak.OUT / "q-cap"
half = p.BODY_WIDTH / 2
seam = [52.0, p.SEAM_Y, 90.0]                      # on the front face, level with the bezel's top corner
ON = [
    {"label": "Landscape", "dir": [-1.0, -0.75, 0.55]},
    {"label": "Portrait (cap on top)", "dir": [-1.0, 0.7, 0.6], "portrait": True, "flip": True},
    {"label": "Seam close-up (port end)", "dir": [-1.0, -0.55, 0.45], "target": seam, "zoom": 2.0},
    {"label": "Matching groove (other end)", "dir": [-1.0, 0.55, 0.45], "target": [52.0, -p.SEAM_Y, 90.0], "zoom": 2.0},
]
OFF = [
    {"label": "Landscape, cap pulled off", "dir": [-1.0, -0.75, 0.55]},
    {"label": "Portrait, cap lifted off", "dir": [-1.0, 0.7, 0.6], "portrait": True, "flip": True},
]
CAP_ONLY = [{"label": "Cap, inside face: six pegs, pocket end, USB openings", "dir": [-0.55, 1.0, 0.5]}]
BODY_ONLY = [{"label": "Body, seam face: six peg holes, pocket open at the top", "dir": [-0.7, -1.0, 0.55],
              "target": [60.0, p.SEAM_Y + 20, 50.0], "zoom": 1.6}]


def render(scene, png):
    subprocess.run(["node", "render.mjs", str(scene), str(png)], cwd=here / "render", check=True)


built = peak.build()
show = {k: built[k] for k in ("body", "cap", "frame", "bezel", "glass", "view", "pads")}
peak.write_scene(OUT / "on", show, views=ON)
render(OUT / "on" / "scene.json", OUT / "on.png")
lift = (0, p.USB_END * 38.0, 0)
peak.write_scene(OUT / "off", show, name="scene.json", views=OFF, offsets={"cap": lift})
render(OUT / "off" / "scene.json", OUT / "off.png")
peak.write_scene(OUT / "cap", {"cap": built["cap"]}, views=CAP_ONLY)
render(OUT / "cap" / "scene.json", OUT / "cap.png")
peak.write_scene(OUT / "body", {k: built[k] for k in ("body", "frame", "pads")}, views=BODY_ONLY)
render(OUT / "body" / "scene.json", OUT / "body.png")


def side_by_side(*names):
    ims = [Image.open(OUT / f"{n}.png").convert("RGB") for n in names]
    out = Image.new("RGB", (sum(i.width for i in ims), max(i.height for i in ims)), "white")
    x = 0
    for im in ims:
        out.paste(im, (x, 0))
        x += im.width
    return out


font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 44)
rows = [("CAP ON", side_by_side("on")), ("CAP OFF", side_by_side("off")), ("THE JOINT", side_by_side("cap", "body"))]
bar, width = 80, max(im.width for _, im in rows)
rows = [(t, im.resize((width, im.height * width // im.width))) for t, im in rows]
sheet = Image.new("RGB", (width, sum(im.height + bar for _, im in rows)), "#222")
y = 0
for title, im in rows:
    ImageDraw.Draw(sheet).text((24, y + 14), title, fill="white", font=font)
    sheet.paste(im, (0, y + bar))
    y += im.height + bar
sheet.thumbnail((2800, 4000))
sheet.save(peak.OUT / "q-cap.png")
print("wrote", peak.OUT / "q-cap.png")
