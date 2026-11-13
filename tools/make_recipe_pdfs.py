#!/usr/bin/env python3
"""Generates the sample PDFs for the web import test: tests/fixtures/pdf/.

  lemon-cake.pdf         a text recipe (title, servings, ingredients, numbered method)
  dips-two-recipes.pdf   two recipes on one page
  scan.pdf               a scanned page: an image, no text layer

Written by hand (text PDFs) and with Pillow (the scan) so no copyrighted recipes are
needed. The output is committed; re-run only to change the samples.
"""
import pathlib

from PIL import Image, ImageDraw, ImageFont

OUT = pathlib.Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "pdf"


def text_pdf(path, lines):
    """lines: [(font_size, text)], top to bottom on one A4 page, Helvetica."""
    esc = lambda s: s.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")
    y, ops = 780, []
    for size, text in lines:
        y -= size * 1.6
        if text:
            ops.append(f"BT /F1 {size} Tf 60 {y:.0f} Td ({esc(text)}) Tj ET")
    stream = "\n".join(ops).encode("latin-1")
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
        b"<< /Length %d >>\nstream\n" % len(stream) + stream + b"\nendstream",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    ]
    out, offsets = bytearray(b"%PDF-1.4\n"), []
    for i, body in enumerate(objs, 1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % i + body + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for off in offsets:
        out += b"%010d 00000 n \n" % off
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)
    path.write_bytes(bytes(out))


def scan_pdf(path):
    img = Image.new("RGB", (1240, 1754), (246, 241, 230))
    d = ImageDraw.Draw(img)
    try:
        big, small = ImageFont.truetype("DejaVuSerif.ttf", 70), ImageFont.truetype("DejaVuSerif.ttf", 44)
    except OSError:
        big = small = ImageFont.load_default()
    d.text((120, 140), "Beef stew", fill=(40, 40, 40), font=big)
    for i, line in enumerate(["Brown the beef well.", "Add onions, carrots and stock.", "Simmer gently for 2 hours."]):
        d.text((120, 320 + i * 90), line, fill=(50, 50, 50), font=small)
    img.rotate(1.5, fillcolor=(232, 226, 212)).save(path, "PDF", resolution=150)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    text_pdf(OUT / "lemon-cake.pdf", [
        (26, "Lemon Drizzle Cake"), (12, "Serves 8"), (12, ""), (16, "Ingredients"),
        (12, "225 g butter, softened"), (12, "225 g caster sugar"), (12, "4 eggs"), (12, "1 1/2 cups self-raising flour"),
        (12, "1 lemon, zested"), (12, ""), (16, "Method"),
        (12, "1. Heat the oven to 180 degrees and line a loaf tin."),
        (12, "2. Beat the butter and sugar until pale, then add the eggs."),
        (12, "3. Fold in the flour and zest. Bake for 45-50 minutes."),
        (12, "4. Cool in the tin for 10 minutes before turning out."),
    ])
    text_pdf(OUT / "dips-two-recipes.pdf", [
        (24, "Guacamole"), (12, ""), (15, "Ingredients:"), (12, "2 avocados"), (12, "1 lime, juiced"), (12, "1/2 tsp salt"),
        (15, "Directions"), (12, "Mash the avocados with the lime juice."), (12, "Season with the salt and serve."), (12, ""),
        (24, "Tomato Salsa"), (12, "Makes 4"), (15, "Ingredients"), (12, "4 tomatoes, diced"), (12, "1 onion, finely chopped"),
        (15, "Instructions"), (12, "Mix the tomatoes and onion."), (12, "Chill for 1 hour before serving."),
    ])
    scan_pdf(OUT / "scan.pdf")
    for f in sorted(OUT.iterdir()):
        print(f.name, f.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
