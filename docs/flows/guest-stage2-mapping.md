---
type: Flow
title: Guest stage 2 mapping
description: How a guest's RAM and device windows become stage 2 tables, and how those tables are switched on for the guest.
resource: ../../core/mm/mmu/guestMmu/
tags: [mmu, stage2, guest]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/main.cpp
  - resource: ../../core/vm/vm.h
  - resource: ../../core/vm/vm.cpp
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/mm/pageTable/pageTable.cpp
  - resource: ../../core/mm/mmu/mmioMap.h
  - resource: ../../core/mm/mmu/guestMmu/guestMmu.h
  - resource: ../../core/mm/mmu/guestMmu/guestMmu.cpp
  - resource: ../../core/deviceTree/deviceTree.cpp
---

# Summary

After the boot loader has copied the guest into fresh RAM, `hmain` reads the guest's own device tree to learn which
device windows the guest may touch, and constructs a `Vmm`, which constructs a `Vm`, which constructs a `GuestMmu`.
The constructor builds the tables: a 40 bit IPA space on two concatenated L1 root tables, the guest RAM as 2 MiB blocks, then
the device windows as 4 KiB pages. `Vm::Start` later calls `GuestMmu::Enable`, which installs the root and the VMID
and turns stage 2 on. See [mm](../subsystems/mm.md), [vm](../subsystems/vm.md) and [vmm](../subsystems/vmm.md).

# Steps

1. `core/main.cpp`, `hmain`: after `BootLoader::Load` fills a `GuestLayout`, builds `TreeParser guestTree` on the guest tree in guest RAM and calls `TreeParser::GetGuestMmio`.
2. `TreeParser::GetGuestMmio`: adds identity page windows with `MmioMap::AddPages` for the first two regions of the guest GIC node and the first region of the guest PL011 node, then, if the IPA `0x107D001000` is not already covered, adds one 4 KiB page there backed by `BSP_UART_BASE`. A missing GIC or UART only logs a warning.
3. `hmain` builds a `VmConfig` with `GUEST_IPA_BASE` (0x40000000), the host RAM base from the layout, `GUEST_RAM_SIZE` (256 MiB) and VMID 1, then constructs `Vmm vmm { config, guestMmio }`.
4. `Vmm::Vmm` constructs `Vm`. In the `Vm::Vm` initializer list, `m_guestMmu` is constructed from `config.ipaBase`, `config.ramHostPa`, `config.ramSize` and the device map.
5. `GuestMmu::GuestMmu` calls `allocStage2RootTable`: two contiguous PMM pages (order 1), zeroed and cleaned from the data cache. They become the root, wrapped in a `PageTable` with start level 1 and index mask 0x3FF.
6. The constructor writes VTCR_EL2 for T0SZ 24 (40 bit IPA), start level 1, 4 KiB granule, inner shareable, write back, 40 bit physical size, then `isb`.
7. It loops over the RAM in 2 MiB steps calling `GuestMmu::MapBlock(ipa, pa, false)`. Each call runs `PageTable::Walk(ipa, true)`, which allocates the L2 table on first use, and writes a block descriptor with normal write back memory, inner shareable, read write, then cleans the entry from the data cache.
8. For each `MmioWindow`, the constructor calls `MapPage` per 4 KiB (or `MapBlock` for a window that is not by page). `MapPage` calls `walkL3`, which splits an existing 2 MiB block into 512 page descriptors when needed (zero the L2 entry, clean it, `dsb ish`, and `tlbi vmalls12e1is` if HCR_EL2.VM is already set), or allocates a fresh L3 table, then writes a Device nGnRnE, execute never page descriptor.
9. A `dsb ishst` ends the constructor. No translation is active yet.
10. `Vmm::Run` checks the state is `READY` and calls `Vm::Start`, which calls `GuestMmu::Enable(vmid)`.
11. `GuestMmu::Enable`: forms VTTBR_EL2 as `vmid << 48 | root`, runs `dsb ish`, writes VTTBR_EL2 and `isb`, sets HCR_EL2.VM and `isb`, then calls `TlbFlushAllGuest` (`tlbi vmalls12e1is`, `dsb ish`, `isb`). The invalidate comes after VM is set on purpose, so stage 2 walk caches are flushed.
12. `Vm::Enter` calls `Vcpu::Run`, and the guest starts with stage 2 on. Any IPA no window or RAM block covers faults to EL2, where `Vmm::Run` logs it and marks the guest faulted.
