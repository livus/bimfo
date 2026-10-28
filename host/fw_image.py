"""Edits ESP32 app images for the update tests: strip or add the signature, and
change the version in the app description (fixing checksum and hash)."""
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

SIG_BLOCK = 4096            # one signature sector appended by espsecure (secure boot v2)
APP_DESC_MAGIC = 0xABCD5432


def unsigned(image):
    """The image without its signature sector (the build pads apps to 64 kB)."""
    if len(image) % 0x10000 != SIG_BLOCK or image[-SIG_BLOCK] != 0xE7:
        raise ValueError("not a signed image")
    return image[:-SIG_BLOCK]


def with_version(image, version):
    """A copy of an unsigned image with another version string."""
    img = bytearray(image)
    if img[0] != 0xE9:
        raise ValueError("not an app image")
    desc = 24 + 8   # image header, first segment header: the app description comes first
    if struct.unpack_from("<I", img, desc)[0] != APP_DESC_MAGIC:
        raise ValueError("no app description")
    v = version.encode()
    img[desc + 16:desc + 48] = v + b"\0" * (32 - len(v))

    # Checksum (XOR of all segment data, seed 0xEF) in the last byte of the
    # 16-byte aligned block after the segments, then SHA-256 of all before it.
    pos, checksum = 24, 0xEF
    for _ in range(img[1]):
        _, length = struct.unpack_from("<II", img, pos)
        for b in img[pos + 8:pos + 8 + length]:
            checksum ^= b
        pos += 8 + length
    pos = (pos + 16) & ~15
    img[pos - 1] = checksum
    if img[23]:   # hash appended
        img[pos:pos + 32] = hashlib.sha256(img[:pos]).digest()
    return bytes(img)


def sign(image, keyfile):
    """The image signed with an RSA-3072 key (espsecure)."""
    with tempfile.TemporaryDirectory() as d:
        src, dst = os.path.join(d, "in.bin"), os.path.join(d, "out.bin")
        with open(src, "wb") as f:
            f.write(image)
        subprocess.run([sys.executable, "-m", "espsecure", "sign_data", "--version", "2", "--keyfile", keyfile,
                        "--output", dst, src], check=True, stdout=subprocess.DEVNULL)
        with open(dst, "rb") as f:
            return f.read()


def new_key(path):
    subprocess.run([sys.executable, "-m", "espsecure", "generate_signing_key", "--version", "2", "--scheme", "rsa3072",
                    path], check=True, stdout=subprocess.DEVNULL)
