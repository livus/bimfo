#!/usr/bin/env python3
"""End-to-end smoke test of the real firmware in Espressif QEMU.

Boots build_qemu/ (see README: sdkconfig.qemu), configures stops through the
device's own web API, waits for live departures fetched by the device over
HTTPS and checks the API, page and serial log (no crash, no watchdog).

    python host/qemu_smoke.py [--qemu PATH] [--port 8090] [--seconds 150]
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys
import threading
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build_qemu"
BAD = re.compile(r"Guru Meditation|abort\(\) was called|assert failed|Stack (canary|protection fault)|"
                 r"task_wdt: Task watchdog got triggered|CORRUPT HEAP")

failures = 0
log_lines = []


def check(cond, what):
    global failures
    print(("  ok   " if cond else "  FAIL ") + what, flush=True)
    failures += 0 if cond else 1


def http(port, path, body=None, timeout=60, raw_body=None):
    data = raw_body if raw_body is not None else (json.dumps(body).encode() if body is not None else None)
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=data, method="POST" if data else "GET")
    if data is not None:
        req.add_header("Content-Type", "application/json")
        req.add_header("X-Bimfo", "1")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        raw = r.read()
        return r.status, raw


def wait_log(pattern, timeout, start=0):
    rx = re.compile(pattern)
    t0 = time.time()
    while time.time() - t0 < timeout:
        if any(rx.search(l) for l in log_lines[start:]):
            return True
        time.sleep(0.5)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qemu", default=next(iter(glob.glob(str(Path.home() / ".espressif/tools/qemu-xtensa/*/qemu/bin/qemu-system-xtensa*"))), None))
    ap.add_argument("--port", type=int, default=8090)
    ap.add_argument("--seconds", type=int, default=150)
    args = ap.parse_args()
    flash = BUILD / "qemu_flash.bin"
    if not args.qemu or not flash.exists():
        sys.exit("need build_qemu/qemu_flash.bin and qemu-system-xtensa (see README)")

    # QEMU writes NVS/OTA changes back into the image: run on a fresh copy.
    run_flash = BUILD / "qemu_flash_run.bin"
    run_flash.write_bytes(flash.read_bytes())
    proc = subprocess.Popen(
        [args.qemu, "-nographic", "-machine", "esp32s3", "-m", "8M",
         "-drive", f"file={run_flash},if=mtd,format=raw",
         "-global", "driver=timer.esp32s3.timg,property=wdt_disable,value=true",
         "-nic", f"user,model=open_eth,hostfwd=tcp::{args.port}-:80"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)

    def reader():
        for raw in proc.stdout:
            line = raw.decode("utf-8", "replace").rstrip()
            log_lines.append(line)
    threading.Thread(target=reader, daemon=True).start()

    try:
        print("boot")
        check(wait_log(r"Calling app_main", 60), "app_main reached")
        check(wait_log(r"got IP", 60), "network up (emulated Ethernet, DHCP)")
        check(wait_log(r"web server running", 10), "web server started")
        check(wait_log(r"LVGL ready", 20), "display task runs LVGL")

        print("web API")
        st, raw = http(args.port, "/")
        check(st == 200 and b"Bimfo" in raw, f"page served ({len(raw) // 1024} kB)")
        st, raw = http(args.port, "/api/config")
        cfg = json.loads(raw)
        check(cfg["stops"] == [] and "pass" not in cfg["wifi"], "fresh config, no password exposed")
        try:
            req = urllib.request.Request(f"http://127.0.0.1:{args.port}/api/reboot", data=b"{}", method="POST")
            urllib.request.urlopen(req, timeout=10)
            check(False, "POST without X-Bimfo rejected")
        except urllib.error.HTTPError as e:
            check(e.code == 403, "POST without X-Bimfo rejected")

        st, raw = http(args.port, "/api/wl/search?q=westbahnhof", timeout=120)
        hits = json.loads(raw)
        check(bool(hits) and hits[0]["name"] == "Westbahnhof", "device searched the live WL station list")
        st, raw = http(args.port, "/api/wl/lines?id=60201468", timeout=60)
        lines = json.loads(raw)
        check(len(lines) >= 8 and all(l["rbl"] > 0 for l in lines), f"device fetched {len(lines)} live WL lines")
        st, raw = http(args.port, "/api/oebb/search?q=Wien%20H%C3%BCtteldorf", timeout=60)
        hits = json.loads(raw)
        check(bool(hits) and "ütteldorf" in hits[0]["name"], f"OEBB search via device: {hits[0]['name'] if hits else '-'}")

        print("configure and fetch")
        u3 = [l for l in lines if l["line"] == "U3"]
        cfg["stops"] = [
            {"kind": "wl", "name": "Westbahnhof", "id": 60201468, "walk": 0,
             "sel": [{"line": l["line"], "dir": l["dir"], "rbl": l["rbl"]} for l in u3]},
            {"kind": "oebb", "name": "Wien Westbf", "id": 1291501, "walk": 0, "trains_only": True, "sel": []},
        ]
        cfg["theme"] = "oebb"
        st, raw = http(args.port, "/api/config", cfg)
        resp = json.loads(raw)
        check(st == 200 and resp.get("ok") and not resp.get("restart"), "config saved without restart")
        check(wait_log(r"fetch: \d+ departures, 0/2 stops failed", 90), "device fetched both stops over HTTPS")
        st, raw = http(args.port, "/api/board")
        board = json.loads(raw)
        deps = board["departures"]
        lines_seen = {d["line"] for d in deps}
        check(any(d["line"] == "U3" for d in deps) and any(d["stop"] == 1 for d in deps),
              f"board has WL U3 and OEBB trains ({len(deps)} departures, lines {sorted(lines_seen)[:8]})")
        check(board["now"] > 1_700_000_000, "device clock set (SNTP or API server time)")
        st, raw = http(args.port, "/api/status")
        s = json.loads(raw)
        check(s["fetch"]["ok"] >= 1 and s["fetch"]["last_error"] == "", "status: fetch ok")
        print(f"    heap internal {s['heap'] // 1024} kB (min {s['heap_min'] // 1024} kB), PSRAM {s['psram'] // 1024} kB")
        check(s["heap_min"] > 40 * 1024, "internal heap never below 40 kB")

        print(f"run for a while ({args.seconds} s)")
        time.sleep(args.seconds)
        st, raw = http(args.port, "/api/status")
        s = json.loads(raw)
        check(s["fetch"]["ok"] >= 3, f"kept fetching ({s['fetch']['ok']} rounds, {s['fetch']['errors']} errors)")
        check(s["heap_min"] > 40 * 1024, f"internal heap minimum {s['heap_min'] // 1024} kB")
        stacks = s.get("stack_free", {})
        print("    stack head-room:", ", ".join(f"{k} {v} B" for k, v in stacks.items()))
        check(all(k in stacks for k in ("fetch", "display", "net", "httpd")), "all tasks report their stack")
        check(all(v >= 1024 for v in stacks.values()), "every task keeps >= 1 kB of stack spare")

        print("firmware update (OTA)")
        try:
            http(args.port, "/api/ota", raw_body=os.urandom(64 * 1024))
            check(False, "random file rejected")
        except urllib.error.HTTPError as e:
            check(e.code == 400 and "Firmware" in e.read().decode(), "random file rejected before flashing")
        image = (BUILD / "bimfo.bin").read_bytes()
        mark = len(log_lines)
        st, raw = http(args.port, "/api/ota", raw_body=image, timeout=300)
        check(st == 200 and json.loads(raw).get("restart"), f"image accepted ({len(image) // 1024} kB)")
        boots_before = sum("rst:" in l for l in log_lines)
        check(wait_log(r"Loaded app from partition at offset 0x420000", 90, mark), "rebooted into the second OTA slot")
        check(sum("rst:" in l for l in log_lines) == boots_before + 1, "exactly one reboot")
        check(wait_log(r"fetch: \d+ departures, 0/2 stops failed", 120, mark), "fetching again after the update")
        st, raw = http(args.port, "/api/status")
        s = json.loads(raw)
        check(s["partition"] == "ota_1", f"running from {s['partition']}")
        st, raw = http(args.port, "/api/config")
        check(len(json.loads(raw)["stops"]) == 2 and json.loads(raw)["theme"] == "oebb", "configuration kept in NVS across the update")
    finally:
        proc.kill()
        bad = [l for l in log_lines if BAD.search(l)]
        check(not bad, "no crash/assert/watchdog in serial log" + (": " + bad[0] if bad else ""))
        (BUILD / "qemu_smoke.log").write_text("\n".join(log_lines), encoding="utf-8")
        print(f"serial log: {BUILD / 'qemu_smoke.log'}")
    print("FAILED" if failures else "QEMU smoke test passed")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
