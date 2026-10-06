---
type: Subsystem
title: Mm
description: The memory layer, holding the buddy page allocator, the heap behind new and delete, the shared page table walk, the EL2 stage 1 map and the per guest stage 2 map.
resource: ../../core/mm/
tags: [mm, mmu, memory]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/mm/CMakeLists.txt
  - resource: ../../core/mm/pmm/pmm.h
  - resource: ../../core/mm/pmm/pmm.cpp
  - resource: ../../core/mm/heap/heap.h
  - resource: ../../core/mm/heap/heap.cpp
  - resource: ../../core/mm/pageTable/pageTable.h
  - resource: ../../core/mm/pageTable/pageTable.cpp
  - resource: ../../core/mm/mmu/mmioMap.h
  - resource: ../../core/mm/mmu/hostMmu/hostMmu.h
  - resource: ../../core/mm/mmu/hostMmu/hostMmu.cpp
  - resource: ../../core/mm/mmu/guestMmu/guestMmu.h
  - resource: ../../core/mm/mmu/guestMmu/guestMmu.cpp
  - resource: ../../core/main.cpp
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../core/deviceTree/deviceTree.h
  - resource: ../../core/vm/vm.cpp
  - resource: ../../core/vm/vm.h
---

# What it is

Everything that owns or maps memory. `Pmm` hands out physical pages from a buddy allocator. `Heap` sits on top of it
and serves the global `new` and `delete`. `PageTable` is the shared translation table walk, used by both
MMUs. `HostMmu` builds the hypervisor's own EL2 stage 1 map and turns the MMU and caches on. `GuestMmu` builds one
stage 2 table set per guest. `MmioMap` is the list of device windows the caller hands to either MMU. The CMake target is
`Mm${BSP_SUFFIX}`, a static library of `pmm.cpp`, `heap.cpp`, `pageTable.cpp`, `hostMmu.cpp` and `guestMmu.cpp` that links
`Drivers${BSP_SUFFIX}` and `Lib${BSP_SUFFIX}` as PUBLIC. Everything runs with an identity map, so a physical address is also its EL2 virtual address.

# Depends on

- [Lib](lib.md): `Log`, `memset`, and `HvPanic` (the heap declares it by hand).
- [Drivers](drivers.md): linked by the CMake target. The source in `core/mm` does not call a driver directly.
- [Device tree](device-tree.md), as a header only: `pmm.h` includes `core/deviceTree/deviceTree.h` for the `MemoryMap` struct.
- Linker symbols `__text_start` and `__uncached_space_end` from the board linker script, read by `Pmm::SetMemoryMap`. See [boards](bsp.md).

# Used by

- `Pmm`: `core/main.cpp` (`SetMemoryMap`), `core/bootLoader/bootLoader.cpp` (guest RAM), `core/mm/heap/heap.cpp`,
  `core/mm/pageTable/pageTable.cpp`, `core/mm/mmu/guestMmu/guestMmu.cpp`, and the integration and unit tests.
