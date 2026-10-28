#!/usr/bin/env python3
"""Signs a finished firmware build and packs it for a release and for the browser flasher.

    idf.py build
    python tools/release.py --key PATH [--build build] [--out dist] [--tag v1.0.0]

Writes:
  dist/release/bimfo-full.bin    whole flash image for address 0x0
                                 (first install with esptool; erases the settings)
  dist/release/bimfo-update.bin  the signed app alone: the update on the settings page,
                                 and what devices download for automatic updates
  dist/release/LICENSE, THIRD_PARTY_NOTICES.md
  dist/site/                     the flasher page (web/) with firmware/manifest.json
                                 and the separate images, which keep the settings

--key is the release signing key (RSA-3072 PEM; default: $BIMFO_SIGNING_KEY). The signed
app is checked against tools/bimfo-release-key.pub.pem, so a release can't go out with
another key: devices only accept updates signed with the key of the firmware they run.
--tag fails unless it matches version.txt (v1.0.0 for 1.0.0). Run it in an ESP-IDF
shell: signing and merging use espsecure and esptool.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PUBLIC_KEY = os.path.join(ROOT, "tools", "bimfo-release-key.pub.pem")

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


def tool(*args):
    r = subprocess.run([sys.executable, "-m", *args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        fail(f"{args[0]} {args[1]} failed:\n{r.stdout}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--key", default=os.environ.get("BIMFO_SIGNING_KEY"))
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
    if not (config.get("SECURE_SIGNED_APPS_RSA_SCHEME") and config.get("SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT")):
        fail("the build does not check update signatures: see sdkconfig.defaults")
    if config.get("SECURE_BOOT_BUILD_SIGNED_BINARIES"):
        fail("the build signed the app itself; release builds are signed here")
    if not args.key or not os.path.isfile(args.key) or os.path.getsize(args.key) == 0:
        fail("signing key missing: --key PATH or BIMFO_SIGNING_KEY")
    if not os.path.isfile(PUBLIC_KEY):
        fail(f"{os.path.relpath(PUBLIC_KEY, ROOT)} missing")

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
        src, dst = os.path.join(args.build, entry["file"]), os.path.join(firmware, name)
        if key == "app":
            tool("espsecure", "sign_data", "--version", "2", "--keyfile", args.key, "--output", dst, src)
            tool("espsecure", "verify_signature", "--version", "2", "--keyfile", PUBLIC_KEY, dst)
        else:
            shutil.copy(src, dst)
        parts.append({"path": name, "offset": offset})
        merge += [hex(offset), dst]

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
    tool("esptool", "--chip", "esp32s3", "merge_bin", "-o", os.path.join(release, "bimfo-full.bin"),
         "--flash_mode", s["flash_mode"], "--flash_freq", s["flash_freq"], "--flash_size", s["flash_size"], *merge)
    shutil.copy(os.path.join(firmware, "bimfo.bin"), os.path.join(release, "bimfo-update.bin"))
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copy(os.path.join(ROOT, name), release)

    print(f"Bimfo {version}, signed with the release key:")
    for d in (release, firmware):
        for name in sorted(os.listdir(d)):
            print(f"  {os.path.relpath(os.path.join(d, name), ROOT)}  ({os.path.getsize(os.path.join(d, name))} bytes)")


if __name__ == "__main__":
    main()
