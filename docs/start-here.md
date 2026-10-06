---
type: Guide
title: Start here
description: The whole hypervisor in one page, how to look up any file, and a reading order.
resource: ../
tags: [overview]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../AGENTS.md
  - resource: ../CMakeLists.txt
  - resource: ../core/main.cpp
  - resource: ../bsp/qemu/boot.S
---

# The hypervisor in one paragraph

HyperBerry is a bare metal Armv8-A type 1 hypervisor. It runs at EL2 on a Raspberry Pi 5 (BCM2712)
or on QEMU `virt`, and today it boots one Linux guest at EL1 with one virtual CPU. Firmware hands
it a device tree and a CPIO archive holding the guest's kernel `Image`, guest tree and optional
initrd. `hmain` in `core/main.cpp` parses the host tree, brings up the physical page allocator and
the host MMU, loads the guest into fresh RAM, builds stage 2 tables for it, and runs it. Every guest
exit comes back to EL2 and is handled by `Vmm`.

# How to look a file up

- Code is in five libraries, layered: `Core` then `Virt` then `Mm` then `Drivers` then `Lib`. See [layering](architecture/layering.md).
- Find the folder in [subsystems](subsystems/), then read its page for the files, the types and who calls them.
- Board specific files live in `bsp/<board>/` ([boards](subsystems/bsp.md)).
- Addresses come from device trees, not from source. See [device tree to regs.inc](flows/device-tree-to-regs.md).

# Reading order

1. [Glossary](glossary.md)
2. [Boot to main](flows/boot-to-main.md)
3. [Mm](subsystems/mm.md), then [Device tree](subsystems/device-tree.md) and [Boot loader](subsystems/boot-loader.md)
4. [Vcpu](subsystems/vcpu.md), [Vm](subsystems/vm.md), [Vmm](subsystems/vmm.md)
5. [Guest trap and HVC](flows/guest-trap-and-hvc.md)
6. [Testing](guides/TESTING.md)

# Status of these pages

Every page is marked `status: unverified`. The bundle was written from a read of the headers and
entry points at `dev` 027a381. Change a page to `verified` once a person has checked it against
the code.
