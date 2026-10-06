---
type: Subsystem
title: Boards (bsp)
description: What each board folder owns, the boot assembly and linker script every image starts from, the device trees and Pi firmware, and the one shared guest tree that still lives in boot/.
resource: ../../bsp/
tags: [bsp, boot, boards]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../CMakeLists.txt
  - resource: ../../bsp/qemu/boot.S
  - resource: ../../bsp/qemu/linker.ld
  - resource: ../../bsp/qemu/dts/guest-qemu.dts
  - resource: ../../bsp/rpi5/boot.S
  - resource: ../../bsp/rpi5/linker.ld
  - resource: ../../bsp/rpi5/dts/guest-rpi5.dts
  - resource: ../../bsp/rpi5/firmware/config.txt
  - resource: ../../boot/dts/guest-linux.dtsi
  - resource: ../../scripts/bspgen/bspgen.py
  - resource: ../../core/deviceTree/deviceTree.cpp
---

# What it is

`bsp/<board>/` holds everything that differs between boards. There are two boards, `qemu` (the QEMU `virt` machine, with a Cortex A76 CPU) and `rpi5` (the Raspberry Pi 5, BCM2712). Each folder has a `boot.S` that takes the CPU from reset to `hmain`, a `linker.ld` that places the image in memory, and device trees. The `rpi5` folder also has the firmware files the Pi's GPU needs to start the image.

There is no C++ and no board macro here. The build puts `bsp/<board>` and that board's generated directory on the include path, so the same source includes `"regs.inc"` and gets that board's addresses. See [no board macro](../decisions/no-board-macro.md).

The guest device tree has two halves. `bsp/<board>/dts/guest-<board>.dts` is the board half. The part shared by every board is `boot/dts/guest-linux.dtsi`, which is not under `bsp/` yet. GitHub issue 32 plans to move it into `bsp/`. Until it moves, the build reads it from `boot/dts`, which is listed in [layout oddities](../architecture/layout-oddities.md).

# Depends on

- [Core](../architecture/layering.md): `boot.S` branches to `hmain` in `core/main.cpp` and loads `el2_vectors` from `core/vmm/exceptions/vectors.S`. Both are linked into the image next to `boot.S`.
- [scripts](scripts.md): `scripts/bspgen` reads the host tree in each board folder and writes `regs.inc`.
- The linker script names symbols other code reads: `__bss_start`, `__bss_end`, `__stack_end`, `__init_array_start`, `__init_array_end`, `__test_suites_start`, `__test_suites_end`.

# Used by

- The top level `CMakeLists.txt` links `bsp/<board>/linker.ld` with `-T` and lists `bsp/<board>/boot.S` as a source of every image for that board.
- [Device tree](device-tree.md): the host tree of each board is the one `bspgen` reads, and the guest tree is the one the boot loader hands to Linux.
- [Boot loader](boot-loader.md): reads `linux/guest.dtb`, which is built from the guest tree here.
- [drivers](drivers.md) include `regs.inc`. `drivers/uart/uart.cpp` and `drivers/gic/gic.cpp` read the `BSP_` constants, and `core/deviceTree/deviceTree.cpp` compares them with the firmware tree.

# Files

| Path | What it is |
|---|---|
| `bsp/qemu/boot.S` | Reset code for QEMU `virt`. Entry symbol `_start` in section `.text.start`. |
| `bsp/qemu/linker.ld` | One `RAM` region at `0x40080000`, length `0xC0000000 - 0x80000`. |
| `bsp/qemu/dts/guest-qemu.dts` | Guest tree: GIC at `0x08000000`, PL011 at `0x09000000`, console `ttyAMA0`. |
| `bsp/qemu/dts/host-qemu.dtb` | A committed capture of the host tree QEMU gives the image. Input to `bspgen` for `qemu`. |
| `bsp/rpi5/boot.S` | Reset code for the Pi 5. Same shape as the QEMU file, and it also clears the SP alignment check. |
| `bsp/rpi5/linker.ld` | A `RESERVED` region of 512K at 0, then `RAM` at `0x00080000`, length `0x200000000 - 0x00080000`. |
| `bsp/rpi5/dts/guest-rpi5.dts` | Guest tree: GIC at `0x107fff9000`, PL011 at `0x107d001000`, console `ttyAMA0`. |
| `bsp/rpi5/firmware/bcm2712-rpi-5-b.dtb` | The firmware's device tree. It is the host tree `bspgen` reads for `rpi5`, and it is copied to the SD card. |
| `bsp/rpi5/firmware/config.txt` | Pi boot config: `arm_64bit=1`, `kernel=kernel8.img`, `initramfs guest.cpio followkernel`, `disable_commandline_tags=1`, `gpu_mem=16`, `enable_uart=1`, `uart_2ndstage=1`. |
| `bsp/rpi5/firmware/start4.elf`, `fixup4.dat` | GPU firmware the Pi needs to boot. Copied to the SD card with the image. |
| `boot/dts/guest-linux.dtsi` | Guest tree nodes common to every board: one Cortex A76 CPU with PSCI by `hvc`, placeholder `memory@0`, the architected timer, and placeholder initrd addresses in `chosen`. |

