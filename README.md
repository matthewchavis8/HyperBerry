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

## Build & Toolchain

**Requirements**

- `clang++` with `--target=aarch64-none-elf`
- `ld.lld` (LLVM linker)
- `llvm-objcopy` for image conversion
- `aarch64-none-elf-toolchain` for cross compilation for the ARM board
- `qemu-aarch64` for running unit test
- `qemu-user` unit test need a virtual libc

**Build flags**

```
-ffreestanding -fno-exceptions -fno-rtti -std=c++14
```

**Deploy**

Copy the resulting `kernel8.img` to the boot partition of a Raspberry Pi 5 SD card.

## License

Apache 2.0 — see [LICENSE](LICENSE).
