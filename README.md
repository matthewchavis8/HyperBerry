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

- LLVM 22.1 or newer with `clang` and `clang++`
- `llvm` (LLVM)
- `cmake` (>= 3.25)
- `just` command runner
- `qemu-system-aarch64` for virtualized hardware
- `minicom` for UART serial console
- `doxygen` (for generating XML used by Sphinx)
- `python3` / `pip` (install doc deps with `pip install -r docs/requirements.txt`)

### C++26 Toolchain

HyperBerry is built as C++26 with LLVM Clang 22.1 or newer. Hosted unit tests
use the `aarch64-linux` toolchain. Bare metal builds use the
`aarch64-none-elf` toolchain in `cmake/aarch64-toolchain.cmake`, Arm newlib,
and libc++.

Fetch the pinned bare metal sysroot, then configure normally:

```sh
python3 tools/toolchain/fetch_sysroot.py
export HB_LLVM_SYSROOT="$PWD/.toolchain/arm-newlib-19.1.5/lib/clang-runtimes/newlib/aarch64-none-elf/aarch64a"
cmake --preset debug
```

The sysroot supplies libc++, libc++abi, libunwind, compiler rt, and newlib.
Clang 22 provides the C++26 language mode. CI uses the pinned
`ghcr.io/matthewchavis8/hyperberry-toolchain:llvm-22.1.2` image.

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

## Guest CPIO archives

Every guest payload is delivered in an uncompressed `newc` CPIO archive.
CMake builds `build/<mode>/<board>/guest.cpio` from the Linux `Image` and
the board's guest device tree. Integration archives live under
`build/<mode>/<board>/integration/guest.cpio` and also contain the vCPU and GIC binaries.

The host needs Python 3, `cpio`, and `dtc`. FVP also needs `fdtput`.
QEMU defaults to a pinned Debian AArch64 Linux `Image` with initramfs support.
Set `QEMU_GUEST_KERNEL`, `RPI5_GUEST_KERNEL`, or `FVP_GUEST_KERNEL` to
select another kernel. CMake downloads and verifies a static AArch64 BusyBox
binary, then places its initramfs in the guest archive. Set the corresponding
`*_GUEST_INITRD` to replace that BusyBox initramfs. Kernels supplied for RPi5
or FVP must enable `CONFIG_BLK_DEV_INITRD`.

Package your own directory with:

```sh
just cpio path/to/root path/to/guest.cpio
```

See [Guest archives](docs/GUEST_ARCHIVE.md) for paths, format rules, and boot behavior.

## Testing

HyperBerry has two test paths:

- Unit tests under `tests/unit/` use GoogleTest with the hosted `aarch64-linux` toolchain and run through `ctest`.
- Integration tests under `tests/integration/` are linked into the bare-metal image and report results over UART.

Run them with:

```sh
just test-unit
just test-integration qemu
cmake --build --preset debug --target flash-rpi5-test
```

The integration build adds a `hyperberry-<board>-test` image alongside each normal one, enables `INTEGRATION_TEST=ON`, and swaps the normal EL2 entry path for `TestRunner::RunAll()`. Full testing notes, layout, and extension instructions live in `docs/TESTING.md`.

## License

Apache 2.0 — see [LICENSE](LICENSE).