- `Heap`: `core/mm/heap/heap.cpp` defines the global operators; `tests/unit/heap/test_heap.cpp` includes the header.
- `PageTable`: both MMUs and `core/bootLoader/bootLoader.cpp`.
- `HostMmu`: `core/main.cpp` calls `Enable`; `core/bootLoader/bootLoader.cpp` uses `HostMmu::PaToVa`.
- `GuestMmu`: `core/vm/vm.h` holds one as a member of `Vm`; the constructor runs in the `Vm` initializer list and `Enable` is called from `Vm::Start`.
- `MmioMap`: `core/deviceTree/deviceTree.h` returns it from `GetHostMmio` and `GetGuestMmio`, and both MMU headers take it.
- Flows: [host MMU bring up](../flows/host-mmu-bring-up.md) and [guest stage 2 mapping](../flows/guest-stage2-mapping.md).

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `core/mm/CMakeLists.txt` | Builds `Mm${BSP_SUFFIX}` and exports six include paths | (none) | `core/CMakeLists.txt` |
| `core/mm/pmm/pmm.h`, `core/mm/pmm/pmm.cpp` | Buddy allocator over the RAM the device tree describes | `Pmm`, `PAGE_SIZE`, `PAGE_SHIFT`, `MAX_ORDER`, `SetMemoryMap`, `AllocPages`, `FreePages`, `DumpState` | `core/main.cpp`, `core/bootLoader/bootLoader.cpp`, `PageTable`, `GuestMmu`, `Heap` |
| `core/mm/heap/heap.h`, `core/mm/heap/heap.cpp` | Slab allocator with a whole page fallback; the global `operator new` and `operator delete` family | `Heap`, `Allocate`, `Deallocate`, `operator new`, `operator delete` | every `new` and `delete` in the image |
| `core/mm/pageTable/pageTable.h`, `core/mm/pageTable/pageTable.cpp` | Descriptor bit macros, index macros, and a walk that stops at the L2 entry | `PageTable`, `Walk`, `AllocTable`, `CleanDataCacheRange`, `PTE_*`, `SIZE_*`, `L0_INDEX` to `L3_INDEX`, `pte_is_table` | `HostMmu`, `GuestMmu`, `core/bootLoader/bootLoader.cpp` |
| `core/mm/mmu/mmioMap.h` | A fixed list of up to 8 device windows, filled by the caller | `MmioWindow`, `MmioMap`, `AddBlocks`, `AddPages`, `Add`, `Covers`, `MMIO_MAX_WINDOWS` | `core/deviceTree/deviceTree.cpp`, `HostMmu::Enable`, `GuestMmu` |
| `core/mm/mmu/hostMmu/hostMmu.h`, `core/mm/mmu/hostMmu/hostMmu.cpp` | EL2 stage 1 identity map, MAIR and TCR setup, MMU and cache enable | `HostMmu`, `GetInstance`, `Enable`, `MapRange`, `UnmapRange`, `TlbFlushAll`, `TlbFlushVa`, `PaToVa`, `PTE_NORMAL`, `PTE_DEVICE` | `core/main.cpp`, `core/bootLoader/bootLoader.cpp` |
| `core/mm/mmu/guestMmu/guestMmu.h`, `core/mm/mmu/guestMmu/guestMmu.cpp` | One guest's stage 2 table set, VTCR_EL2 and VTTBR_EL2 | `GuestMmu`, `MapBlock`, `MapPage`, `Enable`, `TlbFlushAllGuest`, `S2PTE_*`, `VTCR_*` | `core/vm/vm.cpp` (through `Vm`) |

# Entry points

## pmm

`Pmm::GetInstance().SetMemoryMap(map)` once at boot, then `AllocPages(order)` and `FreePages(addr, order)`.
A block is `PAGE_SIZE << order` bytes (4 KiB pages, orders 0 to 16, so the largest block is 256 MiB).
`SetMemoryMap` clears every list and the bitmap, seeds the free lists with the whole pool, then reserves the
kernel image (`__text_start` to `__uncached_space_end`), the TF-A region, the device tree blob, the guest archive when
its size is nonzero, and page zero when the pool starts at address 0.

## heap

No direct API. `new` and `delete` call `Heap::GetInstance().Allocate(size, align)` and `Deallocate(ptr)`.
Sizes up to 1024 bytes come from slab classes of 16, 32, 64, 128, 256, 512 and 1024 bytes. Anything bigger, or
aligned more strictly than 1024, takes whole pages from the PMM with a header placed just before the returned pointer.

## pageTable

`PageTable table { root, startLevel, rootIndexMask }`, then `table.Walk(addr, allocOnMiss)` to get a pointer to the
L2 entry. Stage 1 uses start level 0 and mask 0x1FF. Stage 2 uses start level 1 and mask 0x3FF, because the 40 bit IPA
space uses two concatenated L1 tables.

## mmioMap

`MmioMap::AddBlocks(base, size)` for identity windows widened to 2 MiB blocks (host side), and
`AddPages(ipa, pa, size)` for 4 KiB page windows (guest side). The parser in `core/deviceTree` fills it.

## hostMmu

`HostMmu::GetInstance().Enable(devices)`. `PaToVa(pa)` returns the address cast to a pointer.

## guestMmu

`GuestMmu guestMmu { ipaBase, hostPaBase, sizeBytes, devices }` builds the tables. `Enable(vmid)` commits them.

# Gotchas

- **`SetMemoryMap` must run before anything allocates, and `HostMmu::GetInstance()` allocates.** Its constructor takes the L0
  table from `Pmm`, so the first call has to come after `SetMemoryMap`. `PageTable::AllocTable` and the stage 2 root
  allocation do the same. `AllocPages` returns 0 when it fails, but `AllocTable` and the stage 2 root loop forever in `wfe`
  on failure, and `operator new` calls `HvPanic`.
- **The PMM writes into the free memory itself.** Each free block starts with a `FreeNode` next pointer, so the pages
  must be reachable at their physical addresses. It does not zero pages it hands out; `AllocTable` zeroes its own page.
- **`FreePages` trusts the caller.** A wrong order or a double free corrupts the per order bitmap, which is one bit per buddy
  pair toggled on each allocation and free. `FreePages(0, ...)` is ignored, so an allocation that failed can be freed without a check.
- **The pool is capped at 8 GiB.** `MAX_POOL_SIZE` is `0x200000000`; a larger pool halts with a log line. The bitmap is a static array
  sized for that cap, and a bitmap index past its end also halts silently in a `wfe` loop.
