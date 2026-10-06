---
type: Flow
title: Guest abort
description: How a guest instruction or data abort is captured with FAR_EL2 and HPFAR_EL2, reported in VcpuExit, and turned into a faulted VM.
resource: ../../core/vcpu/
tags: [abort, stage2, exceptions]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vcpu/vcpu.S
  - resource: ../../core/vcpu/vcpu.h
  - resource: ../../core/vcpu/vcpu.cpp
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/vmm/esr/esr.h
  - resource: ../../core/main.cpp
  - resource: ../../tests/integration/vcpu/test_vcpu.cpp
  - resource: ../../tests/integration/abort/guest_payload.S
---

# Summary

When a guest touches memory stage 2 does not map, the CPU takes a data abort to EL2. The Vcpu exit path reads `FAR_EL2` and `HPFAR_EL2` while they are still valid and returns them in `VcpuExit`. `Vmm::Run` has no abort handler, so it logs the syndrome and addresses and stops the VM as `FAULTED`. Three recent commits built this: `967e2c3` returned guest exits to the run loop, `4324037` captured the abort addresses, and `31105a0` routed exits through `Vmm`. `89de507` added the QEMU test.

# Steps

1. The guest runs a load or store to an IPA with no stage 2 mapping. The integration payload `tests/integration/abort/guest_payload.S` loads `0xDEAD0000` into `x0` with `movz` and reads from it.
2. The CPU takes a synchronous exception from a lower EL. `ESR_EL2.EC` is `0x24` (`EsrEc::DATA_ABORT_LOWER`) for data, or `0x20` (`INSTR_ABORT_LOWER`) for instructions.
3. `core/vmm/exceptions/vectors.S` group 3 entry 0 saves `x0` to `x3`, reads `ESR_EL2` into `x0` and branches to `vcpu_exit_sync`.
4. `core/vcpu/vcpu.S`, `vcpu_exit`: saves the guest context, then loads the `VcpuExit` pointer from the host frame and stores `reason` and `syndrome`. It zeroes the `far` and `hpfar` slots.
5. `vcpu_exit`: for a sync exit it extracts the class with `ubfx x4, x5, #26, #6` and compares it with `0x20`, `0x21`, `0x24` and `0x25`. On a match it reads `FAR_EL2` and `HPFAR_EL2` and stores them at offsets 16 and 24 of the `VcpuExit`.
6. `vcpu_exit` returns to `Vcpu::Run`, which returns the `VcpuExit` to `Vm::Enter` and `Vmm::Run`.
7. `core/vmm/vmm.cpp`, `Vmm::Run`: `GetEsrEc` gives `DATA_ABORT_LOWER`, which has no case, so the default branch runs. It logs `[Guest] Unhandled exception EC=... ISS=... ESR=... FAR=... HPFAR=...`.
8. `Vmm::Run`: `Vm::Stop(VmState::FAULTED)`, then returns the state.
9. `core/main.cpp`, `hmain`: logs `[VM] Guest stopped state=4` and idles in `wfe`.

# Gotchas

- `far` and `hpfar` are zero for any exit that is not one of the four abort classes. A zero is not evidence of an address.
- `HPFAR_EL2` is stored raw. It is not converted to a full IPA.
- The integration test (`guestAbortCapturesAddresses` in `tests/integration/vcpu/test_vcpu.cpp`) runs the `Vcpu` directly, not through `Vmm`. It builds a one block `GuestMmu`, calls `Enable(7)`, and puts back the old `VTTBR_EL2`, `VTCR_EL2` and `HCR_EL2` afterwards. It checks `reason == SYNC`, class `DATA_ABORT_LOWER`, `far == 0xDEAD0000` and `hpfar != 0`.
- `Vmm::Run` does not try to recover from an abort, so a guest that relies on a stage 2 fault being fixed up, such as lazy mapping, fails.
- Instruction aborts take the same path as data aborts. Only the class value differs.
