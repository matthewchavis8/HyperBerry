---
type: Flow
title: Device tree to regs.inc
description: How a board's host device tree becomes the constants the code compiles against, and how boot checks those constants against the tree the firmware really hands over.
resource: ../../scripts/bspgen/
tags: [flow, device-tree, build, bsp]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../CMakeLists.txt
  - resource: ../../scripts/bspgen/bspgen.py
  - resource: ../../drivers/uart/uart.cpp
  - resource: ../../drivers/gic/gic.cpp
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../core/main.cpp
---

# Summary

Addresses come from device trees, not from source. Two paths exist. The first runs at configure time: `scripts/bspgen` reads a board's committed host tree and writes `regs.inc`, a header of `#define` lines for the UART and the GIC frames. These must be compile time constants, because the early console has to work before any tree is parsed and the GIC register tables are constexpr. The second runs at boot: `TreeParser` reads the tree the firmware passes in and compares it with those constants. Everything else, including the MMIO windows both MMU layers map, is read from a tree at runtime.

See [device tree is the source of truth](../decisions/device-tree-is-source-of-truth.md) and [device tree](../subsystems/device-tree.md).

# Steps

1. **Pick the host tree.** `CMakeLists.txt` sets `HOST_DTB_rpi5` to `bsp/rpi5/firmware/bcm2712-rpi-5-b.dtb` and `HOST_DTB_qemu` to `bsp/qemu/dts/host-qemu.dtb`. The `qemu` file is a capture of the tree QEMU provides. The unit test configure uses the `qemu` one too.
2. **Run `hb_generate_regs`.** For each board it runs `scripts/bspgen/bspgen.py --dtb <host dtb> --board <board> --out build/<mode>/<board>/generated/regs.inc` with `execute_process`. A nonzero exit stops the configure with the script's message.
3. **Parse the tree.** `bspgen` reads the flattened tree into nodes with their properties. It decodes each `reg` with the parent's `#address-cells` and `#size-cells`, then walks up through each parent's `ranges` to turn a bus local address into a CPU physical one. The Pi needs this because its GIC and UART sit under a `/soc` node whose `ranges` add the high bits.
4. **Find the nodes.** It takes the first node whose `compatible` matches a GIC name (`arm,gic-400`, `arm,cortex-a15-gic`, `arm,gic-v2`, `arm,arm11mp-gic`, `arm,gic-v3`) and the first that matches a UART name (`arm,pl011`, `brcm,bcm2835-aux-uart`).
5. **Name the regions.** A GICv2 `reg` is read as distributor, CPU interface, hypervisor frame and virtual CPU frame. A GICv3 `reg` is read as distributor and redistributor. For each region it emits `BSP_GIC_<NAME>_BASE` and `_SIZE`, and for the UART `BSP_UART_BASE` and `BSP_UART_SIZE`, each as a `ULL` hex literal. `GIC_BASE` is not generated, because on a GIC 400 it is the enclosing block base, which the tree does not state.
6. **Write `regs.inc`.** It begins with a comment listing the nodes it matched, wraps the defines in a guard named `__BSP_<BOARD>_REGS_INC__`, and fails with an error if it found nothing.
7. **Put it on the include path.** The board loop in `CMakeLists.txt` adds `bsp/<board>` and `build/<mode>/<board>/generated` to the include directories, so `#include "regs.inc"` finds that board's file. There is no board macro. See [no board macro](../decisions/no-board-macro.md).
8. **Use it early.** `Uart::Uart()` starts at `BSP_UART_BASE`, so the console works before the tree is read. The `Gic` constructor starts at the four `BSP_GIC_*_BASE` values.
9. **Check it at boot.** `hmain` calls `hostTree.GetHostMmio()` after the PMM is up. That finds the UART and GIC nodes in the firmware tree and panics if the UART is missing or the GIC has fewer than four regions.
10. **Compare.** It compares ten values from the tree, the UART base and size and the base and size of each of the four GIC regions, with the matching `BSP_` constants. The code calls `Uart::SetBase` with the tree's UART base before it looks at the result. If any value differs it calls `HvPanic` with "BSP constants do not match the firmware device tree".
11. **Rebind and map.** On a match it calls `Gic::SetBases` with the tree's four bases, then adds the UART block and every GIC region to an `MmioMap`. `HostMmu::Enable` takes that map, so the host stage 1 map covers exactly what the host tree declares.
12. **The guest side.** `GetGuestMmio` reads the guest tree instead. It maps the first two GIC regions and the UART one to one into the guest. It logs a warning, and does not panic, when the guest tree has no GIC or no PL011.

# Things to know

- A board is only as correct as its committed host tree. If `host-qemu.dtb` no longer matches what QEMU supplies, step 10 stops the boot.
- The `BSP_` constants are checked against the firmware tree, not trusted. Steps 8 and 10 show where they are used and where they are verified.
- Changing a host tree changes `regs.inc` on the next configure, not on the next build, since step 2 runs at configure time.
