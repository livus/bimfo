# Bimfo – renders

Cycles renders of the Bimfo model for the MakerWorld page and the READMEs, written to
`../img/makerworld/`.

```powershell
.\make_renders.ps1 -Blender "C:\path\to\blender.exe"                      # all shots, 256 samples
.\make_renders.ps1 -Blender "C:\path\to\blender.exe" -Samples 32 -Shots hero   # quick check
```

Needs OpenSCAD and Blender 4.5, and the UI frames `host/out/wl_burgtheater@3x.png`,
`compact_grouped@3x.png` and `oebb_delays@3x.png` from the firmware's host renderer
(`host/render.c`). On Windows, keep Blender in a short path: unpacked deep inside a temp folder
it fails to start (DLL init error 0xC0000142) because its files exceed the 260-character path
limit.

| Shot | What |
|---|---|
| `hero` | Front three-quarter view, display on, Vienna tram stop behind |
| `display` | Close-up of the screen |
| `styles` | Three models in a row, slightly turned, showing the Wiener Linien, Kompakt and ÖBB styles |
| `back` | From behind: mock back window, speakers, bin on the railing |
| `detail` | The bin, railing and track |
| `night` | Night look on a dark backdrop |
| `exploded`, `exploded_night` | Parts pulled apart along their assembly direction, board between shell and back cover |
| `static`, `static_back` | The model without electronics: `screen_dummy` and the closed back |

How it works:

- `export.scad` includes the model and exports every part in its assembled position into
  `build/stl/`, and echoes the few dimensions `render.py` needs (written to `build/params.json`).
- `render.py` imports the parts, gives them PLA-like materials, puts the real UI frame on the
  screen and renders each shot. The daylight shots render the model on a transparent background,
  lit by an HDRI, with its shadow from a shadow-catcher floor; the compositor lays it over the
  blurred photo, so the photo keeps its own colours.

Covers: `py -3 make_covers.py` (needs Pillow and numpy) writes `cover_web.jpg` (4:3, 2400×1800)
and `cover_mobile.jpg` (3:4, 1800×2400) from the print photo `front_1.jpg`: the photo with a
little more contrast and colour, beside or below a panel drawn like the stop display, amber LEDs
in the firmware's Wiener Linien face (`tools/wl_glyphs.py`) on its green glow, with the tram of
the start screen.

Background assets (downloaded on first run into `assets/`, both CC0, no attribution required):

- Photo: [Vienna tram line 12 at stop Hillerstrasse 2025-09-01 b](https://commons.wikimedia.org/wiki/File:Vienna_tram_line_12_at_stop_Hillerstrasse_2025-09-01_b.jpg), Wikimedia Commons
- HDRI: [Zawiszy Czarnego](https://polyhaven.com/a/zawiszy_czarnego) by Grzegorz Wronkowski, Poly Haven
