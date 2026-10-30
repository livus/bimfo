#!/usr/bin/env python3
"""Writes matrix.tex: the display decals, drawn like the firmware's Wiener Linien
style (main/ui/ui_theme_wl.c) with its LED faces (tools/wl_glyphs.py): up to three
entries with line and minutes in big digits, a wheelchair for low-floor trams, the
destination on one or two rows with the U-Bahn symbol, and the square for a tram
leaving now; on the green glow of the panel, split into three modules by seams.
One macro per station: \\ledmatrixA{x}{y}, \\ledmatrixB ...

Run: python make_matrix.py   (then xelatex decals.tex)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import gen_fonts  # noqa: E402

# The firmware's panel of 160 x 86 LEDs has the ratio of the display's lit area,
# 32.35 x 17.39 mm, so the decal shows the LEDs one to one.
W, H = 160, 86
AREA_W = 32.35
PITCH = AREA_W / W

# Layout of ui_theme_wl.c, in LEDs.
ENTRY_Y0, ENTRY_H, ROW2_DY = 4, 28, 11
X_LINE, LINE_W, X_DEST, X_MIN, MIN_W = 1, 30, 33, 136, 22
DEST_W = X_MIN - 3 - X_DEST
NOW_X = X_MIN + 2                       # the square's position in the bottom row
SEAMS = [W * i // 3 for i in (1, 2)]

U = " Ⓤ"                           # U-Bahn symbol after a destination
WHEELCHAIR, NOW = "♿", "■"

# One platform each, trams only. A display shows only the next tram of each line, so
# each line appears once; unused entries stay dark. min: None = leaving now (the square).
# A is the reference photo (Wiedner Hauptstrasse, before line 1 was rerouted); the
# others are real departures from the Wiener Linien realtime API on 27 Oct 2026. Where
# a destination is split over two rows, the row break replaces the comma, as on the photo.
VARIANTS = [
    {"id": "A", "name": "Paulanergasse", "note": "as in the reference photo", "rows": [
        {"line": "1", "ulf": True, "dest": ["Stefan-Fadinger", "Platz"], "min": None},
        {"line": "62", "ulf": False, "dest": ["Lainz", "Wolkersbergenstr"], "min": "5"},
    ]},
    {"id": "B", "name": "Schottentor U", "note": "the Jonas-Reindl, lines 37 to 42", "rows": [
        {"line": "38", "ulf": True, "dest": ["Grinzing"], "min": None},
        {"line": "40", "ulf": True, "dest": ["Herbeckstraße"], "min": "4"},
        {"line": "37", "ulf": True, "dest": ["Hohe Warte"], "min": "5"},
    ]},
    {"id": "C", "name": "Oper, Karlsplatz U", "note": "Ring, towards Burgtheater", "rows": [
        {"line": "2", "ulf": True, "dest": ["Dornbach"], "min": "1"},
        {"line": "D", "ulf": True, "dest": ["Börse"], "min": "2"},
        {"line": "1", "ulf": True, "dest": ["Prater", "Hauptallee"], "min": "3"},
    ]},
    {"id": "D", "name": "Rathausplatz, Burgtheater", "note": "Ring, towards Oper", "rows": [
        {"line": "71", "ulf": True, "dest": ["Kaiserebersdorf", "Zinnergasse"], "min": "2"},
        {"line": "1", "ulf": True, "dest": ["Reumannplatz" + U], "min": "3"},
        {"line": "D", "ulf": True, "dest": ["Absberggasse"], "min": "5"},
    ]},
    {"id": "E", "name": "Zentralfriedhof 2. Tor", "note": "at the main gate", "rows": [
        {"line": "11", "ulf": True, "dest": ["Otto-Probst-", "Platz"], "min": "2"},
        {"line": "71", "ulf": True, "dest": ["Schottenring" + U], "min": "4"},
    ]},
]

TEXT = gen_fonts.wl_text_glyphs()
BIG = gen_fonts.wl_big_glyphs()


def glyph(face, ch):
    rows = face.get(ord(ch))
    if rows is None:
        raise SystemExit('no glyph for "%s"' % ch)
    cols = [c for c in range(max((len(r) for r in rows), default=0))
            if any(c < len(r) and r[c] == "#" for r in rows)]
    return rows, cols


def advance(face, ch):
    # as in the LVGL fonts: the glyph plus one dark LED; a space is three LEDs
    _, cols = glyph(face, ch)
    return cols[-1] - cols[0] + 2 if cols else 3


def width(face, s):
    return sum(advance(face, ch) for ch in s)


def draw(lit, face, s, x, y):
    for ch in s:
        rows, cols = glyph(face, ch)
        for r, row in enumerate(rows):
            for c in cols:
                if c < len(row) and row[c] == "#":
                    lit.add((x + c - cols[0], y + r))
        x += advance(face, ch)
    return x


def render(rows):
    if len(rows) > 3:
        raise SystemExit("at most three entries fit")
    lit = set()
    for n, r in enumerate(rows):
        y = ENTRY_Y0 + n * ENTRY_H
        y2 = y + ROW2_DY
        # big digits, a trailing letter small ("13A"), as in the firmware
        line = r["line"]
        cut = len(line)
        while cut and line[cut - 1].isalpha():
            cut -= 1
        if not any(ch.isdigit() for ch in line[:cut]):
            cut = len(line)
        x = draw(lit, BIG, line[:cut], X_LINE, y)
        x = draw(lit, TEXT, line[cut:], x, y2)
        if x - X_LINE > LINE_W:
            raise SystemExit('line "%s" is too wide' % line)
        if r["ulf"] and draw(lit, TEXT, WHEELCHAIR, x, y2) > X_DEST:
            raise SystemExit('no room for the wheelchair after "%s"' % line)
        for i, d in enumerate(r["dest"]):
            if width(TEXT, d) > DEST_W:
                raise SystemExit('"%s" runs into the minutes' % d)
            draw(lit, TEXT, d, X_DEST, y + i * ROW2_DY)
        if r["min"] is None:
            draw(lit, TEXT, NOW, NOW_X, y2)
        else:
            draw(lit, BIG, r["min"], X_MIN + MIN_W - width(BIG, r["min"]), y)
    return lit


def tex_name(s):
    return s.replace("ß", "\\ss{}").replace("ö", '\\"o').replace("ä", '\\"a').replace("ü", '\\"u')


def main():
    # TikZ: \ledmatrixX{x}{y} draws the lit area with its top left at (x, y) mm, y running down
    rad = PITCH * 0.36
    area_h = H * PITCH
    out = ["%% generated by make_matrix.py; %d x %d LEDs, pitch %.4f mm" % (W, H, PITCH)]
    preview = []
    for v in VARIANTS:
        lit = render(v["rows"])
        out.append("%% %s: %s (%s)" % (v["id"], v["name"], v["note"]))
        out.append("\\newcommand{\\ledmatrix%s}[2]{\\begin{scope}[shift={(#1,#2)}]" % v["id"])
        # the backlit panel, brightest in the middle
        out.append("  \\shade[inner color=ledglowcentre, outer color=ledglowedge] (0,0) rectangle (%s,%.3f);"
                   % (AREA_W, area_h))
        # unlit LEDs, just visible: one dashed line per row, zero-length dashes with round caps
        out.append("  \\draw[ledon, draw opacity=0.10, line width=%.4fmm, line cap=round, dash pattern=on 0mm off %.5fmm]"
                   % (2 * rad, PITCH))
        for j in range(H):
            yy = (j + 0.5) * PITCH
            out.append("    (%.4f,%.4f) -- (%.4f,%.4f)" % (PITCH / 2, yy, AREA_W - PITCH / 4, yy))
        out.append("  ;")
        # lit LEDs the same way, one dashed line per run of LEDs (TeX runs out of memory
        # with a circle per LED)
        out.append("  \\draw[ledon, line width=%.4fmm, line cap=round, dash pattern=on 0mm off %.5fmm]"
                   % (2 * rad, PITCH))
        for y in range(H):
            x = 0
            while x < W:
                if (x, y) not in lit:
                    x += 1
                    continue
                x0 = x
                while (x, y) in lit:
                    x += 1
                yy = (y + 0.5) * PITCH
                out.append("    (%.4f,%.4f) -- (%.4f,%.4f)" % ((x0 + 0.5) * PITCH, yy, (x - 0.25) * PITCH, yy))
        out.append("  ;")
        # seams between the three modules, one LED wide, over the LEDs as on the real display
        for c in SEAMS:
            out.append("  \\fill[ledseam] (%.4f,0) rectangle (%.4f,%.3f);" % (c * PITCH, (c + 1) * PITCH, area_h))
        out.append("\\end{scope}}")
        out.append("\\newcommand{\\stationname%s}{%s}" % (v["id"], tex_name(v["name"])))
        out.append("\\newcommand{\\stationnote%s}{%s}" % (v["id"], v["note"]))
        out.append("\\newcommand{\\stationlines%s}{%s}" % (v["id"], ", ".join(r["line"] for r in v["rows"])))

        preview.append("%s: %s" % (v["id"], v["name"]))
        preview += ["".join("#" if (x, y) in lit else "." for x in range(W)) for y in range(H)]
        print("%s %s: %d lit LEDs" % (v["id"], v["name"], len(lit)))
    out.append("\\let\\ledmatrix\\ledmatrixA")
    with open(os.path.join(HERE, "matrix.tex"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    with open(os.path.join(HERE, "matrix_preview.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(preview) + "\n")


if __name__ == "__main__":
    main()
