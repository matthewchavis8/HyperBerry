---
type: Subsystem
title: Scripts
description: Every helper under scripts/, one section per folder, with who calls it and what it reads and writes.
resource: ../../scripts/
tags: [scripts, build, tooling]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../CMakeLists.txt
  - resource: ../../justfile
  - resource: ../../scripts/asmoffsets/asmoffsets.py
  - resource: ../../scripts/bspgen/bspgen.py
  - resource: ../../scripts/bspgen/README.md
  - resource: ../../scripts/busybox/fetch.py
  - resource: ../../scripts/busybox/rootfs.py
  - resource: ../../scripts/cpio/archive.py
  - resource: ../../scripts/linux/fetch.py
  - resource: ../../scripts/lint/report.sh
  - resource: ../../scripts/lint/summary.py
  - resource: ../../scripts/rpi5/flash.sh
  - resource: ../../scripts/toolchain/clangd.sh
  - resource: ../../scripts/toolchain/container.sh
  - resource: ../../scripts/toolchain/fetch_sysroot.py
---

# What it is

Small programs the build and the `justfile` call. Eight are Python and four are shell. None of them is part of the hypervisor image. The Python scripts use only the standard library, apart from `cpio` which `archive.py` and `rootfs.py` run as a program.

# Depends on

- Python 3, the system `cpio` program, and Docker for `container.sh` and `clangd.sh`.
- [bsp](bsp.md): `bspgen` reads the host trees there and `flash.sh` copies the firmware files.

# Used by

- The top level `CMakeLists.txt` runs `bspgen`, `archive.py`, `fetch.py` (both), `rootfs.py` and, through `core/CMakeLists.txt`, `asmoffsets.py`.
- The `justfile` runs `container.sh`, `flash.sh`, and `archive.py` through the `cpio` recipe.
- [Build](../architecture/build.md) and [tests](../architecture/tests.md) describe where each step sits.

# Files

| Folder | File | Language | Called by |
|---|---|---|---|
| `scripts/asmoffsets/` | `asmoffsets.py` | Python | `core/CMakeLists.txt` |
| `scripts/bspgen/` | `bspgen.py`, `README.md` | Python | `hb_generate_regs` in `CMakeLists.txt` |
| `scripts/busybox/` | `fetch.py`, `rootfs.py` | Python | `CMakeLists.txt` |
| `scripts/cpio/` | `archive.py` | Python | `CMakeLists.txt`, `tests/unit/CMakeLists.txt`, `just cpio` |
| `scripts/linux/` | `fetch.py` | Python | `CMakeLists.txt` |
| `scripts/lint/` | `report.sh`, `summary.py` | Shell, Python | By hand |
| `scripts/rpi5/` | `flash.sh` | Shell | `just rpi5` |
| `scripts/toolchain/` | `container.sh`, `clangd.sh`, `fetch_sysroot.py` | Shell, Python | `justfile`, editors, by hand |

# Entry points

## scripts/asmoffsets

`asmoffsets.py --in <assembly text> --out <header>`. It looks for lines of the form `"->NAME VALUE"` that `core/vcpu/vcpuOffsets.cpp` emits through inline assembly, and writes one `#define NAME VALUE` for each. It fails if it finds none. The `VcpuOffsets-<board>` object library is compiled with `-S` so those strings reach the assembly text. The result is `build/<mode>/<board>/generated/vcpuOffsets.h`, which the `.S` files include. See [offsets from layout](../decisions/offsets-from-layout.md) and [Vcpu](vcpu.md).

## scripts/bspgen

`bspgen.py --dtb <host.dtb> --board <name> --out <regs.inc>`. It parses a flattened device tree itself, walks `ranges` up to CPU physical addresses, and finds the first node whose `compatible` matches a GIC or UART list. For a GICv2 node it writes `BSP_GIC_DISTRIBUTOR_*`, `BSP_GIC_CPU_*`, `BSP_GIC_HV_*` and `BSP_GIC_VCPU_*`, each with `_BASE` and `_SIZE`. For a GICv3 node it writes the distributor and redistributor. For the UART it writes `BSP_UART_BASE` and `BSP_UART_SIZE`. It exits with an error if it extracts nothing. See [device tree to regs.inc](../flows/device-tree-to-regs.md).

