---
type: Flow
title: Boot to main
description: What happens from reset at the QEMU entry stub to the end of hmain, with the host tree parsed, the PMM and host MMU up, and the guest running.
resource: ../../bsp/qemu/boot.S
tags: [boot, el2, hmain]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../bsp/qemu/boot.S
  - resource: ../../core/main.cpp
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../core/vmm/exceptions/vectors.S
  - resource: ../../core/mm/pmm/pmm.cpp
---

# Summary

Firmware enters `_start` in `bsp/qemu/boot.S` at EL2 with the device tree address in `x0`. The stub parks every core but core 0, sets up EL2 and the stack, installs the exception vectors, and calls `hmain(dtb)`. `hmain` in `core/main.cpp` parses the tree, brings up the page allocator and the host MMU, then either runs the integration suite or loads and runs the Linux guest. It never returns. Only the QEMU stub is described, because the Pi 5 board has its own `boot.S` under `bsp/`. See [bsp](../subsystems/bsp.md).

# Steps

1. `bsp/qemu/boot.S`, `_start`: `mov x19, x0` keeps the device tree pointer in a callee saved register. This happens before the core check, so parked cores also hold it.
2. `_start`: reads `MPIDR_EL1`, masks the low byte and branches to `.Lblock` for any core other than 0. `.Lblock` is a `wfe` loop.
3. `_start`: reads `CurrentEL`. If it is not EL2 the core also goes to `.Lblock`.
4. `_start`: writes `HCR_EL2` with only bit 31 (RW) set, so guests are AArch64. This replaces the whole register.
5. `_start`: clears the MMU, alignment check, data cache and instruction cache bits in `SCTLR_EL2` and issues an `isb`.
6. `_start`: sets `EL1PCTEN` and `EL1PCEN` in `CNTHCTL_EL2`, zeroes `CNTVOFF_EL2`, and copies `MIDR_EL1` and `MPIDR_EL1` into `VPIDR_EL2` and `VMPIDR_EL2`.
7. `_start`: zeroes `.bss` from `__bss_start` to `__bss_end` with 16 byte stores.
8. `_start`: selects `SP_EL2` with `SPSel`, then sets `sp` to `__stack_end`.
9. `_start`: loads the address of `el2_vectors` (`core/vmm/exceptions/vectors.S`) into `VBAR_EL2`, so a fault inside `hmain` reaches a panic handler.
10. `_start`: moves `x19` back to `x0` and does `bl hmain`.
11. `core/main.cpp`, `hmain`: `RunGlobalConstructors()` from `lib/cxxrt`.
12. `hmain`: `TreeParser hostTree { dtb }` then `ParseMemoryMap()`. See [device tree](../subsystems/device-tree.md).
13. `hmain`: `Pmm::GetInstance().SetMemoryMap(memoryMap)` builds the buddy allocator over the RAM range.
14. `hmain`: `HostMmu::GetInstance().Enable(hostTree.GetHostMmio())`. `GetHostMmio` sets the UART and GIC base addresses, checks them against `regs.inc` and returns the host device windows. See [host MMU bring up](host-mmu-bring-up.md).
15. `hmain`: with `INTEGRATION_TEST` defined, calls `TestRunner::SetBootContext` and `TestRunner::RunAll` and stops there. See [integration run](integration-run.md).
16. `hmain`: otherwise builds a `cpio::Archive` over the firmware archive, then loads and runs the guest. See [load and enter guest](load-and-enter-guest.md).
17. `hmain`: after `Vmm::Run` returns it logs the result and spins in `wfe`.

# Gotchas

- `HCR_EL2` is overwritten, not combined with its old value. Anything set earlier by firmware in that register is lost, and later code, such as `GuestMmu::Enable`, adds bits one at a time.
- A guest or host exception before step 9 has no vector table. After step 9 every exception from EL2 panics.
- Logging before `GetHostMmio` runs relies on the early console constants from `regs.inc`, since the UART base is only set from the tree in step 14.
- `hmain` is `extern "C"` and has no return address. The stub follows `bl hmain` with `.Lblock`, but returning is not supported.
- The archive pointer comes from the host tree, as `linux,initrd-start` and `linux,initrd-end` in `/chosen`. Without them `cpioArchiveBase` is zero and the load in step 16 fails.
