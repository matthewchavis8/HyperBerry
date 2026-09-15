#!/usr/bin/env python3

import argparse
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile


def build(root, output, files):
    output = output.resolve()
    with tempfile.TemporaryDirectory(prefix="hyperberry-cpio-") as temporary:
        staging = Path(temporary)
        if root:
            for source in sorted(root.rglob("*")):
                if source.is_symlink() or not (source.is_file() or source.is_dir()):
                    raise ValueError(f"Unsupported archive input: {source}")
                target = staging / source.relative_to(root)
                if source.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(source, target)
        for name, source in files:
            path = PurePosixPath(name)
            if path.is_absolute() or ".." in path.parts or not path.parts:
                raise ValueError(f"Invalid archive path: {name}")
            target = staging / path
            if target.exists():
                raise ValueError(f"Duplicate archive path: {name}")
            source = Path(source)
            if source.is_symlink() or not source.is_file():
                raise ValueError(f"Expected a regular file: {source}")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        names = sorted(path.relative_to(staging).as_posix() for path in staging.rglob("*"))
        if "TRAILER!!!" in names:
            raise ValueError("TRAILER!!! is reserved by CPIO")
        for name in names:
            path = staging / name
            path.chmod(0o755 if path.is_dir() else 0o644)
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as archive:
            temporary_output = Path(archive.name)
            try:
                subprocess.run(
                    ["cpio", "-o", "-H", "newc", "-0"],
                    input=b"".join(name.encode() + b"\0" for name in names),
                    cwd=staging, stdout=archive, check=True,
                )
                archive.close()
                temporary_output.replace(output)
            finally:
                temporary_output.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description="Create a newc CPIO archive with standard cpio.")
    parser.add_argument("--root", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--file", nargs=2, action="append", default=[], metavar=("NAME", "SOURCE"))
    args = parser.parse_args()
    if args.root and not args.root.is_dir():
        parser.error("--root must be a directory")
    try:
        build(args.root, args.out, args.file)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
