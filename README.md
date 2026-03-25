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
- `doxygen` (optional, for generating API docs)

### Toolchain

The cross-compilation toolchain is defined in `cmake/aarch64-toolchain.cmake` and targets `aarch64-none-elf` (bare-metal, no libc).

### Build Flags

```
-ffreestanding -fno-exceptions -fno-rtti -std=c++17 -nostdlib -static
```

### Build Outputs

The build produces `hyperberry.elf`, which is then converted to `kernel8.img` (raw binary) via `llvm-objcopy -O binary`.

NOTE: the rpi5 firmware looks for kernel8.img in order to boot it

## Quick Start

| Command      | Description                                                                 |
|--------------|-----------------------------------------------------------------------------|
| `just qemu`  | Configure, build, and launch the hypervisor on a virtualized RPi5 (QEMU)    |
| `just rpi5`  | Build and flash the hypervisor on physical Raspberry Pi 5 hardware(SOON)    |
| `just docs`  | Generate Doxygen HTML documentation in `docs/doxygen/html/`                 |
| `just clean` | Remove all build artifacts                                                  |

## License

Apache 2.0 — see [LICENSE](LICENSE).
