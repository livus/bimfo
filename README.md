# Bimfo

**Live tram, bus, U-Bahn and train departures for Vienna, in a 3D-printed replica of the classic
Wiener Linien stop display.**

Bimfo is an open-source departure monitor (*Abfahrtsmonitor*, *Echtzeitanzeige*) for the Waveshare
**ESP32-S3-LCD-1.47B**. It shows real-time departures from the Wiener Linien and ÖBB APIs for the
stops you choose, set up from your phone through a page the device serves itself. The printable
enclosure is a 1:15 model of the grey "Linie … Abfahrt in Minuten" box at Vienna's *Bim* (tram) and
bus stops, on its pole with the loudspeakers and red cap, standing on a piece of platform with the
railing and the Mistkübel. Without the electronics, it prints as a static model.

![Bimfo at a Vienna tram stop](enclosure/img/makerworld/hero.png)

## Which version do I need?

| | **Multicolour** (AMS) | **Single colour** (any printer, decals for the details) |
|---|---|---|
| **Live display** (ESP32 board + 90° USB-C cable) | [**A**: Multicolour, live display](enclosure/README.md#a-multicolour-live-display) | [**C**: Single colour, live display](enclosure/README.md#c-single-colour-live-display) |
| **Static model** (no electronics) | [**B**: Multicolour, static model](enclosure/README.md#b-multicolour-static-model) | [**D**: Single colour, static model](enclosure/README.md#d-single-colour-static-model) |

Each link goes to a short section with that version's parts, filaments, decals and steps. The
four versions are also print profiles on MakerWorld. A 0.4 mm nozzle prints everything; only the
tiny black text on the box needs a 0.2 mm nozzle in multicolour, and the [decal sheet](enclosure/decals/decals.pdf)
replaces it otherwise.

For the live display (A or C), this page covers the firmware: [installing it](#installing-the-firmware)
and [using it](#using-it).

## What the display shows

Two styles, switchable on the settings page or with the BOOT button:

| Wiener Linien | ÖBB |
|---|---|
| ![Wiener Linien style](docs/img/wl.png) | ![ÖBB style](docs/img/oebb.png) |
| Yellow-amber LED dot-matrix on the faint glow of a backlit panel, split into three modules by dark seams, like the stop displays: line, destination, minutes, a blinking `*` when due, a wheelchair for low-floor trams and buses. | Colours, columns and fonts of the ÖBB station monitors: Zeit / Erwartet / Zug / Nach / Steig, delays in yellow, a white notice strip. |

- Any mix of Wiener Linien stops (tram, bus, U-Bahn, night bus) and ÖBB stations (S-Bahn,
  regional and long-distance trains).
- Optional grouping: one row per line with the next two departures.
- A walking time per stop hides departures you can no longer reach.
- Disruption notices as a scrolling ticker.
- Brightness, night mode, refresh interval, 180° rotation.
- Firmware updates from the settings page, with automatic rollback.

## Installing the firmware

**[Install from the browser](https://livus.github.io/bimfo/)**: open the page in Chrome or Edge
on a computer, connect the board with a USB-C cable and click *Install Bimfo*. No software needed. On
a new board, tick *Erase device* when asked; when updating, leave it unticked to keep the settings.

Every [release](../../releases/latest) also has the files for other ways:

| File | For |
|---|---|
| `bimfo-<version>-update.bin` | Updates over Wi-Fi: settings page → System → Firmware-Update. Keeps the settings. |
| `bimfo-<version>-full.bin` | A first install with esptool: `esptool.py --chip esp32s3 write_flash 0x0 bimfo-<version>-full.bin`. Erases the settings. |

To build it yourself, see [Building and flashing](#building-and-flashing).

## Using it

![setup screen](docs/img/setup.png)

1. **First start:** the display shows *WLAN-Setup* with a QR code. Join the open network
   `Bimfo-XXXX` (scan the code); the setup page opens by itself (captive portal), otherwise browse
   to `http://192.168.4.1`.
2. Enter your Wi-Fi and save. The device restarts and shows `http://bimfo.local` and its IP.
3. Open that address on any device in the same network and add stops:
   * **Wiener Linien:** search the station, tick lines and directions (live list); nothing ticked
     means all lines.
   * **ÖBB:** search the station, optionally "nur Züge" and a line filter (`S 50, REX 51`).
   * The walking time per stop hides departures you cannot reach any more.
4. Display settings: style, grouping, brightness, night mode, refresh interval, 180° rotation.

**BOOT button** (in the enclosure: the tab under the box, near the pole): short press switches the
style; hold 3 s to open the setup Wi-Fi for 10 minutes, for example when moving to another network.
Firmware updates are uploaded on the page (System → Firmware-Update); a broken update rolls back
automatically.

> **Updating from an "Abfahrtsmonitor" build:** the project was renamed to Bimfo, and the device
> only accepts updates with its own project name. Install the first Bimfo firmware over USB, with
> the [browser installer](#installing-the-firmware) (without *Erase device*) or `idf.py -p COMx
> flash`; the settings are kept.

## Building and flashing

ESP-IDF 5.5 (the VS Code ESP-IDF extension works as-is). Dependencies (LVGL 8.3, mDNS) are fetched
by the component manager.

```
idf.py build
idf.py -p COM8 flash monitor
```

The partition table differs from the Waveshare demo (two OTA slots), so the first flash must be done
over USB. Optional for development: `idf.py menuconfig` → *Bimfo* → Wi-Fi credentials (stored only
in the untracked `sdkconfig`, used until Wi-Fi is set on the device).

### First flash on the hardware: checklist

Everything above the hardware glue is tested on the PC; these points can only be confirmed on the
board:

1. `idf.py -p COMx erase-flash flash monitor` (erase once: new partition table).
2. The boot screen appears in landscape, yellow-amber on a dark background. If red and blue look
   swapped, the panel colour order in `platform/lcd.c` (`rgb_endian`) is the place to look.
3. Setup screen: the QR code scans and joins `Bimfo-XXXX`; the phone pops up the page.
4. Save Wi-Fi → restart → `http://bimfo.local` is reachable; add a stop; departures appear within a
   few seconds; the log shows `fetch: N departures`.
5. Page → Anzeige → "Anzeige um 180° drehen": the picture turns and is still aligned (no offset
   stripe).
6. BOOT button: a short press switches the style, holding it 3 s brings back the setup screen.
7. The brightness slider and night mode change the backlight; 0 % turns it off.
8. Upload `build/bimfo.bin` on the page → it restarts into the other OTA slot (the status line shows
   `ota_1`).

### Releasing

The version is in `version.txt`; the settings page shows it. To release:

1. Raise `version.txt` (for example to `1.1.0`) and commit.
2. Tag and push: `git tag v1.1.0 && git push origin main v1.1.0`.

The [release workflow](.github/workflows/release.yml) builds the firmware with ESP-IDF 5.5.2,
publishes a GitHub release with the update and full images, the licence and the third-party
notices, and deploys the browser installer ([`web/`](web/index.html)) with the same firmware to
GitHub Pages. It stops if the tag doesn't match `version.txt`. One-time setup: Settings → Pages →
Source: *GitHub Actions*.

`python tools/release.py` (in an ESP-IDF shell, after `idf.py build`) makes the same files locally
in `dist/`. To try the installer: `python -m http.server -d dist/site 8000`, then open
`http://localhost:8000` in Chrome or Edge.

## Enclosure

The printable model lives in [`enclosure/`](enclosure/README.md): OpenSCAD source, STLs, Bambu
Studio projects, the decal sheet and the render scripts. Start with
[Which version do I need?](#which-version-do-i-need).

## Testing without the device

Everything that is not hardware glue builds on the PC (MinGW gcc or any C11 compiler, plus CMake):

```
cmake -S host -B host/build -G Ninja
cmake --build host/build
host/build/bimfo_tests     # unit tests against captured real API responses
host/build/bimfo_fuzz      # truncated/corrupted payloads, random DNS packets
host/build/bimfo_render    # PNG screenshots of all screens -> host/out/index.html
```

`bimfo_render` runs the exact UI code and LVGL version of the firmware against a frame buffer, so
layout changes can be checked in seconds.

`python host/live_check.py` fetches fresh responses for several large stations from the real
services (same URLs as the firmware) and runs them through the firmware parsers via `bimfo_parse`:
a quick way to notice API changes (`--save` refreshes the fixtures).

The configuration page can be developed against a fake device:

```
python host/mock_server.py            # fixtures; add --live to query the real services
cd host/webtest && npm install && npm test   # end-to-end test of the page (jsdom)
```

### Running the firmware in QEMU

The real firmware also boots in Espressif's QEMU (`idf_tools.py install qemu-xtensa`).
`sdkconfig.qemu` switches PSRAM to the emulated quad mode and replaces Wi-Fi by QEMU's `open_eth`
Ethernet, so the device fetches live data and serves its web API:

```
idf.py -B build_qemu -D SDKCONFIG=build_qemu/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.qemu" build
cd build_qemu && esptool.py --chip esp32s3 merge_bin --fill-flash-size 16MB -o qemu_flash.bin @flash_args
qemu-system-xtensa -nographic -machine esp32s3 -m 8M -drive file=qemu_flash.bin,if=mtd,format=raw \
    -nic user,model=open_eth,hostfwd=tcp::8090-:80
```

The configuration page is then at `http://localhost:8090/`. The LCD is not emulated (the UI still
renders, frames are dropped), and QEMU uses software AES/SHA because its crypto blocks cannot DMA
from PSRAM.

`python host/qemu_smoke.py` automates this: it boots the image, configures a Wiener Linien and an
ÖBB stop through the device's API, waits for the device's own HTTPS fetches, and checks the board,
heap, per-task stack head-room (`/api/status` → `stack_free`) and the serial log. It also uploads
the image as an OTA update and checks that the device restarts into the other slot with its
settings.

## Layout

```
main/core/      portable logic, host-tested: data model, WL + ÖBB parsers, config (JSON),
                Vienna time zone, text helpers, API JSON, captive DNS replies
main/ui/        LVGL screens and both themes; fonts/ is generated (tools/gen_fonts.py)
main/platform/  ESP-IDF glue: Wi-Fi/AP/captive portal, HTTPS fetch, fetch loop,
                web server + OTA, NVS config store, display task (backlight, button),
                LCD panel and LVGL port
main/www/       the configuration page (embedded into the firmware)
host/           render harness, tests, fuzzer, fixtures, mock server, page test
tools/          font generator and font sources (Liberation Sans OFL, Adafruit 5x7 BSD),
                release packager
web/            the browser installer (GitHub Pages)
enclosure/      the printable model, decal sheet and renders
```

Tasks: `display` (core 1, the only one touching LVGL), `fetch` (polls the stops), the HTTP server,
and the DNS responder while the setup AP is up. They exchange copies of the configuration and the
board through `platform/app_state.c` (one mutex).

## Data sources

* Wiener Linien OGD realtime API (`/ogd_realtime/monitor`, CC BY 4.0, Stadt Wien –
  data.wien.gv.at). Selected platforms are queried by RBL number, otherwise the whole station by
  DIVA. Station search uses the official `haltestellen.csv` (cached in PSRAM).
* ÖBB: the Scotty/HAFAS station board (`fahrplan.oebb.at/bin/stboard.exe`,
  `outputMode=tickerDataOnly`) and station search (`ajax-getstop.exe`), the same unofficial
  endpoints other open-source monitors use; they may change without notice.

Bimfo is an independent project, not affiliated with Wiener Linien or ÖBB.

## Robustness

* A stop whose request fails keeps showing its last good departures (up to 15 min), and the header
  shows *Offline – Stand HH:MM*; retries back off from 15 s to 5 min (longer when Wiener Linien
  reports its rate limit, code 316).
* Every stop gets an equal share of the 64 board slots, with at most 4 departures per line and
  direction, so a large station cannot crowd out the others.
* Countdown and clock are computed locally from the departure timestamps, so they keep ticking
  between fetches; until SNTP syncs, the API's server time sets the clock.
* Wi-Fi reconnects with backoff; if the network stays away for 30 s, the setup AP opens (the station
  keeps retrying, but pauses while a phone is connected to the AP).
* Firmware uploads are checked (image header, project name, checksum) before switching, and a new
  firmware is only kept after it has run its UI for 30 s.

## Licence

* **Firmware, host tools and scripts:** [MIT](LICENSE).
* **The enclosure** (everything in `enclosure/`: model, STL/3MF files, images, decal sheet):
  [CC BY-NC-SA 4.0](enclosure/LICENSE). Print it, share it and remix it for non-commercial use,
  crediting "Bimfo by Sebastian Röder", with remixes under the same licence. Selling prints or kits needs
  permission.
* Third-party code, fonts and data keep their own licences; see
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Credits

* Fonts: Liberation Sans (SIL OFL 1.1), Adafruit GFX 5×7 (BSD).
* Panel driver: Espressif's esp_lcd ST7789 driver as adapted in the Waveshare demo (Apache-2.0).
* Departure data: Wiener Linien / Stadt Wien – data.wien.gv.at (CC BY 4.0); ÖBB Scotty.
* Render backgrounds: a CC0 photo from Wikimedia Commons and a CC0 HDRI from Poly Haven
  (see [`enclosure/render/`](enclosure/render/README.md)).
* Inspired by the STRABA / catch.it mini-stops.
