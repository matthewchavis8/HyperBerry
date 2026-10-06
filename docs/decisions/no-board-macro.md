---
type: Decision
title: There is no board macro
description: Why no code tests which board it is built for, and how the include path picks the board instead.
resource: ../../CMakeLists.txt
tags: [decision, build, bsp]
status: unverified
decided: 2026-09-10
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../CMakeLists.txt
  - resource: ../../core/deviceTree/deviceTree.cpp
---

# The decision

There is no `BSP_QEMU` or `BSP_RPI5`. The build puts `bsp/<board>` and that board's generated directory on the include path, so code includes `"regs.inc"` and the board selects itself. Never reintroduce an `#ifdef` on the board.

# Why

- Before the change, board files were spread over several folders and the code chose a board's addresses with compile macros. A commit on 2026-04-29 first gathered them into a `bsp` folder, with the reason that one source of truth in `bsp` beats selecting a base address in many places.
- On 2026-09-10 commit 2999987 removed the macros. Each board folder then held everything the board owns, and the include path did the selecting.
- The same commit made one configure build every board. Before, every board wrote the same output file, so building one board overwrote the image of the other.
- A suite that needs to differ by board does it through files and not guards. `tests/bsp/<board>/` was added the same day for that (commit ce050d0).

# Consequences

- A library is built once per board, so target names carry a `-<board>` suffix. See [layering](../architecture/layering.md).
- Adding a board is a new folder and an entry in `SUPPORTED_BOARDS`, not a new macro branch in shared code.
- Anything that differs by address is passed in from outside. The GIC test payload receives its GICV base in `x0` rather than choosing one with an `ifdef`.
