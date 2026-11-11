"""Stack a reference image above the current render: python compare.py -> ../out/compare-target-vs-cad.png"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

here = Path(__file__).resolve().parent
ref = Image.open(here / "../ref/peak-final-60.png").convert("RGB")
cad = Image.open(here / "../out/peak-views.png").convert("RGB")
w, bar = 2000, 46
ref = ref.resize((w, ref.height * w // ref.width))
cad = cad.resize((w, cad.height * w // cad.width))
sheet = Image.new("RGB", (w, ref.height + cad.height + 2 * bar), "#222")
try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
except OSError:
    font = ImageFont.load_default()
draw = ImageDraw.Draw(sheet)
draw.text((16, 8), "TARGET  ref/peak-final-60.png", fill="white", font=font)
sheet.paste(ref, (0, bar))
draw.text((16, ref.height + bar + 8), "CAD at true size (bezel + battery still placeholders)", fill="white", font=font)
sheet.paste(cad, (0, ref.height + 2 * bar))
sheet.save(here / "../out/compare-target-vs-cad.png")
