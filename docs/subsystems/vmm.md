---
type: Subsystem
title: Vmm
description: The monitor that owns the single VM, runs its exit loop, decodes ESR_EL2, answers PSCI over HVC and SMC, and holds the EL2 exception vectors.
resource: ../../core/vmm/
tags: [virt, vmm, psci, exceptions]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vmm/vmm.h
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/vmm/esr/esr.h
  - resource: ../../core/vmm/hvc/hvc.h
  - resource: ../../core/vmm/hvc/hvc.cpp
  - resource: ../../core/vmm/smc/smc.h
  - resource: ../../core/vmm/smc/smc.cpp
  - resource: ../../core/vmm/smc/smccc.h
  - resource: ../../core/vmm/exceptions/exceptions.h
  - resource: ../../core/vmm/exceptions/exceptions.cpp
  - resource: ../../core/vmm/exceptions/entry.S
  - resource: ../../core/vmm/exceptions/vectors.S
  - resource: ../../core/main.cpp
  - resource: ../../core/CMakeLists.txt
  - resource: ../../CMakeLists.txt
---

# What it is

`Vmm` owns one `Vm` and runs it. `Vmm::Run` starts the guest, then loops: enter the guest, look at why it exited, handle that, and either go round again or return the final `VmState`. Interrupt exits go straight back in. A system error faults the guest. A synchronous exit is decoded with `GetEsrEc` and sent to `Hvc::Handle` for HVC or `Smc::Handle` for SMC. Anything else is logged and faults the guest. `Hvc` implements a small slice of PSCI (version, features, system off, system reset). `Smc` refuses every call. The folder also holds the EL2 vector table and the handlers for exceptions taken from EL2 itself, which are all fatal.

# Depends on

- [Vm](vm.md): `Vmm` holds a `Vm` and calls its private `Start`, `Enter`, `Stop` and `GetRegisters` as a friend.
- [Vcpu](vcpu.md): `GuestRegisters`, `VcpuExit` and `ExitReason` from `core/vcpu/vcpu.h`. The lower EL vectors branch to `vcpu_exit_*` in `vcpu.S`.
- [Lib](lib.md): `Log::Println` and `HvPanic`.
- `<expected>`: `Run` returns `std::expected<VmState, RunError>`.

# Used by

- `core/main.cpp` includes `core/vmm/vmm.h`, builds a `Vmm` from a `VmConfig` and calls `Run`.
- `bsp/qemu/boot.S` writes the address of `el2_vectors` into `VBAR_EL2` ([boot to main](../flows/boot-to-main.md), [bsp](bsp.md)).
- The top level `CMakeLists.txt` links `vectors.S` and `entry.S` into the image through `HB_ENTRY_SOURCES`.
- Tests: `tests/unit/vmm/test_vmm.cpp`, `test_hvc.cpp`, `test_esr.cpp`, `tests/unit/vm/test_vm.cpp`, and `tests/integration/gic/test_gic.cpp`, `tests/integration/timer/test_timer.cpp`, `tests/integration/vcpu/test_vcpu.cpp`, which include `esr.h` and `exceptions.h`.

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `core/vmm/vmm.h` | Public interface | `Vmm`, `RunError` | `core/main.cpp` |
| `core/vmm/vmm.cpp` | The run loop and exit dispatch | `Vmm::Run`, `Vmm::GetState`, `Vmm::GetLastExit` | `hmain` |
| `core/vmm/esr/esr.h` | ESR_EL2 field decode, header only | `EsrEc`, `GetEsrEc`, `GetEsrIss` | `vmm.cpp`, tests |
| `core/vmm/hvc/hvc.h` | HVC handler interface | `Hvc::Action`, `Hvc::Handle` | `Vmm::Run` |
| `core/vmm/hvc/hvc.cpp` | PSCI over HVC | `Hvc::Handle`; internal `Psci::VERSION`, `SYSTEM_OFF`, `SYSTEM_RESET`, `FEATURES`, `Psci::Supports` | `Vmm::Run` |
| `core/vmm/smc/smc.h` | SMC handler interface | `Smc::Handle` | `Vmm::Run` |
| `core/vmm/smc/smc.cpp` | Rejects every SMC and steps past it | `Smc::Handle` | `Vmm::Run` |
| `core/vmm/smc/smccc.h` | SMCCC function ID fields and return codes | `SMCCC::GetOwner`, `IsFastCall`, `ToRegister`, `OWNER_*`, `SUCCESS`, `NOT_SUPPORTED` | `hvc.cpp`, `smc.cpp` |
| `core/vmm/exceptions/exceptions.h` | Saved EL2 frame and fatal handler declarations | `El2ExceptionFrame`, `handle_el2_sync`, `handle_el2_irq`, `handle_el2_fiq`, `handle_el2_serror`, `handle_unhandled` | `entry.S`, tests |
| `core/vmm/exceptions/exceptions.cpp` | Each handler panics with the saved registers | the five `handle_*` functions | `entry.S` |
| `core/vmm/exceptions/entry.S` | Saves and restores the 272 byte frame and calls the C handlers | `save_context`, `restore_context`, `el2_sync`, `el2_irq`, `el2_fiq`, `el2_serror`, `el2_unhandled` | `vectors.S` |
| `core/vmm/exceptions/vectors.S` | The EL2 vector table | `el2_vectors` | `boot.S` (`VBAR_EL2`) |

