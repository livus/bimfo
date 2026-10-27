#!/usr/bin/env python3
"""Generates the LVGL fonts in main/ui/fonts/.

* font_sans_*: Liberation Sans (SIL OFL 1.1, metric-compatible with Arial, the
  face of the OEBB departure monitors) converted with lv_font_conv (needs node).
* font_led_*: amber dot-matrix look of the Wiener Linien stop displays, built
  from the classic 5x7 CP437 font of Adafruit GFX (BSD) with every source pixel
  drawn as a textured "LED".

Usage:  python tools/gen_fonts.py [--led-only]
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "fonts")
OUT = os.path.join(ROOT, "main", "ui", "fonts")

LATIN = "0x20-0x7E,0xA0-0xFF,0x2013,0x2014,0x201C,0x201E,0x2026,0x2192"

SANS = [
    ("font_sans_12", "LiberationSans-Regular.ttf", 12),
    ("font_sans_14", "LiberationSans-Regular.ttf", 14),
    ("font_sans_16", "LiberationSans-Regular.ttf", 16),
    ("font_sans_bold_14", "LiberationSans-Bold.ttf", 14),
    ("font_sans_bold_16", "LiberationSans-Bold.ttf", 16),
    ("font_sans_bold_22", "LiberationSans-Bold.ttf", 22),
    ("font_sans_italic_12", "LiberationSans-Italic.ttf", 12),
]

# (name, scale, 4-bit alpha texture of one LED, row-major scale x scale)
LED = [
    ("font_led_16", 2, [[15, 12], [12, 7]]),
    ("font_led_24", 3, [[15, 15, 5], [15, 15, 5], [5, 5, 2]]),
]

# Unicode -> CP437 code (or an ASCII fallback) for the 5x7 table.
CP437 = {
    0xA0: 0x20, 0xA1: 0xAD, 0xA2: 0x9B, 0xA3: 0x9C, 0xA5: 0x9D, 0xA6: 0x7C, 0xA7: 0x15,
    0xA8: 0x22, 0xAA: 0xA6, 0xAB: 0xAE, 0xAC: 0xAA, 0xAD: 0x2D, 0xAF: 0x2D, 0xB0: 0xF8,
    0xB1: 0xF1, 0xB2: 0xFD, 0xB3: 0x33, 0xB4: 0x27, 0xB5: 0xE6, 0xB6: 0x14, 0xB7: 0xFA,
    0xB8: 0x2C, 0xB9: 0x31, 0xBA: 0xA7, 0xBB: 0xAF, 0xBC: 0xAC, 0xBD: 0xAB, 0xBF: 0xA8,
    0xC4: 0x8E, 0xC5: 0x8F, 0xC6: 0x92, 0xC7: 0x80, 0xC9: 0x90, 0xD1: 0xA5, 0xD6: 0x99,
    0xD7: 0x78, 0xDC: 0x9A, 0xDF: 0xE1, 0xE0: 0x85, 0xE1: 0xA0, 0xE2: 0x83, 0xE4: 0x84,
    0xE5: 0x86, 0xE6: 0x91, 0xE7: 0x87, 0xE8: 0x8A, 0xE9: 0x82, 0xEA: 0x88, 0xEB: 0x89,
    0xEC: 0x8D, 0xED: 0xA1, 0xEE: 0x8C, 0xEF: 0x8B, 0xF1: 0xA4, 0xF2: 0x95, 0xF3: 0xA2,
    0xF4: 0x93, 0xF6: 0x94, 0xF7: 0xF6, 0xF9: 0x97, 0xFA: 0xA3, 0xFB: 0x96, 0xFC: 0x81,
    0xFF: 0x98, 0x2013: 0x2D, 0x2014: 0x2D, 0x201C: 0x22, 0x201E: 0x22, 0x2192: 0x1A,
}
# Base letters for U+00C0..U+00FF without a CP437 glyph (one char per code point).
ASCII_FALLBACK = "AAAAAAACEEEEIIII" "DNOOOOOxOUUUUYPs" "aaaaaaaceeeeiiii" "dnooooo/ouuuuypy"
assert len(ASCII_FALLBACK) == 64

# Extra glyphs drawn by hand, rows top to bottom (row 7 is the descender row).
CUSTOM = {
    0x2026: [".....", ".....", ".....", ".....", ".....", ".....", "#.#.#", "....."],
    0x267F: [  # wheelchair (barrier-free vehicle)
        ".#....",
        "......",
        ".##...",
        ".#....",
        ".####.",
        "#...#.",
        "#...##",
        ".###..",
    ],
}


def load_glcd():
    text = open(os.path.join(SRC, "adafruit_glcdfont.c"), encoding="utf-8").read()
    body = text[text.index("{") + 1:text.index("};")]
    data = [int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]{2}", body)]
    assert len(data) == 256 * 5, len(data)
    return [data[i * 5:i * 5 + 5] for i in range(256)]


def rows_to_cols(rows):
    cols = []
    for c in range(len(rows[0])):
        v = 0
        for r, row in enumerate(rows):
            if row[c] == "#":
                v |= 1 << r
        cols.append(v)
    return cols


def led_columns(cp, table):
    if cp in CUSTOM:
        return rows_to_cols(CUSTOM[cp])
    if 0x20 <= cp < 0x7F:
        return table[cp]
    if cp in CP437:
        return table[CP437[cp]]
    if 0xC0 <= cp <= 0xFF:
        return table[ord(ASCII_FALLBACK[cp - 0xC0])]
    return table[ord("?")]


def led_codepoints():
    cps = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100))
    cps += [0x2013, 0x2014, 0x201C, 0x201E, 0x2026, 0x2192, 0x267F]
    return cps


def pack_bits(values, bpp):
    out, acc, nbits = [], 0, 0
    for v in values:
        acc = (acc << bpp) | v
        nbits += bpp
        while nbits >= 8:
            nbits -= 8
            out.append((acc >> nbits) & 0xFF)
    if nbits:
        out.append((acc << (8 - nbits)) & 0xFF)
    return out


def gen_led(name, scale, texture):
    table = load_glcd()
    cps = led_codepoints()
    bitmap, glyphs = [], []
    for cp in cps:
        cols = led_columns(cp, table)
        used = [i for i, c in enumerate(cols) if c]
        if chr(cp).isdigit():
            used = list(range(5))  # fixed-width digits keep minute columns aligned
        if not used:
            adv_cols = 3 if cp in (0x20, 0xA0) else 2
            glyphs.append((cp, len(bitmap), adv_cols * scale * 16, 0, 0, 0, 0))
            continue
        c0, c1 = min(used), max(used)
        rows_used = [r for r in range(8) if any(cols[c] >> r & 1 for c in range(c0, c1 + 1))]
        r0, r1 = min(rows_used), max(rows_used)
        w, h = (c1 - c0 + 1) * scale, (r1 - r0 + 1) * scale
        px = []
        for y in range(h):
            r, ty = r0 + y // scale, y % scale
            for x in range(w):
                c, tx = c0 + x // scale, x % scale
                px.append(texture[ty][tx] if cols[c] >> r & 1 else 0)
        index = len(bitmap)
        bitmap += pack_bits(px, 4)
        adv = (c1 - c0 + 2) * scale * 16
        # Baseline sits below source row 6; row 7 holds descenders.
        glyphs.append((cp, index, adv, w, h, 0, (6 - r1) * scale))

    lines = [
        "/* Generated by tools/gen_fonts.py - do not edit.",
        " * LED dot-matrix font, %d px, based on the 5x7 font of Adafruit GFX" % (8 * scale),
        " * (Copyright (c) 2012 Adafruit Industries, BSD license, see tools/fonts). */",
        "",
        '#include "lvgl.h"',
        "",
        "static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {",
    ]
    for i in range(0, len(bitmap), 16):
        lines.append("    " + ", ".join("0x%02x" % b for b in bitmap[i:i + 16]) + ",")
    lines += ["};", "", "static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {",
              "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},"]
    for cp, idx, adv, w, h, ox, oy in glyphs:
        lines.append("    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d}, /* U+%04X */"
                     % (idx, adv, w, h, ox, oy, cp))
    lines.append("};")

    # cmaps: two contiguous Latin ranges, then a sparse list for the extras.
    extras = cps[95 + 96:]
    lines += ["", "static const uint16_t unicode_list_extra[] = {"]
    lines.append("    " + ", ".join("0x%x" % (cp - extras[0]) for cp in extras))
    lines += ["};", "", "static const lv_font_fmt_txt_cmap_t cmaps[] = {",
              "    {.range_start = 32, .range_length = 95, .glyph_id_start = 1, .unicode_list = NULL,",
              "     .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},",
              "    {.range_start = 160, .range_length = 96, .glyph_id_start = 96, .unicode_list = NULL,",
              "     .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},",
              "    {.range_start = %d, .range_length = %d, .glyph_id_start = 192, .unicode_list = unicode_list_extra,"
              % (extras[0], extras[-1] - extras[0] + 1),
              "     .glyph_id_ofs_list = NULL, .list_length = %d, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}," % len(extras),
              "};", "",
              "static lv_font_fmt_txt_glyph_cache_t cache;",
              "static const lv_font_fmt_txt_dsc_t font_dsc = {",
              "    .glyph_bitmap = glyph_bitmap,",
              "    .glyph_dsc = glyph_dsc,",
              "    .cmaps = cmaps,",
              "    .kern_dsc = NULL,",
              "    .kern_scale = 0,",
              "    .cmap_num = 3,",
              "    .bpp = 4,",
              "    .kern_classes = 0,",
              "    .bitmap_format = 0,",
              "    .cache = &cache,",
              "};", "",
              "const lv_font_t %s = {" % name,
              "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,",
              "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,",
              "    .line_height = %d," % (8 * scale),
              "    .base_line = %d," % scale,
              "    .subpx = LV_FONT_SUBPX_NONE,",
              "    .underline_position = 0,",
              "    .underline_thickness = 0,",
              "    .dsc = &font_dsc,",
              "};", ""]
    with open(os.path.join(OUT, name + ".c"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def gen_sans(name, ttf, size):
    out = os.path.join(OUT, name + ".c")
    cmd = ["npx", "-y", "lv_font_conv@1.5.2", "--font", os.path.join(SRC, ttf), "-r", LATIN,
           "--size", str(size), "--bpp", "4", "--format", "lvgl", "--no-compress",
           "--lv-include", "lvgl.h", "-o", out]
    subprocess.run(cmd, check=True, shell=(os.name == "nt"))
    text = open(out, encoding="utf-8").read()
    # Drop the absolute path lv_font_conv records in the header comment.
    text = re.sub(r" \* Opts: .*\n", " * Opts: Liberation Sans %s, %d px, bpp 4 (tools/gen_fonts.py)\n" % (ttf, size), text)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def gen_header():
    names = [n for n, _, _ in SANS] + [n for n, _, _ in LED]
    lines = ["/* Generated by tools/gen_fonts.py - do not edit. */", "#pragma once", "",
             '#include "lvgl.h"', ""]
    lines += ["LV_FONT_DECLARE(%s)" % n for n in names]
    lines += ["", "/* Code point of the wheelchair glyph (LED fonts only). */",
              '#define LED_SYMBOL_WHEELCHAIR "\\xE2\\x99\\xBF"', ""]
    with open(os.path.join(OUT, "fonts.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, scale, tex in LED:
        gen_led(name, scale, tex)
    if "--led-only" not in sys.argv:
        for name, ttf, size in SANS:
            gen_sans(name, ttf, size)
    gen_header()


if __name__ == "__main__":
    main()
