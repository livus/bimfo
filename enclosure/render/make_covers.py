#!/usr/bin/env python3
"""MakerWorld covers: web (4:3) and mobile (3:4), from the photo of the finished print
and a panel drawn like the stop display: amber LEDs in the Wiener Linien text face
(tools/wl_glyphs.py) on the green glow of the firmware, with the low-floor tram of the
start screen. Writes cover_web.jpg and cover_mobile.jpg into ../img/makerworld.

Run: py -3 make_covers.py   (needs Pillow and numpy)
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import gen_fonts  # noqa: E402
import wl_glyphs  # noqa: E402

IMG = os.path.join(HERE, "..", "img", "makerworld")
PHOTO = os.path.join(IMG, "front_1.jpg")

TEXT = gen_fonts.wl_text_glyphs()
AMBER = np.array([0xFF, 0xD0, 0x2A], np.float32)
GLOW_CENTRE = np.array([0x1E, 0x44, 0x26], np.float32)
GLOW_EDGE = np.array([0x0A, 0x17, 0x0D], np.float32)
UNLIT = 0.72                    # an unlit LED darkens the glow behind it
SS = 3                          # supersampling of the LED dots


def text(s, scale=1):
    """Rows ('#' lit, '.' dark) of `s` in the text face, every LED magnified `scale`
    times; as in the firmware, one dark LED between glyphs and three for a space."""
    cols = []
    for ch in s:
        if ch == " ":
            cols += ["." * 11] * 3
            continue
        rows = TEXT.get(ord(ch))
        if not rows:
            raise SystemExit('no glyph for "%s"' % ch)
        w = max(len(r) for r in rows)
        rows = [r.ljust(w, ".") for r in rows] + ["." * w] * (11 - len(rows))
        cols += ["".join(r[c] for r in rows) for c in range(w)] + ["." * 11]
    cols = cols[:-1]
    rows = ["".join(c[r] for c in cols) for r in range(11)]
    return [("".join(ch * scale for ch in r)) for r in rows for _ in range(scale)]


def tram():
    """The tram under its wire, on its rail: as the start screen draws it."""
    w = len(wl_glyphs.ULF[0])
    return ["#" * w] + list(wl_glyphs.ULF) + ["#" * w]


def size(rows):
    return (max(len(r) for r in rows) if rows else 0), len(rows)


def panel(w, h, pitch, items):
    """The LED panel, w x h px with LEDs every `pitch` px; items: (rows, x, y) in LEDs."""
    cols, nrows = w // pitch, h // pitch
    ox, oy = (w - cols * pitch) / 2, (h - nrows * pitch) / 2
    level = np.zeros((nrows, cols), np.float32)
    for rows, x, y in items:
        for j, row in enumerate(rows):
            for i, ch in enumerate(row):
                if ch in "#+" and 0 <= x + i < cols and 0 <= y + j < nrows:
                    level[y + j, x + i] = max(level[y + j, x + i], 1.0 if ch == "#" else 0.4)

    # green glow, brightest in the middle
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.sqrt(((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2) / np.sqrt(2)
    t = np.clip(r, 0, 1)[..., None] ** 1.3
    glow = GLOW_CENTRE * (1 - t) + GLOW_EDGE * t

    # LED dots, supersampled: coverage of unlit and lit dots
    unlit = Image.new("L", (w * SS, h * SS))
    lit = Image.new("L", (w * SS, h * SS))
    du, dl = ImageDraw.Draw(unlit), ImageDraw.Draw(lit)
    rad = pitch * 0.45 * SS
    for j in range(nrows):
        cy = (oy + (j + 0.5) * pitch) * SS
        for i in range(cols):
            cx = (ox + (i + 0.5) * pitch) * SS
            box = (cx - rad, cy - rad, cx + rad, cy + rad)
            if level[j, i]:
                dl.ellipse(box, fill=round(255 * level[j, i]))
            else:
                du.ellipse(box, fill=255)
    unlit = np.asarray(unlit.resize((w, h), Image.BOX), np.float32)[..., None] / 255
    lit = lit.resize((w, h), Image.BOX)
    bloom = np.asarray(lit.filter(ImageFilter.GaussianBlur(pitch * 1.6)), np.float32)[..., None] / 255
    lit = np.asarray(lit, np.float32)[..., None] / 255

    img = glow * (1 - unlit * (1 - UNLIT))
    img = img * (1 - lit) + AMBER * lit
    img = 255 - (255 - img) * (1 - 0.8 * bloom * AMBER / 255)   # screen the bloom on top
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def photo():
    """The print photo, upright, with more punch: levels, contrast, colour, sharpness."""
    im = ImageOps.exif_transpose(Image.open(PHOTO)).convert("RGB")
    im = ImageOps.autocontrast(im, cutoff=(0.4, 0.2), preserve_tone=True)
    im = ImageEnhance.Contrast(im).enhance(1.10)
    im = ImageEnhance.Color(im).enhance(1.18)
    return im


def place(im, box, w, h):
    """Crop `box` (fractions of the photo: left, top, right, bottom; widened or
    narrowed about its centre to the aspect w:h) and scale it to w x h."""
    W, H = im.size
    l, t, r, b = box[0] * W, box[1] * H, box[2] * W, box[3] * H
    cx, bw, bh = (l + r) / 2, r - l, b - t
    bw = bh * w / h
    l = min(max(cx - bw / 2, 0), W - bw)
    out = im.crop((round(l), round(t), round(l + bw), round(b))).resize((w, h), Image.LANCZOS)
    return out.filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))


def save(img, name):
    path = os.path.join(IMG, name)
    img.save(path, quality=92, subsampling=0, optimize=True)
    print("wrote", path, img.size, os.path.getsize(path) // 1024, "KB")


def web(im):
    # 2400 x 1800: the panel on the left, the photo on the right
    W, H, PW, P = 2400, 1800, 1000, 7
    title, sub1, sub2 = text("Bimfo", 4), text("Live departures"), text("Wiener Linien + ÖBB")
    foot1, foot2 = text("1:15 replica"), text("3D print + ESP32-S3")
    x = 9
    blocks = [[title], [sub1, sub2], [tram()], [foot1, foot2]]
    gaps = [12, 30, 30]
    height = sum(size(r)[1] + 3 for b in blocks for r in b) - 3 * len(blocks) + sum(gaps)
    y = (H // P - height) // 2
    items = []
    for n, block in enumerate(blocks):
        for rows in block:
            items.append((rows, x, y))
            y += size(rows)[1] + 3
        y += (gaps[n] - 3) if n < len(gaps) else 0
    for rows, ix, _ in items:
        assert ix + size(rows)[0] <= PW // P - 3, "too wide"
    img = Image.new("RGB", (W, H))
    img.paste(panel(PW, H, P, items), (0, 0))
    img.paste(place(im, (0.10, 0.20, 0.90, 0.955), W - PW, H), (PW, 0))
    save(img, "cover_web.jpg")


def mobile(im):
    # 1800 x 2400: the panel at the top, over the bare wall, the photo below
    W, H, PH, P = 1800, 2400, 570, 6
    title, sub = text("Bimfo", 3), text("Live departures", 2)
    small = text("Wiener Linien + ÖBB  ·  1:15 replica")
    tw, th = size(title)
    x, y = 12, 8
    t = tram()
    items = [(title, x, y), (t, W // P - 12 - size(t)[0], y + th - size(t)[1] + 2)]
    y += th + 8
    items.append((sub, x, y))
    y += size(sub)[1] + 7
    items.append((small, x, y))
    img = Image.new("RGB", (W, H))
    img.paste(panel(W, PH, P, items), (0, 0))
    img.paste(place(im, (0.0, 0.2, 1.0, 0.9625), W, H - PH), (0, PH))
    save(img, "cover_mobile.jpg")


if __name__ == "__main__":
    im = photo()
    web(im)
    mobile(im)
