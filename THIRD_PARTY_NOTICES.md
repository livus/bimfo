# Third-party notices

Bimfo's own code is MIT ([LICENSE](LICENSE)) and the enclosure is CC BY-NC-SA 4.0
([enclosure/LICENSE](enclosure/LICENSE)). This file lists what Bimfo uses from others, under
their own terms. It is also the notice that goes with every firmware image built from this
repository.

## In the firmware

| Component | Where | Licence |
|---|---|---|
| ESP-IDF 5.5 (Espressif Systems) | build framework, not in the repository | Apache-2.0 |
| espressif/mdns 1.x | fetched by the component manager | Apache-2.0 |
| LVGL 8.3 (LVGL Kft) | fetched by the component manager | MIT, notice below |
| ST7789T panel driver (Espressif Systems, as adapted in the Waveshare demo) | `main/platform/panel_st7789t.c/.h` | Apache-2.0, header in the files |
| Liberation Sans 2.x (Google, Red Hat) | `tools/fonts/`, converted to `main/ui/fonts/font_sans_*.c` | SIL OFL 1.1, `tools/fonts/LiberationSans-LICENSE.txt` |
| Adafruit GFX 5×7 font (Adafruit Industries) | `tools/fonts/adafruit_glcdfont.c`, converted to `main/ui/fonts/font_led_*.c` | BSD, notice below |

The Apache License 2.0 is at <https://www.apache.org/licenses/LICENSE-2.0>. The Liberation
fonts are converted to LVGL bitmap fonts for the display only; they are not offered as fonts
and not named "Liberation".

### LVGL

```
MIT licence
Copyright (c) 2021 LVGL Kft

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

### Adafruit GFX font

```
Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

## Data

- **Wiener Linien realtime data and station list:** Stadt Wien – data.wien.gv.at, licensed
  under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The firmware queries it live;
  `host/fixtures/wl_*` are captured responses and `wl_haltestellen.csv` is the official stop
  list, unchanged. The display decals show departures taken from it.
- **ÖBB:** the firmware queries ÖBB's Scotty/HAFAS station board, an unofficial interface with no
  open licence. `host/fixtures/oebb_*` are three small captured responses, kept only to test the
  parser.

## In the enclosure

- **Fonts:** `enclosure/decals/fonts/` holds Liberation Sans (SIL OFL 1.1, licence next to the
  fonts). The LED matrix on the decal sheet uses the Adafruit GFX 5×7 font (BSD, above).
- **Render backgrounds** (downloaded by `enclosure/render/make_renders.ps1`, not in the
  repository), both CC0:
  - Photo: "Vienna tram line 12 at stop Hillerstrasse 2025-09-01 b", Wikimedia Commons.
  - HDRI: "Zawiszy Czarnego" by Grzegorz Wronkowski, Poly Haven.
- **What the model depicts:** the stop display, the litter bin's texts and the dog sticker
  follow street furniture of Wiener Linien and the City of Vienna. No logos are used. Bimfo is an
  independent project and not affiliated with or endorsed by Wiener Linien, the City of Vienna or
  ÖBB; their names are used only to describe what the model shows and where the data comes from.
