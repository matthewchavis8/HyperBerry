# HyperBerry — Internal Design Notes

Internal reference for goals, roadmap, and design decisions. The README is the source of truth for project overview and build info.

## Objectives

- **Bare-metal execution** — boot from firmware directly into EL2; no Linux dependency at runtime.
- **Multi-guest support** — run two or more guest VMs with independent virtual address spaces and device assignments.
- **Hardware-assisted virtualization** — leverage Armv8-A VHE (Virtualized Host Extension) and Stage-2 translation for memory isolation.
- **Minimal trusted code base** — keep the hypervisor small and auditable; push complexity into guests.
- **Educational clarity** — serve as a readable reference for Arm virtualization on real hardware.

## Roadmap

### Phase 0 — Bootstrap
- [x] Minimal boot: enter EL2, initialize BSS, set up stack
- [x] PL011 UART driver for early debug output
- [ ] Exception vector table and synchronous exception handling

### Phase 1 — Memory Management
- [ ] Physical page allocator (buddy or bitmap)
- [ ] EL2 Stage-1 page tables (hypervisor virtual address space)
- [ ] Stage-2 page tables (guest IPA → PA translation)

### Phase 2 — Single Guest
- [ ] Load a flat binary guest image into isolated memory
- [ ] Context switch: save/restore EL1 system registers
- [ ] Trap and emulate essential system registers (VBAR_EL1, SCTLR_EL1, etc.)
- [ ] Arm Generic Timer virtualization (CNTVOFF_EL2)

### Phase 3 — Interrupt Virtualization
- [ ] GIC distributor and CPU interface initialization
- [ ] Virtual interrupt injection via List Registers
- [ ] Route physical interrupts to the correct guest

### Phase 4 — Multi-Guest
- [ ] vCPU scheduler (round-robin or fixed time-slice)
- [ ] Per-VM Stage-2 address spaces with VMID tagging
- [ ] Memory partitioning and guest resource limits

### Phase 5 — Device Passthrough & Virtio
- [ ] MMIO trap-and-emulate for shared devices
- [ ] Device passthrough via Stage-2 identity mapping
- [ ] Virtio-mmio transport for virtual block / network devices (stretch goal)

## Questions to ask

### Bootstrap Questions
 #### Toolchain requirements to cross compile for ARM? [ x ]
    **Results:** so settled on using aarch64-unknown-elf for cross compilations for baremetal
      then for unit testing I will just cross compile for ARM aarch64-gcc then run it in a       QEMU.
 #### How can I flash the SD card? [ ]
   ##### How does the boot sequence work for a Raspberry PI5? [ ]
 #### How do you get from the firmware to EL2 for booting the hypervisor [ ]
 #### Do I need to have a linker script to layout the Hypervisor code specefically in memory? [ ]
 #### Do I need to create a device tree blob hardware descriptor? [ ]

## AI Use Declaration

AI tools (primarily large language models) were used for **documentation only** —
including Doxygen comments, README text, and design notes in this file.
All hypervisor source code (assembly, C++, linker scripts, and build system
configuration) was written by hand.
