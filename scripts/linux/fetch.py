#!/usr/bin/env python3

import argparse
import hashlib
import shutil
import tempfile
import urllib.request
from pathlib import Path


URL = ("https://snapshot.debian.org/archive/debian/20260908T000000Z/dists/stable/main/"
       "installer-arm64/current/images/netboot/debian-installer/arm64/linux")
SHA256 = "cbe59a02e7ea979a150661032440c94e2c4db0b735af2416e11ae5cac15a58e4"


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def fetch(output):
    if output.exists() and digest(output) == SHA256:
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as temporary:
        temporary_path = Path(temporary.name)
        try:
            with urllib.request.urlopen(URL) as response:
                shutil.copyfileobj(response, temporary)
            temporary.close()
            if digest(temporary_path) != SHA256:
                raise ValueError(f"Linux Image SHA256 mismatch: {URL}")
            header = temporary_path.read_bytes()[:64]
            if len(header) != 64 or header[56:60] != b"ARMd":
                raise ValueError("Linux Image is not an AArch64 boot image")
            temporary_path.replace(output)
        finally:
            temporary_path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description="Fetch a pinned AArch64 Linux Image for QEMU.")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    try:
        fetch(args.out)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Linux Image fetch failed: {error}\n")


if __name__ == "__main__":
    main()
