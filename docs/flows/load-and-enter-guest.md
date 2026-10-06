---
type: Flow
title: Load and enter guest
description: How the guest archive becomes a running EL1 guest, from BootLoader::Load to the first eret.
resource: ../../core/bootLoader/
tags: [boot, guest, stage2]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/main.cpp
  - resource: ../../core/bootLoader/bootLoader.cpp
  - resource: ../../core/bootLoader/bootLoader.h
  - resource: ../../core/vm/vm.cpp
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/vcpu/vcpu.cpp
  - resource: ../../core/vcpu/vcpu.S
  - resource: ../../core/mm/mmu/guestMmu/guestMmu.cpp
---

# Summary

After the host MMU is up, `hmain` reads the guest archive firmware left in RAM, copies the kernel, tree and initrd into a freshly allocated 256 MiB block, patches the guest tree, builds stage 2 tables over that block and the guest's devices, and runs the guest. The last step is an `eret` at the kernel entry with `x0` holding the tree address.

# Steps

1. `core/main.cpp`, `hmain`: builds `cpio::Archive archive { HostMmu::PaToVa(memoryMap.cpioArchiveBase), memoryMap.cpioArchiveSize }` and `BootLoader loader { archive }`.
2. `hmain`: `loader.Load(layout)`.
3. `core/bootLoader/bootLoader.cpp`, `BootLoader::ReadFiles`: finds `linux/Image`, `linux/guest.dtb` and optional `linux/initrd`. Empty files are rejected.
4. `BootLoader::CalculateLayout`: kernel at `KERNEL_LOAD_IPA`, initrd at the top of guest RAM on a 2 MiB boundary, tree below it on a 64 KiB boundary.
5. `BootLoader::Load`: `Pmm::GetInstance().AllocPages(GUEST_RAM_ORDER)` gives `ramHostPa`. A `unique_ptr` with `GuestRamDeleter` guards it.
6. `Load`: copies the kernel and the tree through `HostMmu::PaToVa` and cleans the data cache over each range with `PageTable::CleanDataCacheRange`.
7. `Load`: `DtbPatcher::Run` fills `/memory` `reg` and the two `/chosen` initrd properties in the copied tree, then the tree range is cleaned again.
8. `Load`: copies the initrd if present, releases the RAM guard and returns the layout. On failure `hmain` logs `archive.GetError()` and calls `HvPanic`.
9. `hmain`: `TreeParser guestTree { layout.IpaToHostPa(layout.dtbIpa) }` reads the patched tree from its host physical address.
10. `hmain`: fills `VmConfig` (`ipaBase` `GUEST_IPA_BASE`, `ramSize` `GUEST_RAM_SIZE`, `vmid` 1, `entry` the kernel IPA, `dtb` the tree IPA) and calls `guestTree.GetGuestMmio()` to build the guest device windows.
11. `Vmm::Vmm` constructs `Vm`. `Vm::Vm` (`core/vm/vm.cpp`) builds `GuestMmu` over those arguments, constructs `Vcpu { config.entry }` and sets `x[0]` to `config.dtb`.
12. `Vcpu::Vcpu` (`core/vcpu/vcpu.cpp`): `pc` is the entry, `pstate` is `0x3C5`, and `sctlr` is `vcpu_read_sctlr()` with the MMU, alignment, cache and stack alignment bits cleared.
13. `hmain`: `vmm.Run()`. `Vmm::Run` checks the VM is `READY`, then `Vm::Start` calls `GuestMmu::Enable(vmid)`, which writes `VTTBR_EL2`, sets `HCR_EL2.VM` and flushes the guest TLB. The state becomes `RUNNING`.
14. `Vmm::Run` loop: `Vm::Enter` calls `Vcpu::Run`, which calls `vcpu_run(this, &exit)` in `core/vcpu/vcpu.S`.
15. `vcpu_run`: masks DAIF, saves the host registers on the EL2 stack, stores the `Vcpu` pointer in `tpidr_el2`, loads the guest EL1 system registers, then `SCTLR_EL1`, then `ELR_EL2` and `SPSR_EL2` from the guest `pc` and `pstate`, then the guest `x` registers, and executes `eret`.

# Gotchas

- The guest tree is read as a host physical address in step 9. It is the same bytes the boot loader patched, not a second copy.
- `Load` leaves the allocated RAM owned by the caller. Nothing in `hmain` frees it.
- The guest starts with stage 1 off, so it reads the kernel and tree straight from guest physical memory. That is why every copy is followed by a cache clean.
- `Vm::Vm` does not enable stage 2. Until `Vmm::Run` calls `Start`, no hardware register has changed.
- Guest `x1` to `x30` start at zero. Only `x0` is seeded.
