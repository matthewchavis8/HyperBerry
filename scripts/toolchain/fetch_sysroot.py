#!/usr/bin/env python3
"""Fetch the pinned Arm newlib and libc++ sysroot."""

import argparse
import hashlib
import shutil
import tarfile
import urllib.request
from pathlib import Path


VERSION = "19.1.5"
ARCHIVE = f"LLVM-ET-Arm-newlib-overlay-{VERSION}.tar.xz"
URL = (
    "https://github.com/ARM-software/LLVM-embedded-toolchain-for-Arm/releases/"
    f"download/release-{VERSION}/{ARCHIVE}"
)
SHA256 = "f900d878a9e149d476cdd99f61b9de72e964a5e767e1de3fcd4fb3c59deaa94c"
SYSROOT_RELATIVE = Path("lib/clang-runtimes/newlib/aarch64-none-elf/aarch64a")


def digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=Path(".toolchain") / f"arm-newlib-{VERSION}")
    args = parser.parse_args()

    out = args.out.resolve()
    sysroot = out / SYSROOT_RELATIVE
    if (sysroot / "include/c++/v1/array").is_file() and (sysroot / "lib/libc++.a").is_file():
        print(sysroot)
        return

    archive = out.parent / ARCHIVE
    out.parent.mkdir(parents=True, exist_ok=True)
    if not archive.is_file() or digest(archive) != SHA256:
        print(f"Downloading {URL}")
        with urllib.request.urlopen(URL) as response, archive.open("wb") as destination:
            shutil.copyfileobj(response, destination)
    if digest(archive) != SHA256:
        raise SystemExit(f"checksum mismatch: {archive}")

    if out.exists():
        shutil.rmtree(out)
    with tarfile.open(archive, "r:xz") as source:
        source.extractall(out, filter="data")
    if not (sysroot / "include/c++/v1/array").is_file() or not (sysroot / "lib/libc++.a").is_file():
        raise SystemExit(f"archive did not contain the expected sysroot: {sysroot}")
    print(sysroot)


if __name__ == "__main__":
    main()
