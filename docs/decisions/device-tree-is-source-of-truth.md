---
type: Decision
title: The device tree is the source of truth for addresses
description: Why no address that a device tree declares is written by hand, and the two paths that carry tree values into the code.
resource: ../../scripts/bspgen/
tags: [decision, device-tree]
status: unverified
decided: 2026-09-12
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../scripts/bspgen/bspgen.py
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../CMakeLists.txt
---

# The decision

Do not hand write a value a device tree already declares. Two paths carry tree values into the code. `scripts/bspgen` reads a board's host tree at configure time and writes `regs.inc` for the few addresses that must be compile time constants, which are the early console and the GIC bases the register tables are built from. Everything else is read from a tree at runtime through `core/deviceTree`, including the MMIO windows both MMU layers map. Boot checks the generated constants against the firmware tree and panics on a mismatch.

# Why

- Addresses had been kept by hand in three headers, and a C++ header cannot be included from assembly, so `boot.S` repeated its own literals. One value, `0x88000000`, was written out three times (commit c2a9299, 2026-09-12).
- Nothing checked that the addresses compiled into an image matched the hardware it booted on. A wrong value would fault on the first access somewhere unrelated, so the check panics with a message instead (commit ad77c45, same day).
- Window sizes had also been written by hand with nothing cross checking them, and the guest windows were stated a second time in each guest tree. Deriving the windows from the trees at runtime removed both copies (commit d4ddde1).
- The early console must work before the tree is parsed, and the GIC register tables are constexpr. That is why a small set of values still has to be a compile time constant.

# Consequences

- Each board commits one host tree: the firmware blob for `rpi5`, a capture for `qemu`. A stale capture stops the boot at the comparison.
- The per board `bsp.h` headers were deleted (d4ddde1) and code includes `regs.inc` directly.
- `GIC_BASE` is still not generated, because on a GIC 400 it is the enclosing block base and no tree states it.
- A guest only gets the GIC regions its own tree declares. The guest trees declare the distributor and CPU interface and leave out the hypervisor frames.

See [device tree to regs.inc](../flows/device-tree-to-regs.md).