The generated `regs.inc` is not in the source tree. It is written to `build/<mode>/<board>/generated/regs.inc` at configure time. See [device tree to regs.inc](../flows/device-tree-to-regs.md).

# Entry points

- `_start` in each `boot.S`. The firmware or QEMU jumps here with the host device tree address in `x0`. In order, it:
  1. saves `x0` in `x19`;
  2. parks every core except core 0 in a `wfe` loop;
  3. parks core 0 too if `CurrentEL` is not EL2;
  4. sets `HCR_EL2` bit 31 (RW) so guests run in AArch64;
  5. turns off the MMU and caches in `SCTLR_EL2` (the Pi file also clears the SP alignment check, bit 3);
  6. sets `CNTHCTL_EL2` bits 0 and 1 to let EL1 use the physical counter and timer, and zeroes `CNTVOFF_EL2`;
  7. copies `MIDR_EL1` to `VPIDR_EL2` and `MPIDR_EL1` to `VMPIDR_EL2`;
  8. zeroes `.bss` from `__bss_start` to `__bss_end`;
  9. selects `SP_EL2`, sets `sp` to `__stack_end` and `VBAR_EL2` to `el2_vectors`;
  10. restores `x0` from `x19` and calls `hmain`.
- The linker scripts lay out `.text` (with `.text.start` and `.text.vectors` kept first), `.rodata`, `.init_array`, `.hyperberry_tests`, `.data`, `.bss`, a 4 MB `.stack` and a 1 MB `.uncached_space`.
- The guest tree is turned into `build/<mode>/<board>/guest.dtb` by `dtc` with `-i boot/dts`. Both board files start with `/include/ "guest-linux.dtsi"`.

# Gotchas

- Both guest trees declare only the first two GIC regions, the distributor and the CPU interface. They leave out the hypervisor frames on purpose, because stage 2 maps only what the tree declares. A comment in each file says so.
- `guest-linux.dtsi` holds placeholders that the boot loader patches at load time: `memory@0` `reg`, and `linux,initrd-start` and `linux,initrd-end`.
- The host tree for `qemu` sits in `dts/` and the one for `rpi5` sits in `firmware/`. Both are inputs to `bspgen`, and CMake names them in `HOST_DTB_qemu` and `HOST_DTB_rpi5`.
- At boot, `TreeParser::GetHostMmio` compares the generated `BSP_` constants with the firmware tree and calls `HvPanic` if they differ. A stale `host-qemu.dtb` or a changed firmware tree therefore stops the boot rather than corrupting it.
- `bsp/rpi5/linker.ld` disagrees with itself. Its header says the firmware loads `kernel8.img` to `0x00200000` and that the first 32 KB are reserved, while the `MEMORY` block says `0x00080000` and a 512K reserved region. The `MEMORY` block is what the linker uses.
- Core 0 is the only core that runs. The Pi `boot.S` has a warning that waking cores 1 to 3 would need a spin table or PSCI.
- `AGENTS.md` lists a `bsp.h` per board. No such file exists. Commit d4ddde1 deleted them and their users include `regs.inc` directly.
- Adding a board means a new folder here and a new name in `SUPPORTED_BOARDS`, a `HOST_DTB_<board>` line and a `BOARD_DESC_<board>` line in `CMakeLists.txt`. The guest tree must be named `guest-<board>.dts`, because the build derives the path from the board name.