# Entry points

- `Vmm::Run()`: returns `RunError::INVALID_STATE` unless the VM is `READY`, so a VM runs once. Otherwise it returns the `VmState` that stopped it (`SHUTDOWN`, `RESET_REQUESTED` or `FAULTED`).
- `Hvc::Handle(registers, immediate)`: returns `Action::RESUME`, `SHUTDOWN` or `RESET`. It writes the PSCI result into `x[0]`.
- `Smc::Handle(registers)`: always writes `NOT_SUPPORTED` into `x[0]`.
- `el2_vectors`: the table installed in `VBAR_EL2`.

# Gotchas

- The vector table has four groups of four entries, `0x80` bytes apart. Group 1 (current EL with `SP_EL0`) and group 4 (lower EL AArch32) branch to `el2_unhandled`. Group 2 (current EL with `SP_EL2`) goes to `el2_sync`, `el2_irq`, `el2_fiq`, `el2_serror`. Group 3 (lower EL AArch64) is the guest path and does not use `El2ExceptionFrame` at all.
- Every exception taken from EL2 itself is fatal. All five `handle_*` functions call `HvPanic` and are marked `[[noreturn]]`, so the `restore_context` after each `bl` in `entry.S` is never reached. An interrupt arriving while the hypervisor is running, rather than the guest, panics.
- The group 3 entries push `x0` to `x3` on the EL2 stack before branching. `vcpu_exit` in `vcpu.S` reads them back at fixed offsets, as the comment in `vectors.S` lists. Changing either file alone breaks the exit path.
- IRQ and FIQ exits from the guest are not handled in `Vmm::Run`. The loop just enters the guest again. Interrupt delivery is outside this subsystem.
- A `SERROR` exit sets `FAULTED` with no log line. Other unhandled syndromes log `EC`, `ISS`, `ESR`, `FAR` and `HPFAR` before faulting. A guest data abort takes that second path ([guest abort](../flows/guest-abort.md)).
- `Hvc::Handle` rejects any call with a nonzero HVC immediate, or with an owner other than `SMCCC::OWNER_STANDARD` (4), by returning `NOT_SUPPORTED` in `x[0]` and resuming. It then matches on the exact 32 bit PSCI IDs `0x84000000`, `0x84000008`, `0x84000009` and `0x8400000A`. A 64 bit PSCI ID is not recognised and also returns `NOT_SUPPORTED`.
- `PSCI_FEATURES` answers 0 for exactly those four functions and `NOT_SUPPORTED` for the rest. `PSCI_VERSION` returns `0x00010000`.
- `Hvc::Handle` does not touch `pc`, while `Smc::Handle` adds 4 to it. Check the saved `pc` convention before adding a new handler.
- `SYSTEM_RESET` does not reset anything. It stops the VM with `RESET_REQUESTED`, `Run` returns, and `hmain` logs the state and idles. Reboot is not implemented.
- The `IsFastCall` helper in `smccc.h` is not called by `Hvc::Handle`. The call type bit plays no part in dispatch.
- `GetEsrEc` returns an `EsrEc` value even for a class the enum does not list, because it casts the six bit field directly.
- The `Virt` library compiles `vmm.cpp`, `hvc.cpp`, `smc.cpp` and `exceptions.cpp`. The `.S` files are not part of `Virt`. They are linked into the image through `HB_ENTRY_SOURCES`, so they are not archive members.
- Commit `31105a0` moved `esr.h` into `esr/`, `smccc.h` into `smc/`, and split the old `vmm.S` into `exceptions/entry.S`. Older notes that mention `core/vmm/esr.h` or `core/vmm/vmm.S` are out of date.
