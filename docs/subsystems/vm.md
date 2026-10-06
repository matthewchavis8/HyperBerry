---
type: Subsystem
title: Vm
description: One guest, its stage 2 address space, its virtual CPU and its lifecycle state, configured by VmConfig.
resource: ../../core/vm/
tags: [virt, vm]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vm/vm.h
  - resource: ../../core/vm/vm.cpp
  - resource: ../../core/main.cpp
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/mm/mmu/guestMmu/guestMmu.cpp
  - resource: ../../core/CMakeLists.txt
---

# What it is

A `Vm` is one guest: a `GuestMmu` (stage 2 tables), a `Vcpu`, a name, a VMID, a `VmState` and the last `VcpuExit`. `VmConfig` says where the guest lives (IPA base, host physical RAM, size), how it starts (entry IPA, device tree IPA) and what it is called. The constructor builds the stage 2 mappings and seeds `x0` with the guest tree address, which is the Linux arm64 boot register. It does not run anything. All the operations that change the guest (`Start`, `Enter`, `Stop`, `GetRegisters`) are private, and `Vmm` is a friend, so [Vmm](vmm.md) is the only driver. A `Vm` is not copyable or movable.

# Depends on

- [Vcpu](vcpu.md): owns a `Vcpu` built from `config.entry`.
- [Mm](mm.md): `GuestMmu` and `MmioMap`, from `core/mm/mmu/guestMmu/guestMmu.h`.
- [Lib](lib.md): `Log::Println`.

# Used by

- `core/vmm/vmm.h` includes `core/vm/vm.h`, and `Vmm` holds the single `Vm` ([Vmm](vmm.md)).
- `core/main.cpp` builds a `VmConfig`. It gets `Vm` through `core/vmm/vmm.h`.
- Tests: `tests/unit/vm/test_vm.cpp`.

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `core/vm/vm.h` | Config, state enum and the class | `VmState`, `VmConfig`, `Vm`, `Vm::GetState`, `GetLastExit`, `GetName`, `GetVmId` | `core/vmm/vmm.h`, `core/main.cpp` |
| `core/vm/vm.cpp` | Construction, MMU activation, entering the guest | `Vm::Vm`, `Start`, `Enter`, `Stop`, `GetRegisters` | `Vmm` |

# Entry points

- `Vm(const VmConfig&, const MmioMap& devices)`: builds `GuestMmu` over `ipaBase`, `ramHostPa`, `ramSize` and the device windows, constructs the `Vcpu` at `entry`, and sets `x[0]` to `config.dtb`.
- `Start()`: calls `GuestMmu::Enable(vmid)` and moves the state to `RUNNING`.
- `Enter()`: runs the vCPU until its next exit and stores the result in `m_lastExit`.
- `Stop(VmState)`: only assigns the state.

# Gotchas

- `VmState` is `READY`, `RUNNING`, `SHUTDOWN`, `RESET_REQUESTED`, `FAULTED`, in that order. `hmain` prints the numeric value, so a faulted guest shows as 4.
- Construction builds the tables but does not touch `VTTBR_EL2` or `HCR_EL2`. That happens in `Start()` through `GuestMmu::Enable`, which programs `VTTBR_EL2` with the VMID in bits 48 and up, sets `HCR_EL2.VM` and flushes the guest TLB. A `Vm` that is built but never started changes no hardware state.
- `VmConfig::name` is a `std::string_view` and `Vm` stores it unchanged, so the string must outlive the VM. `hmain` passes a literal.
- `VmConfig::vmid` must be nonzero and unique across live VMs. Nothing in `Vm` checks that.
- The `dtb` field is a guest IPA, while `ramHostPa` is a host physical address. `x[0]` receives the IPA because the guest reads it.
- `Stop` does not tear anything down. Stage 2 stays enabled after the guest stops, until something else changes `HCR_EL2`.
- `GetLastExit` returns the exit of the most recent `Enter`. It is zeroed at construction.