- **Pool seeding, reservation and `DumpState` all print through `Log`,** so they disappear in release.
- **Guest RAM needs one contiguous block.** `core/bootLoader/bootLoader.cpp` asks for order `GUEST_RAM_ORDER`,
  which for 256 MiB is exactly `MAX_ORDER` (16), the biggest block there is. A fragmented pool fails this allocation. A TODO in `pmm.h` notes it.
- **Heap slabs are never given back.** `freeToSlab` pushes the slot on the slab's list and drops `inUse`, but an empty slab page stays with
  its size class. Large blocks are returned to the PMM.
- **`Deallocate` tells the two kinds apart by guessing.** A page aligned pointer must be a large block (slab slots start after the header, so they are never
  page aligned). Otherwise it reads the magic at the start of the page, then the header before the pointer, and panics with
  `HvPanic` if neither matches. Freeing a pointer the heap did not give out reads memory it does not own.
- **`new` failure is a panic, not an exception,** and the heap is not interrupt safe. The global operators are compiled out when
  `HEAP_TESTING_BUILD` is defined, which is how the unit tests link their own.
- **`PageTable::Walk` stops at L2, and trusts the valid bit.** It descends through levels below 2 and treats any valid
  entry as a table pointer without checking the table bit, so a valid 1 GiB block descriptor at L0 or L1 would be walked as a table.
  Nothing in this tree creates one. A 4 KiB mapping needs the L3 step that only `GuestMmu` has (`walkL3` in `guestMmu.cpp`).
- **The page table code is written for hardware that does not snoop its stores.** `AllocTable`, `MapBlock` and `MapPage` call
  `CleanDataCacheRange` (`dc cvac` and a `dsb ishst`) after writing entries. QEMU hides a missing clean, real hardware does not.
- **`HostMmu::MapRange` and `UnmapRange` do none of that.** `MapRange` writes the L2 entries with no cache clean and no TLB flush,
  which is correct inside `Enable` because the MMU and caches are still off. Called after `Enable` it would need both.
  `UnmapRange` flushes the TLB per address but does not clean the entry. Both walk in 2 MiB steps, so addresses and sizes must be
  multiples of 2 MiB; nothing checks.
- **The host map is permissive by default.** `Enable` first maps the whole range from `HV_VA_BASE` (0) of `HV_VA_SIZE` (16 GiB) as cacheable normal
  memory, executable, then overwrites the device windows as Device with execute never. A peripheral missing from `devices` stays mapped cacheable
  rather than faulting, as the header says.
- **A full `MmioMap` is silent.** The comment says a full map "drops further windows and says so", but `Add` only returns `false` and logs nothing.
  The callers in `core/deviceTree/deviceTree.cpp` check the result and panic. The limit is `MMIO_MAX_WINDOWS` (8).
- **`MmioWindow` and `MmioMap` are `alignas(16)` on purpose.** The host map is built while the MMU is off, where all memory is Device and every
  access must be naturally aligned, and the compiler copies a struct this size with 128 bit NEON pairs. `MemoryMap` carries the same attribute.
- **Mm includes a Core header and Core includes an Mm header.** `pmm.h` pulls in `core/deviceTree/deviceTree.h` for `MemoryMap`, while
  `deviceTree.h` pulls in `mmioMap.h`. `AGENTS.md` says Mm may not depend on Core, and this works only because both are headers.
- **`GuestMmu` programs `VTCR_EL2` in its constructor,** not in `Enable`, so building a guest changes global CPU state before it runs. `Enable(vmid)`
  does not check that `vmid` is nonzero or unique. It is called by `Vm::Start`, not by the constructor.
- **`GuestMmu`'s destructor frees only the root.** The deleter returns the order 1 root (two 4 KiB pages) to the PMM. The L2 and L3
  tables that `Walk` and `walkL3` allocated are not freed.
- **Device windows are applied after RAM and win.** `MapPage` over an IPA already covered by a 2 MiB RAM block splits that block
  into 512 pages first (`walkL3`), with a break before make: it zeroes the L2 entry, cleans it, and issues `tlbi vmalls12e1is` only when HCR_EL2.VM is already set.
- **`guestMmu.cpp` maps with `pa` equal to `ipa` for device windows.** What a guest sees at an IPA is decided entirely by the `MmioMap` the caller built.
  `GetGuestMmio` identity maps the first two GIC regions of the guest tree, so nothing in `core/mm` redirects the guest CPU interface to the GICV frame. Check this against the guest tree before relying on it.
- **Size and alignment rules are unchecked.** `GuestMmu` expects a 2 MiB aligned and sized RAM range. The stage 2 root needs 8 KiB alignment, which
  comes from asking the PMM for an order 1 block.
