#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path


INITTAB = """::sysinit:/etc/init.d/rcS
ttyAMA0::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
"""

RCS = """#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
echo "HyperBerry BusyBox user space"
"""


def write(path, contents, mode):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents)
    path.chmod(mode)


def build(binary, output):
    if not binary.is_file():
        raise ValueError(f"BusyBox binary is missing: {binary}")
    header = binary.read_bytes()[:20]
    if header[:4] != b"\x7fELF" or header[4] != 2 or header[5] != 1 or header[18:20] != b"\xb7\0":
        raise ValueError(f"BusyBox binary is not an AArch64 little endian ELF: {binary}")
    with tempfile.TemporaryDirectory(prefix="hyperberry-rootfs-") as temporary:
        root = Path(temporary)
        for destination in (root / "bin/busybox", root / "bin/sh", root / "sbin/init"):
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(binary, destination)
            destination.chmod(0o755)
        write(root / "etc/inittab", INITTAB, 0o644)
        write(root / "etc/init.d/rcS", RCS, 0o755)
        names = sorted(path.relative_to(root).as_posix() for path in root.rglob("*")
                       if path.is_file() or path.is_dir())
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("wb") as archive:
            subprocess.run(["cpio", "-o", "-H", "newc", "--quiet"],
                           cwd=root, input=("\n".join(names) + "\n").encode(), stdout=archive, check=True)


def main():
    parser = argparse.ArgumentParser(description="Build the HyperBerry BusyBox initramfs.")
    parser.add_argument("--busybox", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    try:
        build(args.busybox, args.out)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"BusyBox rootfs build failed: {error}\n")


if __name__ == "__main__":
    main()
