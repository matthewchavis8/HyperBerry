---
type: Architecture
title: Build
description: How one configure builds every board, the presets, the just recipes, how the guest tree and guest archive are made, where regs.inc comes from, and how Docker wraps it all.
resource: ../../CMakeLists.txt
tags: [architecture, build]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../README.md
  - resource: ../../CMakeLists.txt
  - resource: ../../CMakePresets.json
  - resource: ../../justfile
  - resource: ../../Dockerfile
  - resource: ../../.dockerignore
  - resource: ../../cmake/aarch64-toolchain.cmake
  - resource: ../../cmake/aarch64-linux-toolchain.cmake
  - resource: ../../core/CMakeLists.txt
  - resource: ../../scripts/toolchain/container.sh
  - resource: ../../scripts/bspgen/bspgen.py
  - resource: ../../scripts/cpio/archive.py
---

# The short version

CMake is in charge, and `just` wraps it. One configure builds every board, so there is no board option. Each board gets its own copy of every library and its own image. Images land under `build/<mode>/<board>/`, where `<mode>` is `debug` or `release`.

```sh
just build              # every board, debug
just build release      # every board, release
just qemu               # build and run in QEMU
just rpi5               # build and flash an SD card
just test-unit
just test-integration   # defaults to qemu
```

# Presets

`CMakePresets.json` has three configure presets, each with a build preset of the same name.

| Preset | Directory | Notes |
|---|---|---|
| `debug` | `build/debug` | `Debug`, `BUILD_INTEGRATION=ON`. Uses `cmake/aarch64-toolchain.cmake`. |
| `release` | `build/release` | `Release`, `BUILD_INTEGRATION=OFF`. Same toolchain file. |
| `unit-tests` | `build/unit-tests` | `Debug`, `BUILD_TESTING=ON`, `clang++-22`, uses `cmake/aarch64-linux-toolchain.cmake`. Also has a test preset that prints output on failure. |

# Just recipes

| Recipe | What it runs |
|---|---|
| `build MODE=debug` | `cmake --preset MODE`, then `cmake --build --preset MODE`, inside the container. |
| `qemu MODE=debug` | Configure and build, then the `run-qemu` target, with a terminal attached. |
| `rpi5 MODE=release SD_DEV=/dev/sdd1` | Configure, build `hyperberry-rpi5`, then `scripts/rpi5/flash.sh` with the image, `guest.cpio` and the partition. |
| `test-unit` | Configure, build and `ctest` with the `unit-tests` preset. |
| `test-integration BOARD=qemu` | Configure `debug`, build `hyperberry-<BOARD>-test`, build `run-<BOARD>-test`. |
| `cpio ROOT OUT` | `scripts/cpio/archive.py --root ROOT --out OUT`. |
| `compile-db` | Configure `debug` and link `build/debug/compile_commands.json` at the repo root. |
| `docs` | Doxygen, then `docs/.venv/bin/sphinx-build`, then serves `docs/_build/html` on port 8000. Runs on the host, not in Docker. |
| `docs-clean`, `clean` | Remove `docs/_doxygen` and `docs/_build`, or `build/`. |

# What one configure does

For each name in `SUPPORTED_BOARDS` (`rpi5 qemu`) the top level `CMakeLists.txt`:

1. Sets `BSP_BOARD`, `BSP_SUFFIX` (`-<board>`), `BSP_DIR` (`bsp/<board>`) and `BSP_GEN` (`build/<mode>/<board>/generated`).
2. Runs `scripts/bspgen/bspgen.py` on the board's host tree to write `regs.inc` into `BSP_GEN`. This happens at configure time with `execute_process`, so it fails the configure if it fails. See [device tree to regs.inc](../flows/device-tree-to-regs.md).
3. Sets the include path to the repo root, `bsp/<board>` and `BSP_GEN`. That is what selects the board's constants. See [no board macro](../decisions/no-board-macro.md).
4. Adds `lib`, `drivers` and `core` as subdirectories named `<dir>-<board>`. See [layering](layering.md).
5. Calls `hb_add_image` for `hyperberry-<board>`, and with `BUILD_INTEGRATION=ON` adds `tests/integration` and a second image, `hyperberry-<board>-test`.

