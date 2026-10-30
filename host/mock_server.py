#!/usr/bin/env python3
"""Serves main/www/index.html with a fake device API, so the configuration page
can be developed and tested without the hardware.

    python host/mock_server.py [--port 8080] [--live]

Without --live, searches and line lists come from host/fixtures; with --live
they are fetched from the real Wiener Linien / OEBB services (like the device
does). Saved configurations are kept in memory and printed.
"""
import argparse
import csv
import io
import json
import re
import time
import unicodedata
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
PAGE = HERE.parent / "main" / "www" / "index.html"
FIX = HERE / "fixtures"
WL = "https://www.wienerlinien.at/ogd_realtime"

TYPES = {"ptMetro": "metro", "ptTram": "tram", "ptTramWLB": "tram", "ptBusCity": "bus", "ptBusNight": "nightbus",
         "ptTrainS": "sbahn"}

state = {
    "config": {
        "wifi": {"ssid": "Zuhause", "has_pass": True}, "hostname": "bimfo", "theme": "wl", "group": True, "wl_notice": False, "wl_paging": True, "wl_step_free": True,
        "brightness": 80, "refresh": 30, "flip": False, "auto_update": False,
        "night": {"brightness": 15, "start": "22:00", "end": "06:00"},
        "stops": [{"kind": "wl", "name": "Westbahnhof", "id": 60201468, "walk": 3,
                   "sel": [{"line": "U3", "dir": "H", "rbl": 4921}, {"line": "U3", "dir": "R", "rbl": 4920}]}],
    },
    "saved": [],
    "update_started": 0,    # time of the last "Jetzt prüfen"; a check takes 2 s
    "booted": time.time(),
}
LIVE = False


def fold(s):
    s = s.lower().replace("ß", "ss")
    return "".join(c for c in unicodedata.normalize("NFKD", s) if not unicodedata.combining(c))


