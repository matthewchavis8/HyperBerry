---
type: Architecture
title: Dependencies
description: The compilers, libraries, tools and downloaded files the build needs, what each is for, and the file that pins its version.
resource: ../../Dockerfile
tags: [architecture, build, dependencies]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../README.md
  - resource: ../../Dockerfile
  - resource: ../../CMakeLists.txt
  - resource: ../../cmake/aarch64-linux-toolchain.cmake
  - resource: ../../scripts/toolchain/fetch_sysroot.py
  - resource: ../../scripts/busybox/fetch.py
  - resource: ../../scripts/linux/fetch.py
  - resource: ../../.github/workflows/toolchain.yml
  - resource: ../../.github/workflows/docs.yml
  - resource: ../../.github/actions/setup-ci/action.yml
  - resource: ../../docs/sphinx/requirements.txt
---

# The short version

Nothing needs to be installed on the host except Docker and `just`. The `Dockerfile` at the repo root holds the toolchain, and CI uses the same file published as an image. The table lists what is in it and what is fetched on top of it at build time.

# Toolchain

| What | Used for | Where pinned |
|---|---|---|
| LLVM Clang 22 (`clang-22`, `clang++-22`, `clangd-22`) | The only supported compiler. CMake stops if it is not Clang 22.1 or newer. | `Dockerfile` installs `llvm.sh 22`. The check is in `CMakeLists.txt`. |
| `lld-22` | Linker for the bare metal images, selected with `-fuse-ld=lld`. | `Dockerfile` |
| `llvm-objcopy` (`llvm-objcopy-22`) | Turns the ELF into `kernel8.img`. Found by `find_program` with the name `llvm-objcopy-22` first. | `CMakeLists.txt` |
| Arm newlib and libc++ overlay 19.1.5 | The C and C++ runtime for `aarch64-none-elf`: `libc++.a`, `libc++abi.a`, `libunwind.a`, `libc.a`, `libclang_rt.builtins.a`. | URL and SHA256 `f900d878...` in `Dockerfile` and in `scripts/toolchain/fetch_sysroot.py`. Found through `HB_LLVM_SYSROOT`. |
| `g++-aarch64-linux-gnu`, `qemu-user` | The hosted AArch64 toolchain and emulator for the unit tests. | `Dockerfile`, used by `cmake/aarch64-linux-toolchain.cmake` |
| CMake 3.25 or newer, `make`, `ninja-build` | The build system. The presets use the `Unix Makefiles` generator. | `cmake_minimum_required` in `CMakeLists.txt`, packages in `Dockerfile` |
| Ubuntu 24.04 | Base image. | `Dockerfile` |

`HB_LLVM_SYSROOT` is read from the environment first, then from the cache, then from `.toolchain/arm-newlib-19.1.5/...` if that folder exists. The `Dockerfile` sets it to `/opt/hyperberry/lib/clang-runtimes/newlib/aarch64-none-elf/aarch64a`. Configure fails if any of the libc++ headers or archives is missing.

# Tools the build runs

| What | Used for | Where |
|---|---|---|
| `dtc` (package `device-tree-compiler`) | Compiles each guest tree to `guest.dtb`. | `CMakeLists.txt` requires it with `find_program(DTC dtc REQUIRED)` |
| `cpio` | Packs the guest archives and the BusyBox root. | `CMakeLists.txt` requires it |
| Python 3 | Runs everything in `scripts/`. Standard library only. | `Dockerfile` |
| `qemu-system-aarch64` (package `qemu-system-arm`) | Runs `run-qemu` and the integration suite. | `Dockerfile`, flags in `CMakeLists.txt` and `run_qemu_integration.py` |
| `just` | Command runner. | `Dockerfile` for CI, host install for the recipes |
| `ipxe-qemu` | Installed in the image. No build file mentions it. | `Dockerfile` |

# Downloaded at build time

| What | Fetched by | Pin |
|---|---|---|
| Static BusyBox, Debian `busybox-static_1.38.0-3+b1_arm64.deb` | `scripts/busybox/fetch.py` | `VERSION` and `SHA256` in the file |
| Debian arm64 netboot `linux` image, snapshot 20260908T000000Z | `scripts/linux/fetch.py` | `URL` and `SHA256` in the file |
| GoogleTest v1.14.0 | `FetchContent_Declare` in `CMakeLists.txt`, only when `BUILD_TESTING=ON` | The tag in the URL |

# Docs and CI

- `just docs` needs `doxygen` and a virtual environment at `docs/.venv` holding `sphinx`, `breathe` and `furo`. The list is in `docs/sphinx/requirements.txt`. The docs workflow installs the same three by name instead of reading that file.
- `minicom` is only for reading the Pi's serial console.
- The CI image is `ghcr.io/matthewchavis8/hyperberry-toolchain:llvm-22.1.2`, built from the `Dockerfile` by `.github/workflows/toolchain.yml` when the `Dockerfile` or that workflow changes. The tag is written in the three workflow files that use it, and the `setup-ci` action only checks that Clang 22 and the libc++ files are in the image.

# Gotchas

- The Clang version is `22` in the `Dockerfile`, `22.1` in the CMake check and `22.1.2` in the image tag. Bumping Clang means touching all three.
- The newlib archive checksum is written twice, in `Dockerfile` and in `fetch_sysroot.py`.
- The README says the doc dependencies install with `pip install -r docs/requirements.txt`. That file does not exist. See [layout oddities](layout-oddities.md).
