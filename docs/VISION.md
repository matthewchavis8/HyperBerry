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
- [x] Exception vector table and synchronous exception handling
- [ ] Write tutorial articles for writing a type 1 hypervisor for the RPI5

### Phase 1 — Memory Management
- [x] Physical page allocator (buddy allocator)
- [ ] EL2 Stage-1 page tables (hypervisor virtual address space)
- [ ] Stage-2 page tables (guest IPA → PA translation)
- [ ] Write tutorial articles for writing a type 1 hypervisor for the RPI5

### Phase 2 — Single Guest
- [ ] Load a flat binary guest image into isolated memory
- [ ] Context switch: save/restore EL1 system registers
- [ ] Trap and emulate essential system registers (VBAR_EL1, SCTLR_EL1, etc.)
- [ ] Arm Generic Timer virtualization (CNTVOFF_EL2)
- [ ] Write tutorial articles for writing a type 1 hypervisor for the RPI5

### Phase 3 — Interrupt Virtualization
- [ ] GIC distributor and CPU interface initialization
- [ ] Virtual interrupt injection via List Registers
- [ ] Route physical interrupts to the correct guest
- [ ] Write tutorial articles for writing a type 1 hypervisor for the RPI5

### Phase 4 — Multi-Guest
- [ ] vCPU scheduler (round-robin or fixed time-slice)
- [ ] Per-VM Stage-2 address spaces with VMID tagging
- [ ] Memory partitioning and guest resource limits

### Phase 5 — Device Passthrough & Virtio
- [ ] MMIO trap-and-emulate for shared devices
- [ ] Device passthrough via Stage-2 identity mapping
- [ ] Virtio-mmio transport for virtual block / network devices (stretch goal)
