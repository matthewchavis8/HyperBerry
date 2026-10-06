---
type: Architecture
title: Layering
description: The five libraries, which may depend on which, and how the CMake link lines enforce the order.
resource: ../../
tags: [architecture, build]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../CMakeLists.txt
  - resource: ../../core/CMakeLists.txt
  - resource: ../../core/mm/CMakeLists.txt
  - resource: ../../drivers/CMakeLists.txt
  - resource: ../../lib/CMakeLists.txt
---

# The rule

The code is five static libraries, stacked. Each may depend only on those below it.

| Layer | Library target | Source folders |
|---|---|---|
| 1, top | `Core` | `core/main.cpp`, `core/deviceTree/`, `core/bootLoader/` |
| 2 | `Virt` | `core/vcpu/`, `core/vm/`, `core/vmm/` |
| 3 | `Mm` | `core/mm/` (pmm, heap, pageTable, mmu) |
| 4 | `Drivers` | `drivers/` (uart, gic, timer) |
| 5, bottom | `Lib` | `lib/` (cpio, cxxrt, log, panic, strings) |

`mmio` and `registerDump` under `lib/` are headers only. See [lib](../subsystems/lib.md), [drivers](../subsystems/drivers.md), [mm](../subsystems/mm.md), [vcpu](../subsystems/vcpu.md), [vm](../subsystems/vm.md) and [vmm](../subsystems/vmm.md). The two `Core` folders are [device tree](../subsystems/device-tree.md) and [boot loader](../subsystems/boot-loader.md).

# How the link states it

Each library says what it links in its own `CMakeLists.txt`.

- `Lib` links nothing.
- `Drivers` links `Lib` publicly.
- `Mm` links `Drivers` and `Lib` publicly.
- `Virt` links `Mm`, `Drivers` and `Lib` publicly.
- `Core` links `Virt`, `Mm`, `Drivers` and `Lib` privately, through `HB_CORE_DEPS`.

The list `HB_SUBSYSTEMS` in the top level `CMakeLists.txt` is `Virt Mm Drivers Lib`, in order, and the comment above it says each may depend only on those below it. The image link in `hb_add_image` puts `Core`, then those four, inside one `--start-group`, followed by the C and C++ runtime archives.

AGENTS.md says the linker enforces the order, so a violation shows up as a build failure. What the CMake files declare is the downward only list above. The final image link uses one `--start-group`, which lets archives resolve symbols from each other in any order, so I could not confirm from the files that an upward call fails to link. This page records what is declared and claims no more.

See [layering by link](../decisions/layering-by-link.md) for why the order is kept this way.

# The board suffix

Every library target is named with a `-<board>` suffix, held in `BSP_SUFFIX`. The libraries are `Lib-qemu`, `Lib-rpi5`, `Drivers-qemu` and so on, and the images are `hyperberry-qemu` and `hyperberry-rpi5`. CMake does not allow two targets with one name in one project, and one configure builds every board, so the board is part of the name. AGENTS.md also says a prefix like `Hb` is not worth adding to target names.

The top level file calls `add_subdirectory(lib lib-<board>)`, then `drivers`, then `core` for each board. `core/CMakeLists.txt` adds `mm/` itself.

# Where the rule bends

- The three assembly files `core/vmm/exceptions/vectors.S`, `core/vmm/exceptions/entry.S` and `core/vcpu/vcpu.S` belong to `Virt` by folder but are linked straight into each image through `HB_ENTRY_SOURCES`. Nothing references the vector table by symbol, so inside an archive the linker would drop them before `KEEP` in the linker script applied.
- `CoreTest-<board>` is a second variant of `Core` built with `INTEGRATION_TEST`, and `Tests-<board>` is linked in with `--whole-archive`. See [tests](tests.md).
- The unit tests compile chosen source files directly and do not use these libraries. See [tests](tests.md).