`hb_add_image` makes an executable from `bsp/<board>/boot.S` and the three entry assembly files, links the libraries inside one `--start-group`, and passes `-T bsp/<board>/linker.ld -nostdlib -static -Wl,--gc-sections -fuse-ld=lld`. After linking, `llvm-objcopy -O binary` writes `kernel8.img` next to the ELF. The normal image goes to `build/<mode>/<board>/` and the test image to `build/<mode>/<board>/integration/`.

The compiler is Clang 22.1 or newer with the target `aarch64-none-elf`, and the build refuses to configure with anything else. Compile flags for every target include `-ffreestanding -fno-exceptions -fno-rtti`. The per mode flags, `-O0 -g3 -gdwarf-4` for Debug and `-Os -DNDEBUG` for Release, are set by `hb_add_image` with `target_compile_options` on the image target itself.

# The offsets header

`core/CMakeLists.txt` compiles `core/vcpu/vcpuOffsets.cpp` to assembly text, and `scripts/asmoffsets/asmoffsets.py` turns that into `vcpuOffsets.h` in `BSP_GEN`. Each image depends on the `VcpuOffsetsHeader-<board>` target. See [offsets from layout](../decisions/offsets-from-layout.md).

# The guest tree and the guest archive

For each board:

1. `dtc -I dts -O dtb -i boot/dts -o build/<mode>/<board>/guest.dtb bsp/<board>/dts/guest-<board>.dts`. The `-i boot/dts` is how `/include/ "guest-linux.dtsi"` is found.
2. `scripts/cpio/archive.py` packs `linux/Image`, `linux/guest.dtb` and `linux/initrd` into `build/<mode>/<board>/guest.cpio`. The target is `guest-archive-<board>`, and `hyperberry-<board>` depends on it.
3. The test archive, `build/<mode>/<board>/integration/guest.cpio`, has the same three files plus `tests/vcpu.bin`, `tests/gic.bin` and `tests/abort.bin`. The target is `test-archive-<board>`. The three payloads are assembled from `tests/integration/<name>/guest_payload.S` with `tests/integration/guest/payload.ld` by the `test-payloads` target.

The kernel comes from `QEMU_GUEST_KERNEL` or `RPI5_GUEST_KERNEL`. If unset, QEMU fetches a pinned Debian kernel with `scripts/linux/fetch.py` into `build/<mode>/linux/qemu/Image`, and the Pi uses the `Image` file at the repo root. The initrd comes from `QEMU_GUEST_INITRD` or `RPI5_GUEST_INITRD`. If unset, a BusyBox root is built by `scripts/busybox/fetch.py` and `rootfs.py`. See [guest archive](../guides/GUEST_ARCHIVE.md) and [scripts](../subsystems/scripts.md).

# Run and flash targets

- `run-qemu` starts `qemu-system-aarch64` with `-machine virt,virtualization=on,gic-version=2 -cpu cortex-a76 -m 8G -nographic`, the QEMU image as `-kernel` and the guest archive as `-initrd`.
- `run-qemu-test` runs `.github/actions/run-qemu-integration/run_qemu_integration.py` on the test image. See [integration run](../flows/integration-run.md).
- `flash-rpi5` and `flash-rpi5-test` copy the image, archive and four firmware files to `SD_MOUNT`, which defaults to `/mnt/sdcard`. The recipe `just rpi5` uses `flash.sh` instead.

# Docker

Every container backed recipe goes through `scripts/toolchain/container.sh`. It builds `hyperberry-toolchain:local` from the `Dockerfile` (Docker reuses unchanged layers), then runs the command as the calling user with the checkout mounted at `/workspace`. Inside GitHub Actions it skips Docker and runs the command directly, because the job already runs in the published toolchain image. The `.dockerignore` excludes everything except the `Dockerfile`, so building the image sends almost no context. See [dependencies](dependencies.md).

# Gotchas

- The output file is named `hyperberry-<board>`, not `hyperberry.elf`. `set(ELF hyperberry.elf)` in the top level file is never used. The README says the build produces `hyperberry.elf`.
- A cmake cache written by a host build is deleted by `container.sh` before the first container build, because it points at host paths.
- The `unit-tests` configure returns before the board loop. It generates `regs.inc` for `qemu` only, into `build/unit-tests/qemu/generated`.
- `FDTPUT` is looked up with `find_program` but nothing uses it.
- `just test-integration` accepts a board name, but only `qemu` has a `run-<board>-test` target. For the Pi use `flash-rpi5-test`.
