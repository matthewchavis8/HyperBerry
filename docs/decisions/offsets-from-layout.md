---
type: Decision
title: Assembly offsets come from the C++ layout
description: Why vcpu.S gets its struct offsets from the compiler through a generated header and not from hand kept macros.
resource: ../../scripts/asmoffsets/
tags: [decision, build, vcpu]
status: unverified
decided: 2026-09-25
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vcpu/vcpuOffsets.cpp
  - resource: ../../core/CMakeLists.txt
  - resource: ../../scripts/asmoffsets/asmoffsets.py
---

# The decision

`vcpu.S` indexes into `Vcpu` with offsets the compiler computes. `core/vcpu/vcpuOffsets.cpp` emits each `offsetof` as an `.ascii "->NAME VALUE"` marker through inline assembly. It is compiled with `-S`, and `scripts/asmoffsets/asmoffsets.py` lifts the markers into a generated `vcpuOffsets.h` of `#define` lines that the `.S` files include.

# Why

- Commit 027a381 (2026-09-25) states it: the offsets can no longer drift from the layout.
- Before, `VCPU_*` offset macros were kept by hand, with `static_assert` lines that checked them against the struct, plus unit and integration tests that compared the macros with the layout. Those all went away.
- The same commit made `Vcpu`'s state private, with the `Gpr`, `El2Reg` and `El1Reg` enums selecting registers.
- The `static_assert` lines that remain in `vcpuOffsets.cpp` guard the layout facts the assembly relies on, such as standard layout and the `VcpuExit` field offsets.

# Consequences

- Every image depends on the `VcpuOffsetsHeader-<board>` target, and so does the integration test library.
- The offsets header is generated into `build/<mode>/<board>/generated/` and is not committed.
- Adding a field the assembly reads means adding an `OFFSET(...)` line in `vcpuOffsets.cpp`.

See [Vcpu](../subsystems/vcpu.md) and [build](../architecture/build.md).
