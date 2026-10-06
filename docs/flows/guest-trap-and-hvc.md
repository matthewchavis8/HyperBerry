---
type: Flow
title: Guest trap and HVC
description: How a guest exception reaches EL2, becomes a VcpuExit, is decoded and handled, and how the guest is resumed, with an HVC PSCI call as the example.
resource: ../../core/vmm/
tags: [trap, hvc, psci, exceptions]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vmm/exceptions/vectors.S
  - resource: ../../core/vcpu/vcpu.S
  - resource: ../../core/vcpu/vcpu.cpp
  - resource: ../../core/vm/vm.cpp
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/vmm/esr/esr.h
  - resource: ../../core/vmm/hvc/hvc.cpp
  - resource: ../../core/vmm/smc/smc.cpp
  - resource: ../../core/vmm/smc/smccc.h
---

# Summary

Every guest exit follows one path. The CPU takes the exception to EL2 through the lower EL AArch64 group of the vector table. The vector entry stashes four registers and jumps into `vcpu.S`, which saves the whole guest context, fills in a `VcpuExit` and returns to the caller of `Vcpu::Run`. `Vmm::Run` then decodes the syndrome and handles it. If the guest is still running it enters again.

# Steps

1. The guest executes `hvc` (or `smc`, or faults). The CPU switches to EL2 and jumps to `el2_vectors` in `core/vmm/exceptions/vectors.S`, group 3, entry 0 for a synchronous exception.
2. The vector entry pushes `x0` to `x3` on the EL2 stack, reads `ESR_EL2` into `x0` and branches to `vcpu_exit_sync`. IRQ, FIQ and SError use their own entries and branch to `vcpu_exit_irq`, `vcpu_exit_fiq` and `vcpu_exit_serror`.
3. `core/vcpu/vcpu.S`, `vcpu_exit_sync`: puts `VCPU_EXIT_SYNC` in `x1` and falls into `vcpu_exit`. The IRQ and FIQ labels zero `x0` first, so their syndrome is zero.
4. `vcpu_exit`: finds the `Vcpu` through `tpidr_el2`, stores guest `x0` to `x30` into it, then the guest EL1 system registers, `pc` from `ELR_EL2` and `pstate` from `SPSR_EL2`.
5. `vcpu_exit`: writes `reason` and `syndrome` into the `VcpuExit` the caller passed, zeroes `far` and `hpfar`, and fills those two only for abort classes. See [guest abort](guest-abort.md).
6. `vcpu_exit`: unwinds the host frame, restores the host registers and DAIF, and executes `ret`. Control is back in `Vcpu::Run`, which returns the `VcpuExit`.
7. `core/vm/vm.cpp`, `Vm::Enter`: stores the result in `m_lastExit` and returns it.
8. `core/vmm/vmm.cpp`, `Vmm::Run`: an IRQ or FIQ goes back to step 14 of [load and enter guest](load-and-enter-guest.md) immediately. SError stops the VM as `FAULTED`.
9. `Vmm::Run`: for a synchronous exit, `switch (GetEsrEc(exit.syndrome))` from `core/vmm/esr/esr.h`.
10. `EsrEc::HVC_AARCH64`: `Hvc::Handle(registers, GetEsrIss(exit.syndrome))` in `core/vmm/hvc/hvc.cpp`. A nonzero immediate or an owner other than `SMCCC::OWNER_STANDARD` returns `NOT_SUPPORTED` in `x[0]` and resumes.
11. `Hvc::Handle`: `PSCI_VERSION` puts `0x00010000` in `x[0]`. `PSCI_FEATURES` puts 0 or `NOT_SUPPORTED` there. `SYSTEM_OFF` returns `Action::SHUTDOWN` and `SYSTEM_RESET` returns `Action::RESET`. Anything else is `NOT_SUPPORTED`.
12. `Vmm::Run`: `SHUTDOWN` calls `Vm::Stop(VmState::SHUTDOWN)` and `RESET` calls `Vm::Stop(VmState::RESET_REQUESTED)`.
13. `EsrEc::SMC_AARCH64`: `Smc::Handle` puts `NOT_SUPPORTED` in `x[0]` and adds 4 to `pc`.
14. Any other class: logs `EC`, `ISS`, `ESR`, `FAR` and `HPFAR`, then `Vm::Stop(VmState::FAULTED)`.
15. `Vmm::Run`: if the state is no longer `RUNNING` it returns the state to `hmain`. Otherwise it loops back to `Vm::Enter`, and `vcpu_run` restores the saved registers, including the `x[0]` the handler wrote, and executes `eret`.

# Gotchas

- The handler's answer reaches the guest only because `Vcpu` keeps its registers in memory and `vcpu_run` reloads them on every entry. Handlers edit `GuestRegisters`, they never touch hardware.
- `Hvc::Handle` leaves `pc` alone and `Smc::Handle` adds 4. When writing a new handler, check what `pc` already holds on that exit.
- A guest that calls `hvc #0x42` (the integration test payload) hits the nonzero immediate check and gets `NOT_SUPPORTED`.
- Exceptions taken while EL2 itself is running do not use this path. They go to `el2_sync` and the other group 2 entries and panic. See [Vmm](../subsystems/vmm.md).
