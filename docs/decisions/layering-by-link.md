---
type: Decision
title: Layering is enforced by the link
description: Why the code is five libraries in a fixed order, and why CMake link lines state the order.
resource: ../../CMakeLists.txt
tags: [decision, build, layering]
status: unverified
decided: 2026-09-10
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

# The decision

The code is five static libraries, `Core`, `Virt`, `Mm`, `Drivers` and `Lib`, and each may depend only on those below it. Each library's `target_link_libraries` lists only layers below it. Target names carry a `-<board>` suffix, because one CMake project cannot hold three targets of the same name.

# Why

- Commit 2999987 (2026-09-10) turned 17 module libraries into the five layers and kept every dependency that already crossed a layer.
- AGENTS.md says a violation should show up as a build failure and not as a review comment.
- The suffix is needed since one configure builds every board, so each board gets its own copy of each library.

# Consequences

- A change in `Lib` can reach every layer above it, and a change in `Core` reaches no other library.
- The final image link groups all five libraries with `--start-group`, so the order itself comes from the declared dependencies. See [layering](../architecture/layering.md) for what I could and could not confirm.
- The entry assembly files are linked into the image and not into `Virt`, so the linker script's `KEEP` applies to them.
