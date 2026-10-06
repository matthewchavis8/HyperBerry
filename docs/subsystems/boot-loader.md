---
type: Subsystem
title: Boot loader
description: Loads the Linux kernel, guest tree and initrd out of the firmware archive into fresh guest RAM and patches the guest tree, plus the hmain entry that drives it.
resource: ../../core/bootLoader/
tags: [core, boot, guest-archive]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/bootLoader/bootLoader.h
  - resource: ../../core/bootLoader/bootLoader.cpp
  - resource: ../../core/main.cpp
  - resource: ../../core/CMakeLists.txt
  - resource: ../../bsp/qemu/boot.S
---

# What it is

`BootLoader` takes a CPIO archive that firmware placed in memory and gets a Linux guest ready to run. It finds `linux/Image`, `linux/guest.dtb` and optionally `linux/initrd` (`ReadFiles`), picks a spot for each inside the guest's 256 MiB of RAM (`CalculateLayout`), then allocates that RAM from the PMM, copies the files in and patches the guest tree (`Load`). The patch fills three placeholders the tree must already contain: `/memory` `reg`, and `linux,initrd-start` and `linux,initrd-end` under `/chosen`. The archive format is in [guest archive](../guides/GUEST_ARCHIVE.md).

`core/main.cpp` is documented here too. It holds `hmain`, the C++ entry that `bsp/qemu/boot.S` branches to. It runs the whole boot in order and, on the normal build, ends by running the guest through [Vmm](vmm.md). See [boot to main](../flows/boot-to-main.md) and [load and enter guest](../flows/load-and-enter-guest.md).

# Depends on

- [Device tree](device-tree.md): `core/deviceTree/fdt.h` for the header, tokens and byte order helpers. `hmain` also uses `TreeParser` and `MemoryMap`.
- [Mm](mm.md): `Pmm` (`AllocPages`, `FreePages`), `HostMmu::PaToVa`, `PageTable::CleanDataCacheRange`.
- [Lib](lib.md): `cpio::Archive` and `cpio::File` from `lib/cpio/cpio.h`, `memcpy`, `StrEq` and `StrStartsWith` from `lib/strings/strings.h`, `Log`, `HvPanic`, `RunGlobalConstructors`.
- [Vmm](vmm.md) and [Vm](vm.md): `hmain` builds a `VmConfig` and a `Vmm`.

# Used by

- `core/main.cpp` includes `core/bootLoader/bootLoader.h` for `BootLoader`, `GuestLayout`, `GUEST_IPA_BASE` and `GUEST_RAM_SIZE`.
- `tests/unit/bootLoader/test_bootLoader.cpp` and `tests/integration/bootLoader/test_bootLoader.cpp`.

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `core/bootLoader/bootLoader.h` | Constants, the layout types and the loader class | `GUEST_IPA_BASE`, `GUEST_RAM_SIZE`, `KERNEL_LOAD_IPA`, `GuestFiles`, `GuestLayout::IpaToHostPa`, `BootLoader` | `core/main.cpp`, tests |
| `core/bootLoader/bootLoader.cpp` | Archive lookup, layout maths, RAM allocation, copy and cache clean, and the guest tree patcher | `BootLoader::ReadFiles`, `CalculateLayout`, `Load`; internal `DtbPatcher`, `GuestRamDeleter` | `hmain` |
| `core/main.cpp` | `hmain`, the hypervisor entry after assembly setup | `hmain` | `bsp/qemu/boot.S` (`bl hmain`) |

# Entry points

- `hmain(uintptr_t dtb)`: `extern "C"`, never returns. The argument is the physical address of the firmware tree.
- `BootLoader::Load(GuestLayout&)`: the one call `hmain` makes. `ReadFiles` and `CalculateLayout` are public so tests can call them alone, and `CalculateLayout` is static.

# Gotchas

- The layout is fixed by constants: guest RAM starts at IPA `0x40000000`, is 256 MiB, and the kernel is at `KERNEL_LOAD_IPA`, which is `GUEST_IPA_BASE + 0x200000`. That address is also the entry point.
- The initrd goes at the top of guest RAM, aligned down to 2 MiB. The tree goes below the initrd, aligned down to 64 KiB. Both must sit above the end of the kernel, otherwise `CalculateLayout` returns false.
- `ReadFiles` rejects an archive with an empty `linux/Image` or `linux/guest.dtb`, and also one where `linux/initrd` is present but empty. A missing initrd is fine.
- The guest tree must already contain `/memory` `reg` of 16 bytes and `/chosen` `linux,initrd-start` and `linux,initrd-end` of 8 bytes each. The patcher fails the load if any is missing, wrongly sized, or appears twice. A guest tree without an initrd placeholder cannot boot even when the archive has no initrd. With no initrd, both properties are written as zero.
- The patcher matches nodes by name at depth one only: `memory`, `memory@...` and `chosen`.
- `ramHostPa` is allocated as one buddy block of `GUEST_RAM_ORDER`, which is the smallest order covering 256 MiB. The block is held by a `unique_ptr` with a deleter, so every failure after allocation returns it to the PMM. On success `Load` releases ownership and the caller owns it.
- Each copy is followed by `PageTable::CleanDataCacheRange` so the guest sees the bytes with its caches off. The tree is cleaned a second time after patching.
- `Load` on failure leaves `out` cleared, so `ramHostPa` is zero. `hmain` then logs `archive.GetError()` and panics with "Failed to spin up Linux VM".
- `GuestLayout::IpaToHostPa` is only valid for IPAs inside guest RAM and does no range check.
- `hmain` runs the guest only when `INTEGRATION_TEST` is not defined. The integration build swaps in `CoreTest`, which differs only in this macro, and calls `TestRunner::SetBootContext` and `TestRunner::RunAll` instead.
- When `Vmm::Run` returns, `hmain` logs the state and then spins on `wfe`. Nothing restarts the guest, so a PSCI reset ends the same way as a shutdown.
