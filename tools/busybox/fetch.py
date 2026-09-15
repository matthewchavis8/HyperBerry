#!/usr/bin/env python3

import argparse
import hashlib
import io
import shutil
import tarfile
import tempfile
import urllib.request
from pathlib import Path


VERSION = "1.38.0-3+b1"
URL = f"https://deb.debian.org/debian/pool/main/b/busybox/busybox-static_{VERSION}_arm64.deb"
SHA256 = "968d1aa8f579fa1ac59c26afa365454369e13cf29848e6400b50028fed0ffda0"


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def download(package):
    if package.exists() and digest(package) == SHA256:
        return
    package.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=package.parent, delete=False) as temporary:
        temporary_path = Path(temporary.name)
        try:
            with urllib.request.urlopen(URL) as response:
                shutil.copyfileobj(response, temporary)
            temporary.close()
            if digest(temporary_path) != SHA256:
                raise ValueError(f"BusyBox package SHA256 mismatch: {URL}")
            temporary_path.replace(package)
        finally:
            temporary_path.unlink(missing_ok=True)


def member(package, name):
    with package.open("rb") as source:
        if source.read(8) != b"!<arch>\n":
            raise ValueError("BusyBox package is not a Debian archive")
        while header := source.read(60):
            if len(header) != 60 or header[58:] != b"`\n":
                raise ValueError("BusyBox package has an invalid archive member")
            size = int(header[48:58].decode().strip())
            member_name = header[:16].decode().strip().rstrip("/")
            contents = source.read(size)
            if len(contents) != size:
                raise ValueError("BusyBox package ends inside an archive member")
            if size % 2:
                source.read(1)
            if member_name == name:
                return contents
    raise ValueError(f"BusyBox package does not contain {name}")


def extract(package, output):
    contents = member(package, "data.tar.xz")
    with tarfile.open(fileobj=io.BytesIO(contents), mode="r:xz") as archive:
        entry = archive.getmember("./usr/bin/busybox")
        if not entry.isfile():
            raise ValueError("BusyBox package usr/bin/busybox is not a regular file")
        source = archive.extractfile(entry)
        if source is None:
            raise ValueError("BusyBox package usr/bin/busybox cannot be extracted")
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            try:
                shutil.copyfileobj(source, temporary)
                temporary.close()
                temporary_path.replace(output)
            finally:
                temporary_path.unlink(missing_ok=True)
    output.chmod(0o755)


def main():
    parser = argparse.ArgumentParser(description="Fetch a pinned static AArch64 BusyBox binary.")
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    package = args.cache / f"busybox-static_{VERSION}_arm64.deb"
    try:
        download(package)
        extract(package, args.out)
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        parser.exit(1, f"BusyBox fetch failed: {error}\n")


if __name__ == "__main__":
    main()
