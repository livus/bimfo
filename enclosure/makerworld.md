# MakerWorld page

Text for the MakerWorld model page and its four print profiles. Copy the sections into the
matching fields.

---

## Model name

Bimfo – Vienna tram stop display (Wiener Linien) with live departures, ESP32-S3

## Summary

A 1:15 replica of the classic Wiener Linien "Linie … Abfahrt in Minuten" stop display, with live
tram, bus, U-Bahn and train departures on a Waveshare ESP32-S3-LCD-1.47B, or as a static model.

## Licence

CC BY-NC-SA 4.0 (choose it in the upload form, so it matches GitHub).

## Tags

Bimfo, Wiener Linien, Vienna, Wien, tram, Straßenbahn, Bim, Haltestelle, departure board,
Abfahrtsmonitor, Echtzeitanzeige, ESP32, ESP32-S3, Waveshare, ÖBB, desk toy

## Description

**Bimfo** is a desk-sized model of the grey departure display at Vienna's tram and bus stops: the
"Linie … Abfahrt in Minuten" box on its pole, with the band brackets, the two horn loudspeakers
and the red cap, standing on a piece of tram platform and track with the tube railing and the
grey-and-orange Mistkübel.

With a **Waveshare ESP32-S3-LCD-1.47B** inside, it is a working *Abfahrtsmonitor*: real-time
departures from Wiener Linien (tram, bus, U-Bahn, night bus) and ÖBB (S-Bahn and trains) for the
stops you choose, in the amber LED look of the original or in the style of the ÖBB station
monitors. You set it up from your phone; no app, no account. Without the board, it prints as a
static model with a switched-off display, or a lit one from the decal sheet.

### Which profile do I need?

| | Multicolour (AMS) | Single colour (any printer) |
|---|---|---|
| **Live display** (ESP32 board) | **A** | **C** |
| **Static model** | **B** | **D** |

- **A · Multicolour · live display:** the black text, the back window and the bin's orange are
  printed in. Needs the board and a 90° USB-C cable.
- **B · Multicolour · static model:** as A, with a black display stand-in and a closed back. No
  electronics.
- **C · Single colour · live display:** all grey (plus a red cap); text, back window and the bin's
  orange come from the decal sheet. Needs the board and a 90° USB-C cable.
- **D · Single colour · static model:** as C, with the display stand-in and a closed back. No
  electronics.

Step-by-step instructions for each version, with only what that version needs:
**https://github.com/livus/bimfo/blob/main/enclosure/README.md** (sections A to D).

### Print notes

- Bambu Lab A1 mini or larger (180 mm bed), PLA, no supports.
- **Nozzle:** 0.4 mm prints every part. The small black text "Linie" / "Abfahrt in Minuten"
  (1.7 mm capitals) is only legible with a **0.2 mm** nozzle. With 0.4 mm, print the text in the
  box colour as well (the face stays flat) and use the label decal.
- Print the box face down on the textured PEI plate for the matte, powder-coated look.
- Bin colours (multicolour profiles): filament change to orange at 6.0 mm, grey at 13.0 mm, orange
  at 43.3 mm (set in the A and B profiles).
- Glue (CA or PLA glue) and a few short pieces of 1.75 mm filament as dowels and pins.

### For the live display

- Waveshare ESP32-S3-LCD-1.47B (with or without pin headers).
- A 90° (left/right side-angle) USB-C cable, cable Ø ≤ 4.4 mm, head about 11 × 17.5 × 7.2 mm
  (for example Lindy 32005). Two small test prints check the board and the cable before the big
  parts.
- Firmware: open source, installed from the browser in a minute (Chrome or Edge, no software
  needed): **https://livus.github.io/bimfo/**. Source and instructions: **https://github.com/livus/bimfo**.

### Decals

A two-page A4 PDF, in the files here or as a direct download:
**https://github.com/livus/bimfo/raw/main/enclosure/decals/decals.pdf**. It has the box labels for clear decal film or plain
sticker paper, a lit LED display in five variants for real Vienna tram stops (Schottentor, Oper,
Rathausplatz, Zentralfriedhof, Paulanergasse), the bin's "Misttelefon" band, slogan stickers and a
dog sticker. Print at 100 % and check the 50 mm scale bar.

### Licence and credits

- Licence: **CC BY-NC-SA 4.0**. Non-commercial use;
  selling prints or kits needs permission. Firmware on GitHub: MIT.
- Background photo: "Vienna tram line 12 at stop Hillerstrasse", Wikimedia Commons, CC0. HDRI
  "Zawiszy Czarnego" by Grzegorz Wronkowski, Poly Haven, CC0.
- Departure data: Wiener Linien / Stadt Wien – data.wien.gv.at (CC BY 4.0); ÖBB.
- Fonts: Liberation Sans (SIL OFL 1.1); LED font after Adafruit GFX 5×7 (BSD).
- Bimfo is an independent fan project, not affiliated with Wiener Linien or ÖBB.

---

## Print profiles

### A · Multicolour · live display

Plates: box (`shell_with_labels`, grey + black), back cover with header slits (`back_with_labels`,
grey + black), pole, pole cover, pole top, horns ×2, speaker plate, base, base strip, railing
halves ×2, bin with filament changes (grey + orange), cap (red). Fit test and cable test on their
own plate. Filaments: light grey, black, red, orange. Nozzle 0.2 mm for the text plates.

### B · Multicolour · static model

As A, but the closed back cover (`back_closed_with_labels`) and the black display stand-in
(`screen_dummy`) instead of the back with slits; no fit or cable test. Filaments: light grey, black,
red, orange.

### C · Single colour · live display

As A, with the text and window parts mapped to light grey and the bin in plain grey. Fit test and
cable test on their own plate. Filaments: light grey, red; black or dark grey for the speaker plate
(optional). Any nozzle. Decal sheet needed.

### D · Single colour · static model

As C, with the closed back cover and the black display stand-in; no fit or cable test. Filaments:
light grey, red, black (stand-in). Any nozzle. Decal sheet needed.
