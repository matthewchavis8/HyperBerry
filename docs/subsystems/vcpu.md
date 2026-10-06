---
type: Subsystem
title: Vcpu
description: One guest virtual CPU, its saved registers, and the assembly that enters the guest and returns on the next exception.
resource: ../../core/vcpu/
tags: [virt, vcpu, assembly]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vcpu/vcpu.h
  - resource: ../../core/vcpu/vcpu.cpp
  - resource: ../../core/vcpu/vcpu.S
  - resource: ../../core/vcpu/vcpuOffsets.cpp
  - resource: ../../scripts/asmoffsets/asmoffsets.py
  - resource: ../../core/CMakeLists.txt
  - resource: ../../CMakeLists.txt
  - resource: ../../core/vmm/exceptions/vectors.S
  - resource: ../../tests/integration/vcpu/test_vcpu.cpp
---

# What it is

`Vcpu` holds the saved state of one EL1 guest and runs it for one interval. `Run()` enters the guest with `eret` and returns a `VcpuExit` when the guest next takes an exception to EL2. Guest general registers, `pc`, `pstate`, `SP_EL0` and `SP_EL1` live in `GuestRegisters`. The EL1 system registers (SCTLR, TTBR0 and TTBR1, TCR, MAIR, VBAR and the rest) live in a private `SystemRegisters`. All of it is saved and restored in assembly by `vcpu_run` and the `vcpu_exit_*` labels in `vcpu.S`. There is no scheduler. The caller loops on `Run()` itself, which today is [Vmm](vmm.md) through [Vm](vm.md).

# Depends on

- Nothing from other subsystems. `vcpu.h` includes only `<array>` and `<cstdint>`.
- `scripts/asmoffsets/asmoffsets.py`, through the build, to produce the `vcpuOffsets.h` that `vcpu.S` includes ([scripts](scripts.md)).
- [Device tree](device-tree.md) and [Mm](mm.md) are not used here. The stage 2 tables the guest runs under belong to `GuestMmu`.

# Used by

- `core/vm/vm.h` includes `core/vcpu/vcpu.h` and owns a `Vcpu` ([Vm](vm.md)).
- `core/vmm/hvc/hvc.h` and `core/vmm/smc/smc.h` include it for `GuestRegisters` ([Vmm](vmm.md)).
- `core/vmm/vmm.cpp` reads `VcpuExit` and `ExitReason`.
- The EL2 vector table in `core/vmm/exceptions/vectors.S` branches to `vcpu_exit_sync`, `vcpu_exit_irq`, `vcpu_exit_fiq` and `vcpu_exit_serror`.
- Tests: `tests/unit/vcpu/` (with a stub in `tests/unit/vcpu/backend.h`), `tests/integration/vcpu/test_vcpu.cpp`, `tests/integration/gic/test_gic.cpp`.

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `core/vcpu/vcpu.h` | Saved state and the exit result | `GuestRegisters`, `ExitReason`, `VcpuExit`, `Vcpu`, `Vcpu::Run`, `Vcpu::GetRegisters` | `core/vm/vm.h`, `core/vmm/` |
| `core/vcpu/vcpu.cpp` | Seeds the guest and calls the assembly | `Vcpu::Vcpu`, `Vcpu::Run`, extern `vcpu_run`, extern `vcpu_read_sctlr` | `Vm::Enter` |
| `core/vcpu/vcpu.S` | Entry, guest context save and restore, abort address capture | `vcpu_run`, `vcpu_read_sctlr`, `vcpu_exit_sync`, `vcpu_exit_irq`, `vcpu_exit_fiq`, `vcpu_exit_serror`, `vcpu_exit` | `Vcpu::Run`, `core/vmm/exceptions/vectors.S` |
| `core/vcpu/vcpuOffsets.cpp` | Emits compiler chosen field offsets as assembler markers, with `static_assert`s on `VcpuExit` | `VcpuLayout::Emit`, `vcpu_offsets`, the `OFFSET` macro | the build only (`VcpuOffsets` object library) |
| `scripts/asmoffsets/asmoffsets.py` | Turns the markers in the compiled assembly into `#define`s | `main` | `core/CMakeLists.txt` custom command |

# Entry points

- `Vcpu(uint64_t entry)`: sets `pc` to `entry`, leaves `pstate` at `0x3C5` (EL1h, DAIF masked) and takes the current `SCTLR_EL1`.
- `Vcpu::Run()`: returns a `VcpuExit` holding `reason`, `syndrome`, `far` and `hpfar`.
- `Vcpu::GetRegisters()`: how the caller reads and writes `x`, `pc`, and the rest while the guest is stopped.

# Gotchas

- `Run()` returns a `VcpuExit` through a pointer the assembly writes, not through registers. `vcpu_run(Vcpu*, VcpuExit*)` keeps that pointer in its host frame on the EL2 stack and the exit path reads it back. This is the abort capture change in commit `4324037`.
- `far` and `hpfar` are filled only when the exception class in the syndrome is `0x20`, `0x21`, `0x24` or `0x25` (instruction or data abort, lower or same EL). For every other exit they are zero. IRQ and FIQ exits also report a zero syndrome.
- `hpfar` is the raw `HPFAR_EL2` value, not a shifted address. `tests/integration/vcpu/test_vcpu.cpp` only checks it is nonzero.
- `tpidr_el2` holds the `Vcpu` pointer for the whole guest interval. The exit path finds the register file through it, so a `Vcpu` must not move. The copy and move operations are deleted for that reason, and the host's own `tpidr_el2` is saved and restored around the run.
- `vcpu_run` masks all of DAIF on entry and restores the caller's DAIF on return. It also saves and restores the host `SP_EL0`, `ELR_EL2`, `SPSR_EL2` and callee saved registers `x19` to `x30`, so it behaves as an ordinary C function call.
- `SCTLR_EL1` is restored last, after an `isb`, so the guest's MMU setting takes effect only when every other register is in place. The constructor clears bits 0, 1, 2, 3 and 12 (MMU, alignment, data cache, stack alignment, instruction cache) from the current value.
- Any field added to `Vcpu`, `GuestRegisters` or `SystemRegisters` that assembly must reach needs a matching `OFFSET(...)` line in `vcpuOffsets.cpp`. `vcpuOffsets.h` is generated into the board's generated directory and is never edited. `vcpuOffsets.cpp` is compiled with `-S` so the markers survive as text, and the script fails if it finds none.
- `HOST_FRAME_SIZE` and the other host frame offsets in `vcpu.S` are plain `.equ` values, not generated. Changing the host frame means editing them by hand.
- `vcpu.S` is not part of the `Virt` library. It is linked into the image through `HB_ENTRY_SOURCES` in the top level `CMakeLists.txt`, beside `vectors.S` and `entry.S`. The CMake comment explains that archive members only referenced from the vector table would be dropped before the linker script's `KEEP` applied.
- The class has `friend struct VcpuLayout` so the offsets file can take `offsetof` on private members. Both `Vcpu` and `GuestRegisters` must stay standard layout, which `vcpuOffsets.cpp` asserts.
