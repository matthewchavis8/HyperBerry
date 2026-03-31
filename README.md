<img width="1536" height="1024" alt="HyperVLogo" src="https://github.com/user-attachments/assets/7c128dd6-837f-4e4b-8a20-92f075da0a0c" />

# HyperBerry

A Type-1 bare-metal hypervisor for the Raspberry Pi 5, written in freestanding C++ and AArch64 assembly.

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

HyperBerry occupies EL2 (hypervisor exception level). Guest operating systems run at EL1 and are isolated from each other via Stage-2 address translation, with virtual interrupts and timers managed by the hypervisor.

## Project Structure

```
HyperBerry/
├── arch/       # AArch64 boot assembly and platform-specific startup
├── cmake/      # Cross-compilation toolchain and CMake configuration
├── core/       # Hypervisor core (scheduler, memory, exception handling)
├── drivers/    # Software device drivers
├── lib/        # Freestanding C++ utility headers (hv::array, register dump)
├── linker/     # Platform-specific linker scripts and memory layouts
├── tests/      # Test infrastructure
├── docs/       # extra docs like roadmap and such
├── CMakeLists.txt
└── justfile    # Quick command runner
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

| Command                                  | Description                              |
|------------------------------------------|------------------------------------------|
| `just qemu (debug/release)`              | Build and run in QEMU                    |
| `just rpi5 (debug/release) [/dev/sdX1]`  | Build and flash SD card for Pi 5         |
| `just docs`                              | Generate and serve Sphinx + Breathe docs |
| `just clean`                             | Remove build artifacts                   |

### QEMU

**Quick start:**

```sh
just qemu          # debug build (default)
just qemu release  # release build
```

**Manual steps (without `just`):**

```sh
# 1. Configure
cmake --preset debug -DBOARD=qemu

# 2. Build
cmake --build --preset debug

# 3. Spin up virtual RPI5 with hyperBerry image
qemu-system-aarch64 \
  -machine virt,virtualization=on,gic-version=2 \
  -cpu cortex-a76 \
  -m 4G \
  -nographic \
  -kernel build/debug/kernel8.img
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
cmake --preset release -DBOARD=rpi5 -DSD_MOUNT=/mnt/sdcard

# 2. Build
cmake --build --preset release

# 3. Mount and flash
sudo mkdir -p /mnt/sdcard
sudo mount -o uid=$(id -u),gid=$(id -g) /dev/sda1 /mnt/sdcard

cp build/release/kernel8.img       /mnt/sdcard/
cp boot/start4.elf                 /mnt/sdcard/
cp boot/bcm2712-rpi-5-b.dtb        /mnt/sdcard/
cp boot/config.txt                 /mnt/sdcard/
cp boot/fixup4.dat                 /mnt/sdcard/

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
```

4. Open a serial console:

```sh
minicom -b 115200 -D /dev/ttyUSB*   # Make sure it is the correct USB
```

## AI Use Declaration

AI tools (primarily large language models) were used for **documentation only** —
including Doxygen comments, README text, and design notes in this file.
All hypervisor source code (assembly, C++, linker scripts, and build system
configuration) was written by hand.

## License

Apache 2.0 — see [LICENSE](LICENSE).
