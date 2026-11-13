#!/usr/bin/env python3
"""Generates the sample SD card used by the simulator and tests: tests/fixtures/sd/.

Three recipes: one with steps only, one with steps + pages + photo (as if imported
from a PDF), one with pages only (a "scan"). Page images are drawn here, 905x1280
(long side 1280, as the web converter will write them). Needs Pillow. The output is
committed; re-run only to change the samples.
"""
import json
import pathlib
import shutil

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "tests" / "fixtures" / "sd" / "recipes"


def font(size):
    for name in ("DejaVuSerif.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def page(path, title, lines, number, scanned=False):
    img = Image.new("RGB", (905, 1280), (250, 246, 236) if scanned else (255, 255, 255))
    d = ImageDraw.Draw(img)
    d.text((70, 80), title, fill=(30, 30, 30), font=font(54))
    d.line((70, 160, 835, 160), fill=(120, 120, 120), width=3)
    y = 200
    for line in lines:
        d.text((70, y), line, fill=(40, 40, 40), font=font(30))
        y += 52
    d.text((430, 1200), f"- {number} -", fill=(120, 120, 120), font=font(24))
    if scanned:
        img = img.rotate(1.2, fillcolor=(235, 230, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, "JPEG", quality=72, optimize=True)


def photo(path, color):
    img = Image.new("RGB", (640, 480), color)
    d = ImageDraw.Draw(img)
    d.ellipse((170, 90, 470, 390), fill=(214, 160, 80), outline=(140, 90, 40), width=8)
    img.save(path, "JPEG", quality=75)
    img.resize((320, 240)).save(path.with_name("thumb.jpg"), "JPEG", quality=70)


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "inbox").mkdir(parents=True)
    (OUT / "inbox" / ".keep").write_text("")

    def write(rid, recipe):
        (OUT / rid).mkdir(parents=True, exist_ok=True)
        (OUT / rid / "recipe.json").write_text(json.dumps(recipe, indent=2) + "\n")

    write("pancakes", {
        "format": 1, "title": "Pancakes", "servings": 4,
        "ingredients": [{"qty": 1.5, "unit": "cup", "item": "flour", "note": "sifted"},
                        {"qty": 2, "unit": "", "item": "eggs"}, {"qty": 250, "unit": "ml", "item": "milk"},
                        {"qty": None, "unit": "", "item": "salt", "note": "to taste"}],
        "steps": [{"text": "Whisk the flour, eggs and milk until smooth."},
                  {"text": "Rest the batter for 30 minutes.", "timer_s": 1800},
                  {"text": "Fry 2-3 minutes per side until golden."}],
        "source": {"type": "manual"}, "tags": ["breakfast", "quick"], "created": "2026-11-12T09:00:00"})

    write("apple-pie", {
        "format": 1, "title": "Apple pie", "servings": 8,
        "ingredients": [{"qty": 6, "unit": "", "item": "apples", "note": "peeled and sliced"},
                        {"qty": 0.75, "unit": "cup", "item": "sugar"}, {"qty": 2, "unit": "tsp", "item": "cinnamon"},
                        {"qty": 500, "unit": "g", "item": "shortcrust pastry"}],
        "steps": [{"text": "Heat the oven to 200 degrees. Toss the apples with sugar and cinnamon."},
                  {"text": "Line the tin with pastry, fill, and cover with a pastry lid."},
                  {"text": "Bake 45-50 minutes until deep golden. Cool for 1 hour before slicing."}],
        "source": {"type": "pdf", "name": "apple-pie.pdf"}, "tags": ["dessert", "baking"],
        "created": "2026-11-12T09:10:00"})
    page(OUT / "apple-pie/pages/001.jpg", "Apple pie", [
        "Serves 8", "", "Ingredients", "6 apples, peeled and sliced", "3/4 cup sugar", "2 tsp cinnamon",
        "500 g shortcrust pastry", "", "Method", "1. Heat the oven to 200 degrees. Toss the",
        "   apples with sugar and cinnamon.", "2. Line the tin with pastry, fill, and",
        "   cover with a pastry lid."], 1)
    page(OUT / "apple-pie/pages/002.jpg", "Apple pie (continued)", [
        "3. Bake 45-50 minutes until deep golden.", "   Cool for 1 hour before slicing.", "",
        "Notes", "Bramley or Granny Smith apples hold", "their shape best.", "", "Serve warm with cream or custard."], 2)
    photo(OUT / "apple-pie/photo.jpg", (120, 150, 110))

    write("grandmas-stew", {
        "format": 1, "title": "Grandma's stew (scan)", "ingredients": [], "steps": [],
        "source": {"type": "pdf", "name": "scan0001.pdf"}, "tags": ["dinner"], "created": "2026-11-12T09:20:00"})
    for n in (1, 2, 3):
        page(OUT / f"grandmas-stew/pages/{n:03d}.jpg", "Stew" if n == 1 else f"Stew, page {n}", [
            "(handwritten recipe, scanned)", "", "Brown the beef well.", "Add onions, carrots,",
            "a bay leaf and stock.", "Simmer very gently", "for about 2 hours."][: 3 + n * 2], n, scanned=True)
    print("fixtures written to", OUT)


if __name__ == "__main__":
    main()