## scripts/busybox

- `fetch.py --cache <dir> --out <file>` downloads the Debian package `busybox-static_1.38.0-3+b1_arm64.deb`, checks its SHA256, and extracts `usr/bin/busybox`. Both the version and the checksum are constants in the file.
- `rootfs.py --busybox <file> --out <cpio>` checks that the binary is an AArch64 little endian ELF, builds a small root with `inittab`, `rcS` and copies of the binary named `busybox`, `sh`, `mount`, `init` and `reboot`, and packs it as a `newc` CPIO. The console it starts is `ttyAMA0`. This is the default initramfs of the guest archive.

## scripts/cpio

`archive.py [--root <dir>] --out <file> [--file NAME SOURCE]...` stages the files and packs them with the system `cpio -o -H newc -0`. It rejects symlinks, absolute paths, `..`, duplicate names and a name of `TRAILER!!!`. It is how `guest.cpio` is built, and also the unit test fixture. The format is in [guest archive](../guides/GUEST_ARCHIVE.md).

## scripts/linux

`fetch.py --out <file>` downloads a pinned Debian arm64 netboot `linux` image from `snapshot.debian.org`, checks its SHA256, and checks that bytes 56 to 60 read `ARMd`, the AArch64 boot image magic. That file is the default guest kernel for QEMU.

## scripts/lint

`report.sh` writes `build/lint/` with `format.diff`, `tidy.txt` and `SUMMARY.md`. It runs `clang-format` over tracked `.cpp` and `.h` files and `run-clang-tidy` over `boot`, `bsp`, `core`, `drivers`, `lib` and `tests`, using `build/debug/compile_commands.json`. It changes nothing under version control. `summary.py` turns the raw output into the summary.

## scripts/rpi5

`flash.sh <image> <guest-archive> <sd-partition>` mounts the partition at `/mnt/sdcard` with `sudo`, copies `kernel8.img`, `guest.cpio`, and the four firmware files from `bsp/rpi5/firmware/`, runs `sync`, and unmounts on exit. It needs `sudo`.

## scripts/toolchain

- `container.sh [--tty] <command...>` builds `hyperberry-toolchain:local` from the root `Dockerfile`, then runs the command in it with the checkout mounted at `/workspace`, as the calling user. Under GitHub Actions it runs the command directly. Before building it deletes the `CMakeCache.txt` and `CMakeFiles` of any `build/debug`, `build/release` or `build/unit-tests` whose cache was made outside `/workspace`.
- `clangd.sh` runs `clangd-22` inside that image with a path mapping, so an editor on the host can use the container's compiler. It exits with a message if Docker is stopped or the image does not exist.
- `fetch_sysroot.py [--out dir]` downloads the pinned Arm newlib overlay 19.1.5 into `.toolchain/arm-newlib-19.1.5` and prints the sysroot path. CMake uses that folder as the default for `HB_LLVM_SYSROOT` when it exists. Inside Docker the same archive is installed by the `Dockerfile` instead.

# Gotchas

- `scripts/bspgen/README.md` says `bspgen` lets "the BSP header and `boot.S` share one definition". No BSP header exists, and no `boot.S` includes `regs.inc`. Only C++ sources do.
- The `bspgen` output file begins with a comment naming `bsp/<board>/regs.inc`. The file really lands in `build/<mode>/<board>/generated/`.
- `GIC_BASE` is deliberately not generated. The comment in `bspgen.py` says that on a GIC 400 it is the enclosing block base, which the tree does not state.
- `fetch.py` in `busybox` and `linux` check hashes, so a changed upstream file fails the build rather than changing the guest. To move to a new version, change the constants in the file.
- `report.sh` uses `build/debug`, which needs a configured debug preset.
