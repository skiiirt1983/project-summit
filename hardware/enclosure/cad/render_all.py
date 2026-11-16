"""Review renders of the full model -> ../out/peak-views.png, peak-inside.png"""
import math
import subprocess
from pathlib import Path

from build123d import Box, Pos

import params as p
import parts
import peak

here = Path(__file__).resolve().parent
fa = math.radians(p.FRONT_ANGLE)
n = (-math.sin(fa), 0, math.cos(fa))                 # out of the screen


def render(scene, png):
    subprocess.run(["node", "render.mjs", str(peak.OUT / scene), str(peak.OUT / png)],
                   cwd=here / "render", check=True)


built = peak.export()
hidden = ("display", "battery", "accelerometer")
peak.write_scene(peak.OUT, built, "scene.json", hide=hidden, views=[
    {"label": "Landscape", "dir": [-1.0, -0.75, 0.55]},
    {"label": "Side", "dir": [0, -1, 0], "ortho": True},
    {"label": "From behind", "dir": [1.0, 0.75, 0.55]},
    {"label": "Portrait (ports on top)", "dir": [-1.0, 0.7, 0.6], "portrait": True, "flip": True},
])
render("scene.json", "peak-views.png")

# Inside: exploded stack, and a cut through the middle of the width.
steps = {"frame": 45, "cover": 85, "battery": 112, "accelerometer": 112, "plate": 140,
         "display": 165, "glass": 165, "view": 165, "bezel": 200}
offsets = {k: tuple(c * d for c in n) for k, d in steps.items()}
offsets["cap"] = (0, p.USB_END * 45.0, 0)                 # the cap pulls off sideways
peak.write_scene(peak.OUT, built, "scene-exploded.json", offsets=offsets, hide=("pads",), views=[
    {"label": "Pulled apart, from the front", "dir": [-0.6, -1.0, 0.5], "noground": True},
    {"label": "Pulled apart, from behind", "dir": [1.0, -1.0, 0.35], "noground": True},
])
keep = Pos(0, 300 - 8.0, 0) * Box(600, 600, 600)     # slice 8 mm off centre, through a USB port
cut = {k: v & keep for k, v in built.items() if k != "pads"}
cut = {k: v for k, v in cut.items() if v.volume > 0}
zero = {k: (0, 0, 0) for k in cut}
peak.write_scene(peak.OUT, cut, "scene-cut.json", offsets=zero, views=[
    {"label": "Cut through the middle", "dir": [0, -1, 0], "ortho": True, "noground": True},
    {"label": "Cut, front wall close-up", "dir": [-0.35, -1, 0.2], "noground": True},
])
render("scene-exploded.json", "peak-exploded.png")
render("scene-cut.json", "peak-cut.png")

# Close-up slice through the lip that hides the frame (upper long side of the pocket).
loc = peak.face_plane().location
window = loc * (Pos(1, -p.FRAME_W / 2 + 1, -4.5) * Box(2, 14, 11))
lip = {k: built[k] & window for k in ("body", "frame", "bezel", "glass")}
lip = {k: v for k, v in lip.items() if v.volume > 0}
peak.write_scene(peak.OUT, lip, "scene-lip.json", offsets={k: (0, 0, 0) for k in lip}, views=[
    {"label": "Slice through the lip: body (grey) slopes at 45 degrees over the frame (dark); cream bezel on the left",
     "dir": [0, -1, 0], "ortho": True, "noground": True}])
render("scene-lip.json", "peak-lip.png")

# Button access: where the three pusher pins leave the case, and the path from a button to the ridge.
from build123d import Pos as _Pos
xs = parts.button_xs()
exit_w = loc * _Pos(xs[1], p.BTN_EXIT_Y, p.BTN_Z)
exit_pt = [exit_w.position.X, exit_w.position.Y, exit_w.position.Z]
keep_half = _Pos(0, xs[1] - 300, 0) * Box(600, 600, 600)                        # world Y <= the middle button
path = {k: built[k] & keep_half for k in ("body", "frame", "bezel", "cover", "plate", "pins", "display") if k in built}
path = {k: v for k, v in path.items() if v.volume > 0}
peak.write_scene(peak.OUT, built, "scene-buttons.json", hide=("battery", "accelerometer", "display"), views=[
    {"label": "Landscape: three pins on the top ridge", "dir": [0.35, -0.9, 0.85], "target": exit_pt, "zoom": 2.4},
    {"label": "Portrait: the ridge is the vertical back edge", "dir": [1.0, -0.2, 0.6], "portrait": True, "flip": True},
])
peak.write_scene(peak.OUT, path, "scene-button-path.json", offsets={k: (0, 0, 0) for k in path}, views=[
    {"label": "Cut through the middle button: pin from the button to the ridge", "dir": [0, 1, 0], "ortho": True, "noground": True}])
render("scene-buttons.json", "peak-buttons.png")
render("scene-button-path.json", "peak-button-path.png")
