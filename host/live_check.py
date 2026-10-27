#!/usr/bin/env python3
"""Fetches fresh responses from the real services (the same URLs the firmware
builds) and runs them through the firmware's parsers (bimfo_parse), checking
plausibility and the size limits the device uses.

    python host/live_check.py [--save]     (--save refreshes host/fixtures)
"""
import datetime
import json
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
PARSE = next((p for p in [HERE / "build" / "bimfo_parse.exe", HERE / "build" / "bimfo_parse"] if p.exists()), None)
WL = "https://www.wienerlinien.at/ogd_realtime"
WL_MAX_BODY = 384 * 1024      # main/platform/fetcher.c
OEBB_MAX_BODY = 64 * 1024

# (label, diva) - from small stops to the largest interchanges
WL_STATIONS = [("Westbahnhof", 60201468), ("Karlsplatz", 60200657), ("Schwedenplatz", 60201198),
               ("Praterstern", 60201040), ("Stephansplatz", 60201320)]
OEBB_STATIONS = [("Wien Hbf", 1290401), ("Wien Westbahnhof", 1291501), ("Wien Meidling", 1191201)]

failures = 0


def check(cond, what):
    global failures
    print(("  ok   " if cond else "  FAIL ") + what)
    failures += 0 if cond else 1


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "bimfo-live-check/1.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def parse(mode, data, *extra):
    with tempfile.NamedTemporaryFile(delete=False, suffix=".txt") as f:
        f.write(data)
        path = f.name
    out = subprocess.run([str(PARSE), mode, path, *extra], capture_output=True, text=True, encoding="utf-8", errors="replace")
    Path(path).unlink()
    if out.returncode != 0 or not out.stdout:
        print("    parser error:", out.stderr.strip())
        return None
    return json.loads(out.stdout)


def main():
    if not PARSE:
        sys.exit("build host/ first (bimfo_parse missing)")
    save = "--save" in sys.argv
    lines_seen = 0
    for name, diva in WL_STATIONS:
        print(f"Wiener Linien {name} (diva {diva})")
        body = fetch(f"{WL}/monitor?activateTrafficInfo=stoerungkurz&diva={diva}")
        check(len(body) < WL_MAX_BODY, f"response {len(body) // 1024} kB below the device limit")
        board = parse("wl", body)
        check(board is not None, "parsed")
        if board:
            deps = board["departures"]
            check(len(deps) > 0, f"{len(deps)} departures")
            check(all(d["line"] and d["towards"] for d in deps), "every departure has line and destination")
            check(all(0 <= d["minutes"] < 240 for d in deps), "minutes plausible")
            print("    next:", ", ".join(f'{d["line"]}>{d["towards"]} {d["minutes"]}' for d in deps[:4]))
        lines = parse("wllines", body)
        if lines is not None:
            lines_seen += len(lines)
            check(all(l["rbl"] > 0 and l["dir"] in ("H", "R") for l in lines), f"{len(lines)} lines with platform ids")
        if save and name == "Westbahnhof":
            (HERE / "fixtures" / "wl_westbahnhof_diva.json").write_bytes(body)

    print("Wiener Linien station list")
    csv = fetch(f"{WL}/doku/ogd/wienerlinien-ogd-haltestellen.csv")
    check(len(csv) < 512 * 1024, f"{len(csv) // 1024} kB below the device limit")
    hits = parse("wlsearch", csv, "karlsplatz")
    check(bool(hits) and hits[0]["name"] == "Karlsplatz", "search 'karlsplatz' -> Karlsplatz first")

    now = datetime.datetime.now()
    for name, eva in OEBB_STATIONS:
        print(f"OEBB {name} (eva {eva})")
        url = ("https://fahrplan.oebb.at/bin/stboard.exe/dn?L=vs_scotty.vs_liveticker&boardType=dep"
               "&outputMode=tickerDataOnly&start=yes&selectDate=period&additionalTime=0"
               f"&evaId={eva}&maxJourneys=16&productsFilter=1111110000000000"
               f"&time={now:%H:%M}&dateBegin={now:%d.%m.%Y}&dateEnd={now:%d.%m.%Y}")
        body = fetch(url)
        check(len(body) < OEBB_MAX_BODY, f"response {len(body) // 1024} kB below the device limit")
        board = parse("oebb", body)
        check(board is not None, "parsed")
        if board:
            deps = board["departures"]
            check(len(deps) > 0, f"{len(deps)} departures, station '{board['title']}'")
            check(all(d["planned"] > 0 for d in deps), "every departure has a time")
            check(not any("&#" in d["towards"] for d in deps), "HTML entities decoded")
            print("    next:", ", ".join(f'{d["line"]}>{d["towards"]} {d["minutes"]}' for d in deps[:4]))
    body = fetch("https://fahrplan.oebb.at/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1&REQ0JourneyStopsB=12"
                 "&js=true&S=" + urllib.parse.quote("Wien Hütteldorf"))
    hits = parse("oebbsearch", body)
    check(bool(hits) and "tteldorf" in hits[0]["name"], f"OEBB search 'Wien Hütteldorf' -> {hits[0]['name'] if hits else '-'}")

    print("FAILED" if failures else "all live checks passed")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