def get(url, encoding="utf-8"):
    req = urllib.request.Request(url, headers={"User-Agent": "bimfo-mock/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.read().decode(encoding, errors="replace")


def wl_monitor(diva):
    if LIVE:
        return json.loads(get(f"{WL}/monitor?diva={diva}"))
    return json.loads((FIX / "wl_westbahnhof_diva.json").read_text(encoding="utf-8"))


def wl_lines(diva):
    out, seen = [], set()
    for m in wl_monitor(diva)["data"].get("monitors", []):
        rbl = m["locationStop"]["properties"]["attributes"].get("rbl", 0)
        for l in m.get("lines", []):
            key = (l["name"], l.get("direction"), rbl)
            if key in seen:
                continue
            seen.add(key)
            out.append({"line": l["name"], "towards": l.get("towards", "").strip(), "dir": l.get("direction", ""),
                        "rbl": rbl, "type": TYPES.get(l.get("type"), "unknown")})
    order = ["metro", "tram", "bus", "nightbus", "sbahn", "train", "unknown"]
    nat = lambda s: [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", s)]
    out.sort(key=lambda x: (order.index(x["type"]), nat(x["line"]), x["dir"]))
    return out


def wl_search(q):
    text = get(f"{WL}/doku/ogd/wienerlinien-ogd-haltestellen.csv") if LIVE else \
        (FIX / "wl_haltestellen.csv").read_text(encoding="utf-8")
    fq, hits = fold(q), []
    for row in csv.reader(io.StringIO(text), delimiter=";"):
        if len(row) < 2 or not row[0].isdigit():
            continue
        name = fold(row[1])
        if fq in name:
            hits.append(((0 if name.startswith(fq) else 1000) + len(name), {"id": int(row[0]), "name": row[1]}))
    return [h for _, h in sorted(hits, key=lambda x: x[0])[:15]]


def oebb_search(q):
    if LIVE:
        text = get("https://fahrplan.oebb.at/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1&REQ0JourneyStopsB=12"
                   "&js=true&S=" + urllib.parse.quote(q), "latin-1")
    else:
        text = (FIX / "oebb_getstop_westbahnhof.txt").read_text(encoding="utf-8")
    data = json.loads(text[text.index("{"):text.rindex("}") + 1])
    return [{"id": int(s["extId"]), "name": s["value"]} for s in data.get("suggestions", []) if s.get("type") == "1"]


def board():
    now = int(time.time())
    deps = []
    data = json.loads((FIX / "wl_westbahnhof_diva.json").read_text(encoding="utf-8"))
    for m in data["data"]["monitors"]:
        for l in m["lines"]:
            for i, d in enumerate(l["departures"]["departure"][:2]):
                cd = d["departureTime"].get("countdown", 0)
                deps.append({"line": l["name"], "towards": l["towards"], "platform": l.get("platform", ""),
                             "dir": l.get("direction", ""), "type": TYPES.get(l.get("type"), "unknown"),
                             "minutes": cd, "planned": now + cd * 60, "expected": now + cd * 60, "delayed": False,
                             "cancelled": False, "barrier_free": l.get("barrierFree", False), "realtime": True,
                             "stop": 0})
    deps.sort(key=lambda d: d["minutes"])
    return {"title": "Westbahnhof", "notice": "", "fetched_at": now - 12, "now": now, "departures": deps}


def update_status(now):
    busy = time.time() - state["update_started"] < 2
    checked = state["update_started"] and not busy
    return {"available": True, "auto": state["config"].get("auto_update", False), "busy": busy,
            "progress": -1, "restarting": False, "last_check": int(state["update_started"]) if checked else 0,
            "next_check": now + 6 * 3600 if state["config"].get("auto_update") else 0,
            "message": "Prüfe" if busy else "Aktuell (neueste Version 1.0.0)" if checked else ""}


def status():
    now = int(time.time())
    return {"version": "mock", "build": "", "idf": "", "partition": "ota_0", "hostname": state["config"]["hostname"],
            "uptime": int(now - state["booted"]), "heap": 182000, "heap_min": 150000, "psram": 7900000,
            "time": now, "time_synced": True,
            "wifi": {"configured": True, "ssid": state["config"]["wifi"]["ssid"], "connected": True,
                     "ip": "192.168.1.50", "rssi": -58, "disconnects": 0},
            "ap": {"active": False, "ssid": "", "clients": 0},
            "fetch": {"last_ok": now - 12, "last_attempt": now - 12, "ok": 42, "errors": 0,
                      "consecutive_errors": 0, "last_error": ""},
            "update": update_status(now)}


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, ctype="application/json; charset=utf-8"):
        data = body if isinstance(body, bytes) else json.dumps(body, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):
        print("  " + fmt % args)

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(url.query)
        try:
            if url.path in ("/", "/index.html"):
                return self.send(200, PAGE.read_bytes(), "text/html; charset=utf-8")
            if url.path == "/api/status":
                return self.send(200, status())
            if url.path == "/api/config":
                return self.send(200, state["config"])
            if url.path == "/api/board":
                return self.send(200, board())
            if url.path == "/api/wifi/scan":
                return self.send(200, [{"ssid": "Zuhause", "rssi": -55, "secure": True},
                                       {"ssid": "Nachbar", "rssi": -80, "secure": True}])
            if url.path == "/api/wl/search":
                return self.send(200, wl_search(q.get("q", [""])[0]))
            if url.path == "/api/wl/lines":
                return self.send(200, wl_lines(int(q.get("id", ["0"])[0])))
            if url.path == "/api/oebb/search":
                return self.send(200, oebb_search(q.get("q", [""])[0]))
        except Exception as e:  # mirror the device: JSON error, 502
            return self.send(502, {"error": str(e)})
        self.send(404, {"error": "not found"})

    def do_POST(self):
        if not self.headers.get("X-Bimfo"):
            return self.send(403, {"error": "X-Bimfo header fehlt"})
        body = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
        if self.path == "/api/config":
            try:
                new = json.loads(body)
            except ValueError:
                return self.send(400, {"error": "invalid JSON"})
            old = state["config"]
            restart = new.get("wifi", {}).get("ssid") != old["wifi"]["ssid"] or "pass" in new.get("wifi", {}) \
                or new.get("hostname") != old["hostname"]
            new.setdefault("wifi", {})["has_pass"] = bool(new["wifi"].pop("pass", None)) or old["wifi"]["has_pass"]
            state["config"] = new
            state["saved"].append(new)
            print("  saved config:", json.dumps(new, ensure_ascii=False))
            return self.send(200, {"ok": True, "restart": restart})
        if self.path == "/api/update":
            if time.time() - state["update_started"] < 2:
                return self.send(409, {"error": "Es läuft bereits ein Update"})
            state["update_started"] = time.time()
            return self.send(200, {"ok": True, "restart": False})
        if self.path in ("/api/reboot", "/api/reset", "/api/setup", "/api/ota"):
            return self.send(200, {"ok": True, "restart": self.path != "/api/setup"})
        self.send(404, {"error": "not found"})


def main():
    global LIVE
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--live", action="store_true", help="query the real Wiener Linien / OEBB services")
    args = ap.parse_args()
    LIVE = args.live
    srv = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"mock device on http://127.0.0.1:{args.port}/ ({'live' if LIVE else 'fixtures'})")
    srv.serve_forever()


if __name__ == "__main__":
    main()
