Architecture
============

Design Goals
------------

- **Bare-metal execution** — boot from firmware directly into EL2 with no Linux dependency at runtime.
- **Multi-guest support** — run two or more guest VMs with independent virtual address spaces and device assignments.
- **Hardware-assisted virtualization** — leverage Armv8-A VHE and Stage-2 translation for memory isolation.
- **Minimal trusted code base** — keep the hypervisor small and auditable; push complexity into guests.
- **Educational clarity** — serve as a readable reference for Arm virtualization on real hardware.

Exception Level Layout
----------------------

.. code-block:: text

   ┌──────────────────────────────────────────────────┐
   │                   Guest OS (EL1)                 │
   │              Linux / RTOS / bare-metal           │
   ├──────────────────────────────────────────────────┤
   │               HyperBerry (EL2)                   │
   │  ┌───────────┬───────────┬──────────┬──────────┐ │
   │  │  vCPU     │  Stage-2  │  vGIC    │  vTimer  │ │
   │  │  Sched    │  MMU      │          │          │ │
   │  └───────────┴───────────┴──────────┴──────────┘ │
   ├──────────────────────────────────────────────────┤
   │               Hardware (EL3 / TF-A)              │
   └──────────────────────────────────────────────────┘

HyperBerry occupies EL2. Guests run at EL1 and are isolated via
Stage-2 address translation. Virtual interrupts and timers are
managed by the hypervisor.

Boot Sequence
-------------

The AArch64 assembly entry point (``boot.S``) runs before any C++ code:

1. Park all non-zero cores in a ``wfe`` spin loop.
2. Verify EL2 via ``CurrentEL`` — park core 0 if not at EL2.
3. Configure ``HCR_EL2``: set RW bit to force AArch64 guests.
4. Configure ``SCTLR_EL2``: disable MMU, D-cache, I-cache until page tables are ready.
5. Configure ``CNTHCTL_EL2``: grant guests access to the physical counter and timer.
6. Shadow ``MIDR/MPIDR`` into ``VPIDR/VMPIDR``.
7. Zero the ``.bss`` section using linker-exported symbols.
8. Set ``SP_EL2`` to ``__stack_end`` and branch to ``hmain()``.

Development Roadmap
-------------------

Phase 0 — Bootstrap *(current)*
  - [x] Enter EL2, initialize BSS, set up EL2 stack
  - [x] PL011 UART driver for early debug output
  - [ ] Exception vector table and synchronous exception handling

Phase 1 — Memory Management
  - [ ] Physical page allocator (buddy or bitmap)
  - [ ] EL2 Stage-1 page tables
  - [ ] Stage-2 page tables (guest IPA → PA translation)

Phase 2 — Single Guest
  - [ ] Load a flat binary guest image into isolated memory
  - [ ] Context switch: save/restore EL1 system registers
  - [ ] Trap and emulate essential system registers

Phase 3 — Interrupt Virtualization
  - [ ] GIC-400 distributor and CPU interface initialization
  - [ ] Virtual interrupt injection via List Registers
  - [ ] Route physical interrupts to the correct guest

Phase 4 — Multi-Guest
  - [ ] vCPU scheduler (round-robin or fixed time-slice)
  - [ ] Per-VM Stage-2 address spaces with VMID tagging

Phase 5 — Device Passthrough & Virtio
  - [ ] MMIO trap-and-emulate for shared devices
  - [ ] Virtio-mmio transport for virtual block/network devices
