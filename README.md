<img width="1536" height="1024" alt="HyperVLogo" src="https://github.com/user-attachments/assets/7c128dd6-837f-4e4b-8a20-92f075da0a0c" />

# HyperBerry

A Type-1 bare-metal hypervisor for the Raspberry Pi 5, written in C++ and a bit of AArch64 assembly.

## Overview

HyperBerry runs directly on the BCM2712 SoC with no host OS, entering EL2 at boot and using Armv8-A hardware-assisted virtualization to host multiple isolated guest operating systems concurrently.

## Target Hardware

| Feature              | Detail                                      |
|----------------------|---------------------------------------------|
| SoC                  | Broadcom BCM2712                            |
| CPU                  | 4x Arm Cortex-A76 (AArch64)                 |
| RAM                  | 4 GB / 8 GB LPDDR4X                         |
| Interrupt Controller | GIC-400 (GICv2)                             |
| UART                 | PL011 (debug console)                       |
| Timer                | Arm Generic Timer (CNTPCT_EL0)              |
| Boot                 | Raspberry Pi firmware → kernel8.img at EL2  |

## Architecture

```
┌──────────────────────────────────────────────────┐
│                   Guest OS (EL1)                 │
│              Linux / RTOS / bare-metal           │
├──────────────────────────────────────────────────┤
│               HyperBerry (EL2)                   │
│  ┌───────────┬───────────┬──────────┬──────────┐ │
│  │  vCPU     │  Stage-2  │  vGIC    │  vTimer  │ │
│  │  Sched    │  MMU      │          │          │ │
│  └───────────┴───────────┴──────────┴──────────┘ │
├──────────────────────────────────────────────────┤
│               Hardware (EL3 / TF-A)              │
└──────────────────────────────────────────────────┘
```

HyperBerry runs at EL2. Guest operating systems run at EL1, where they remain isolated from each other. The hypervisor’sjob is to make each guest OS believe it owns the underlying hardware, even thougheach one actually controls only a smallslice of it. In addition, the hypervisor is responsible for scheduling, VM isolation, and enforcing security between
guests and firmware.

## Memory Management

HyperBerry includes an early-boot physical page allocator implemented with the
buddy allocation algorithm in `core/mm/pmm/`. The allocator is initialized
from the firmware DTB, reserves the hypervisor image, TF-A, and the DTB blob,
and provides physically contiguous page blocks from 4 KiB up to 8 MiB.

That allocator now backs shared page-table helpers, the EL2 host stage-1 MMU,
and per-VM stage-2 mappings. Guest loading and richer VM memory ownership are
still in progress, and the current layout is documented in
`docs/sphinx/memory.rst`.

## Project Structure

```
HyperBerry/
├── bsp/            # One folder per board: constants, boot.S, linker.ld, dts, firmware
├── cmake/          # Cross-compilation toolchain and CMake configuration
├── core/           # Hypervisor core, each subsystem with its own entry assembly
├── drivers/        # Software device drivers
├── lib/            # C++ utility headers
├── tests/          # integration/ (board-neutral), bsp/<board>/ (board-specific), unit/
├── docs/           # extra docs
├── CMakeLists.txt
└── justfile        # Quick command runner
```

## Build & Toolchain

### Requirements

- `clang` / `clang++` with `--target=aarch64-none-elf`
- `llvm` (LLVM)
- `cmake` (>= 3.16)
- `just` command runner
- `qemu-system-aarch64` for virtualized hardware
- `minicom` for UART serial console
- `doxygen` (for generating XML used by Sphinx)
- `python3` / `pip` (install doc deps with `pip install -r docs/requirements.txt`)

### Toolchain

The cross-compilation toolchain is defined in `cmake/aarch64-toolchain.cmake` and targets `aarch64-none-elf` (bare-metal, no libc).

### Build Outputs

The build produces `hyperberry.elf`, which is then converted to `kernel8.img` (raw binary) via `llvm-objcopy -O binary`.

## Quick Start

| Command                                  | Description                                |
|------------------------------------------|--------------------------------------------|
| `just build (debug/release)`            | Build every board's image                  |
| `just qemu (debug/release)`              | Build and run in QEMU                      |
| `just rpi5 (debug/release) [/dev/sdX1]`  | Build and flash SD card for Pi 5           |
| `just test-unit`                         | Build and run hosted GoogleTest unit tests |
| `just test-integration (qemu/rpi5)`      | Build and run bare-metal integration tests |
| `just docs`                              | Generate and serve Sphinx + Breathe docs   |
| `just clean`                             | Remove build artifacts                     |

### QEMU

**Quick start:**

```sh
just qemu          # debug build (default)
just qemu release  # release build
```

**Manual steps (without `just`):**

