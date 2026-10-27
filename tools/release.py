#!/usr/bin/env python3
"""Packs a finished firmware build for a release and for the browser flasher.

    idf.py build
    python tools/release.py [--build build] [--out dist] [--tag v1.0.0]

Writes:
  dist/release/bimfo-<version>-full.bin    whole flash image for address 0x0
                                           (first install with esptool; erases the settings)
  dist/release/bimfo-<version>-update.bin  the app alone, for the update on the settings page
  dist/release/LICENSE, THIRD_PARTY_NOTICES.md
  dist/site/                               the flasher page (web/) with firmware/manifest.json
                                           and the separate images, which keep the settings

--tag fails unless it matches version.txt (v1.0.0 for 1.0.0). Run it in an ESP-IDF
shell: the full image is merged with esptool.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# flasher_args.json keys -> file names on the flasher site
PARTS = [
    ("bootloader", "bootloader.bin"),
    ("partition-table", "partition-table.bin"),
    ("otadata", "ota_data_initial.bin"),
    ("app", "bimfo.bin"),
]


def fail(msg):
    sys.exit("release: " + msg)


def load(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        fail(f"{path} missing: run idf.py build first")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(ROOT, "build"))
    ap.add_argument("--out", default=os.path.join(ROOT, "dist"))
    ap.add_argument("--tag")
    args = ap.parse_args()

    desc = load(os.path.join(args.build, "project_description.json"))
    flasher = load(os.path.join(args.build, "flasher_args.json"))
    config = load(os.path.join(args.build, "config", "sdkconfig.json"))

    with open(os.path.join(ROOT, "version.txt"), encoding="utf-8") as f:
        wanted = f.read().strip()
    version = desc["project_version"]
    if version != wanted:
        fail(f"build has version {version}, version.txt says {wanted}: rebuild")
    if args.tag and args.tag != "v" + version:
        fail(f"tag {args.tag} does not match version.txt ({version})")
    if config.get("APP_QEMU"):
        fail("this is the QEMU build; release the hardware build")
    if desc["target"] != "esp32s3":
        fail(f"unexpected target {desc['target']}")

    release = os.path.join(args.out, "release")
    site = os.path.join(args.out, "site")
    firmware = os.path.join(site, "firmware")
    shutil.rmtree(args.out, ignore_errors=True)
    os.makedirs(release)
    shutil.copytree(os.path.join(ROOT, "web"), site)
    os.makedirs(firmware)

    parts, merge = [], []
    for key, name in PARTS:
        entry = flasher[key]
        offset = int(entry["offset"], 16)
        shutil.copy(os.path.join(args.build, entry["file"]), os.path.join(firmware, name))
        parts.append({"path": name, "offset": offset})
        merge += [hex(offset), os.path.join(firmware, name)]

    manifest = {
        "name": "Bimfo",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
    }
    with open(os.path.join(firmware, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    s = flasher["flash_settings"]
    full = os.path.join(release, f"bimfo-{version}-full.bin")
    subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin", "-o", full,
                    "--flash_mode", s["flash_mode"], "--flash_freq", s["flash_freq"],
                    "--flash_size", s["flash_size"]] + merge, check=True, stdout=subprocess.DEVNULL)
    shutil.copy(os.path.join(firmware, "bimfo.bin"), os.path.join(release, f"bimfo-{version}-update.bin"))
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copy(os.path.join(ROOT, name), release)

    print(f"Bimfo {version}:")
    for d in (release, firmware):
        for name in sorted(os.listdir(d)):
            print(f"  {os.path.relpath(os.path.join(d, name), ROOT)}  ({os.path.getsize(os.path.join(d, name))} bytes)")


if __name__ == "__main__":
    main()
