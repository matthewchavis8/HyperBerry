---
type: Flow
title: Host MMU bring up
description: How hmain takes the hypervisor from running with the MMU off to running on its own EL2 stage 1 identity map with caches on.
resource: ../../core/mm/mmu/hostMmu/
tags: [mmu, boot, stage1]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/main.cpp
  - resource: ../../core/mm/pmm/pmm.cpp
  - resource: ../../core/mm/pageTable/pageTable.cpp
  - resource: ../../core/mm/mmu/hostMmu/hostMmu.h
  - resource: ../../core/mm/mmu/hostMmu/hostMmu.cpp
  - resource: ../../core/mm/mmu/mmioMap.h
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../bsp/qemu/boot.S
---

# Summary

`bsp/<board>/boot.S` leaves SCTLR_EL2 with the MMU and caches off. `hmain` then parses the host tree, gives the
page allocator its memory, builds a map of the device windows, and calls `HostMmu::Enable`, which fills a four level
translation regime with one 16 GiB identity range of normal memory plus the device windows, programs MAIR, TCR and TTBR0,
and sets M, C and I in SCTLR_EL2. After it, physical and virtual addresses are still the same number. See [mm](../subsystems/mm.md).

# Steps

1. `bsp/qemu/boot.S`: clears SCTLR_EL2 bits for the MMU and both caches, sets the stack and VBAR_EL2, then branches to `hmain` with the tree pointer. Until step 11 every access behaves as Device, so all data has to be naturally aligned.
2. `core/main.cpp`, `hmain`: calls `RunGlobalConstructors`, then builds `TreeParser hostTree` and calls `TreeParser::ParseMemoryMap`.
3. `Pmm::SetMemoryMap`: seeds the buddy free lists with the RAM range and reserves the kernel, TF-A, the tree, the guest archive and page zero.
4. `HostMmu::GetInstance`: the first call runs `HostMmu::HostMmu`, which calls `PageTable::AllocTable` for the L0 table (one PMM page, zeroed and cleaned from the data cache) and wraps it in a `PageTable` with start level 0 and index mask 0x1FF.
5. `TreeParser::GetHostMmio` (`core/deviceTree/deviceTree.cpp`): finds the UART and GIC, panics if either is absent, compares the tree's addresses with the `BSP_*` constants and panics on a mismatch, calls `Uart::SetBase` and `Gic::SetBases`, and returns an `MmioMap` made with `MmioMap::AddBlocks`, each window widened to whole 2 MiB blocks.
6. `HostMmu::Enable`: writes MAIR_EL2 with normal write back (index 0), Device (index 1) and normal non cacheable (index 2).
7. `HostMmu::Enable`: writes TCR_EL2 for a 48 bit space (T0SZ 16), 4 KiB granule, inner shareable, write back, then an `isb`.
8. `HostMmu::MapRange(HV_VA_BASE, HV_VA_BASE, HV_VA_SIZE, PTE_NORMAL | PTE_AP_RW)`: for each 2 MiB step, `PageTable::Walk(va, true)` allocates the missing L1 and L2 tables from the PMM and returns the L2 entry, which is written as a block descriptor. This covers 0 to 16 GiB.
9. For every window in the map, `MapRange(window.base, window.pa, window.size, PTE_DEVICE)` overwrites those blocks with Device memory that is execute never.
10. `Enable` writes the root to TTBR0_EL2, then `dsb ishst` and `isb`.
11. `Enable` reads SCTLR_EL2, sets bit 0 (MMU), bit 2 (data cache) and bit 12 (instruction cache), writes it back and runs `isb`. Translation is now on.
12. `hmain` continues: in a normal build it loads the guest archive, and in an integration build it hands control to `TestRunner`. See [guest stage 2 mapping](guest-stage2-mapping.md) for the next map.

Gotchas: step 5 also fixes where the console lives, so `Log` lines after step 11 reach a UART that is mapped as Device only because step 9 maps it. `hostMmu.cpp` does not flush the TLB, which is safe only because the MMU was off.
