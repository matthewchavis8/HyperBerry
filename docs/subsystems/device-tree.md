---
type: Subsystem
title: Device tree
description: The flattened device tree parser that finds RAM, the guest archive and the MMIO windows of the host and guest.
resource: ../../core/deviceTree/
tags: [core, device-tree, fdt]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/deviceTree/deviceTree.h
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../core/deviceTree/fdt.h
  - resource: ../../core/mm/mmu/mmioMap.h
  - resource: ../../core/main.cpp
  - resource: ../../core/CMakeLists.txt
---

# What it is

A read only parser for a flattened device tree blob, bound to a physical address through `TreeParser`. It answers four questions. `ParseMemoryMap` returns RAM, the TF-A reserved region, the DTB itself and the guest archive that firmware loaded. `FindDevice` finds the first node whose `compatible` matches a list and returns its `reg` entries translated through `ranges`. `GetHostMmio` builds the host MMU's device windows and checks them against the `regs.inc` constants. `GetGuestMmio` builds the stage 2 device windows for a guest tree. The addresses come from the tree at run time, which is why no board address is written by hand outside `regs.inc` (the rule is in [AGENTS.md](../../AGENTS.md)).

# Depends on

- [Mm](mm.md): `MmioMap` is declared in `core/mm/mmu/mmioMap.h`. Mm owns the shape and Core fills it in, because Mm may not depend on Core.
- [Drivers](drivers.md): `GetHostMmio` calls `Uart::GetInstance().SetBase` and `Gic::GetInstance().SetBases`.
- [Lib](lib.md): `HvPanic` from `lib/panic/panic.h` and `Log::Println` from `lib/log/log.h`.
- `regs.inc`, generated per board ([bsp](bsp.md), [scripts](scripts.md)): the `BSP_UART_*` and `BSP_GIC_*` constants.

# Used by

- `core/main.cpp` includes `deviceTree/deviceTree.h` and builds one `TreeParser` for the host tree and one for the guest tree.
- `core/mm/pmm/pmm.h` includes it for `MemoryMap`, so [Mm](mm.md) consumes the type (`Pmm::SetMemoryMap`).
- `core/bootLoader/bootLoader.cpp` includes `core/deviceTree/fdt.h` for `FdtHeader`, `FDT`, `Be32` and `Be64`.
- Tests: `tests/unit/deviceTree/test_deviceTree.cpp`, `tests/unit/bootLoader/test_bootLoader.cpp`, and `tests/integration/suite.h`.

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `core/deviceTree/deviceTree.h` | Public interface and the result types | `TreeParser`, `MemoryMap`, `DeviceNode`, `DeviceRegion`, `DT_MAX_REGIONS` | `core/main.cpp`, `core/mm/pmm/pmm.h` |
| `core/deviceTree/deviceTree.cpp` | Header validation, memory walk, compatible match, `ranges` translation, MMIO map building | `TreeParser::validateHeader`, `ParseMemoryMap`, `FindDevice`, `GetHostMmio`, `GetGuestMmio`; internal `translate`, `matchesCompatible`, `getCombinedCell` | `hmain` |
| `core/deviceTree/fdt.h` | On the wire format shared by the parser and the guest tree patcher | `FDT` token enum, `FdtHeader`, `FdtProp`, `Be32`, `Be64`, `FdtAlign` | `deviceTree.cpp`, `bootLoader.cpp` |

# Entry points

- `TreeParser::ParseMemoryMap`: takes `memBase` and `memSize` from the first `memory` node, the TF-A range from a child of `reserved-memory` named `atf`, `bl31`, `secmon`, `optee` or `tee`, and the guest archive from `linux,initrd-start` and `linux,initrd-end` in `/chosen`.
- `TreeParser::FindDevice`: the generic lookup. Pass a span of compatible strings.
- `TreeParser::GetHostMmio`: called once, before the host MMU is enabled.
- `TreeParser::GetGuestMmio`: called on the guest tree after the boot loader has patched it.

# Gotchas

- Every public call starts with `validateHeader`, which panics on a malformed tree. A bad tree is a boot failure, not an error code.
- `cpioArchiveBase` and `cpioArchiveSize` stay zero unless both initrd properties exist and the end is above the start. A missing archive is therefore silent here and shows up later in the boot loader.
- `GetHostMmio` compares the tree against the generated `regs.inc` constants (UART and four GIC regions) and panics on any mismatch with "BSP constants do not match the firmware device tree". It also sets the UART and GIC base addresses as a side effect, before that check can panic.
- The host GIC must expose at least four regions. The first four feed `Gic::SetBases` as distributor, CPU, hypervisor and virtual CPU frames.
- Host windows are widened to whole 2 MiB blocks (`MmioMap::AddBlocks`). Guest windows are 4 KiB pages (`AddPages`).
- `GetGuestMmio` maps only the first two GIC regions into the guest, so the guest never gets the hypervisor frame. A guest tree with no GIC or no PL011 logs a warning instead of panicking.
- `GetGuestMmio` always adds one extra console window at IPA `0x107D001000` backed by this board's UART, unless the map already covers it. The source marks this as a HACK with a TODO to replace it.
- `FindDevice` returns the first matching node only. A node with an unusable `reg` (empty, or a size that is not a multiple of the cell stride) is skipped and the search goes on. Regions are capped at `DT_MAX_REGIONS` (8).
- The parser reads the tree through `volatile` pointers and uses `alignas(16)` types. `MemoryMap` and `MmioMap` carry the alignment because they are built before the host MMU is on, when every access has to be naturally aligned.
- Nesting deeper than 16 levels panics in `validateHeader`. `FindDevice` and the `ranges` translation use the same limit.
- `ParseMemoryMap` takes the first `memory` node only. Later ones are ignored.