```sh
# 1. Configure
cmake --preset debug

# 2. Build all boards (image at build/debug/qemu/kernel8.img)
cmake --build --preset debug

# 3. Spin up virtual RPI5 with hyperBerry image
qemu-system-aarch64 \
  -machine virt,virtualization=on,gic-version=2 \
  -cpu cortex-a76 \
  -m 4G \
  -nographic \
  -kernel build/debug/qemu/kernel8.img
```

UART output prints directly to the terminal. Exit QEMU with `Ctrl-A X`.

### Raspberry Pi 5

1. Format an SD card as FAT32.
2. Plug the SD card into the host and identify the partition (e.g. `/dev/sda1`).

**Quick start:**

```sh
just rpi5                          # release build, default /dev/sda1
just rpi5 release /dev/sdX1        # specify a different partition
```

**Manual steps (without `just`):**

```sh
# 1. Configure
cmake --preset release

# 2. Build all boards (image at build/release/rpi5/kernel8.img)
cmake --build --preset release

# 3. Mount and flash
sudo mkdir -p /mnt/sdcard
sudo mount -o uid=$(id -u),gid=$(id -g) /dev/sda1 /mnt/sdcard

cp build/release/rpi5/kernel8.img        /mnt/sdcard/
cp bsp/rpi5/firmware/start4.elf          /mnt/sdcard/
cp bsp/rpi5/firmware/bcm2712-rpi-5-b.dtb /mnt/sdcard/
cp bsp/rpi5/firmware/config.txt          /mnt/sdcard/
cp bsp/rpi5/firmware/fixup4.dat          /mnt/sdcard/

sudo umount /mnt/sdcard
```

3. Insert the SD card into the Pi 5 and power on.

### UART Debugging

Hardware: [Waveshare Pi UART Debugger](https://www.waveshare.com/wiki/Pi_UART_Debugger) with a 3-pin to 3-pin cable.

<!-- TODO: replace with your uploaded GitHub image URL -->
<!-- <img width="600" alt="3-pin to 3-pin cable" src="YOUR_GITHUB_IMAGE_URL_HERE" /> -->

1. Connect the debugger's 3-pin header straight to the Pi 5's UART slot.
2. Plug the debugger's USB side into the host machine.
3. Find the serial device:

```sh
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

The board may appear as either a `/dev/ttyUSB*` or `/dev/ttyACM*` device, depending on the USB-to-UART adapter/driver.

4. Open a serial console:

```sh
minicom -b 115200 -D /dev/ttyUSB0   # Example
# or
minicom -b 115200 -D /dev/ttyACM0   # Example
```

## Building Guest Boot Packages (`.hvgbp`)

HyperBerry expects a firmware-loaded guest package (`.hvgbp`) for Linux guest boot.
Use the Rust CLI at `tools/mkguestpkg` to build it from a kernel `Image` and guest DTB.
This package is required because HyperBerry consumes one firmware-provided blob at boot
that bundles guest kernel + DTB (+ optional initrd) with offsets/CRC metadata the loader validates.

```sh
cargo run --manifest-path tools/mkguestpkg/Cargo.toml -- \
  --kernel path/to/Image \
  --dtb path/to/guest.dtb \
  --out boot/profiles/guest-qemu.hvgbp
```

Or use the `just` wrapper:

```sh
just guestpkg path/to/Image path/to/guest.dtb
```

Optional initrd + build id:

```sh
just guestpkg path/to/Image path/to/guest.dtb boot/profiles/guest-qemu.hvgbp path/to/rootfs.cpio.gz my-build-id
```

For ABI/layout details, see `docs/GUEST_BOOT_PACKAGE.md`.

## Testing

HyperBerry has two test paths:

- Unit tests under `tests/unit/` use GoogleTest with the hosted `aarch64-linux` toolchain and run through `ctest`.
- Integration tests under `tests/integration/` are linked into the bare-metal image and report results over UART.

Run them with:

```sh
just test-unit
just test-integration qemu
just test-integration rpi5 /dev/sdX1
```

The integration build adds a `hyperberry-<board>-test` image alongside each normal one, enables `INTEGRATION_TEST=ON`, and swaps the normal EL2 entry path for `TestRunner::RunAll()`. Full testing notes, layout, and extension instructions live in `docs/TESTING.md`.

## AI Use Declaration

AI tools (claude code) are used heavily here now, for implementation as much as
documentation: C++, assembly, CMake, host tooling, tests and docs. This section
used to say AI was limited to documentation and that every line of hypervisor
code was mine. That stopped being true, and a stale claim in a README is worse
than an honest one.

My view of the tradeoff has not changed. Leaning on AI trades knowledge depth for
speed. In industry that makes sense, fast paced teams prioritise delivery and
engineers juggle several things at once. The cost is less cognitive engagement
when you offload too much, which I do not think is inherently bad, just a
tradeoff like everything else in software. What changed is that I now want the
speed, and I read and review what lands instead of pretending I typed it.

You will probably still find messy code and choices that are not best practice,
but hey, I am getting the job done and having fun.

## License

Apache 2.0 — see [LICENSE](LICENSE).
