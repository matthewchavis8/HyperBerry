---
type: Index
title: Flows
description: Every flow page, one line each, from reset to a running guest and the paths a guest exit takes.
resource: ../../core/
tags: [flows]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/main.cpp
  - resource: ../../bsp/qemu/boot.S
---

# Flows

- [Boot to main](boot-to-main.md): reset at `bsp/qemu/boot.S` through `hmain` to the point the guest runs.
- [Load and enter guest](load-and-enter-guest.md): the guest archive becomes guest RAM, a patched tree, stage 2 tables and a first `eret`.
- [Guest trap and HVC](guest-trap-and-hvc.md): a guest exception reaches EL2, is decoded in `Vmm::Run` and handled, with PSCI over HVC as the example.
- [Guest abort](guest-abort.md): a stage 2 fault is captured with `FAR_EL2` and `HPFAR_EL2` and ends the VM as `FAULTED`.
- [GIC interrupt](gic-interrupt.md): how an interrupt travels through the GIC.
- [Timer tick](timer-tick.md): how the timer raises its tick.
- [Host MMU bring up](host-mmu-bring-up.md): the host stage 1 map is built from the device tree and enabled.
- [Guest stage 2 mapping](guest-stage2-mapping.md): how the guest's stage 2 tables are built.
- [Device tree to regs.inc](device-tree-to-regs.md): `scripts/bspgen` turns a host tree into the generated defines.
- [Integration run](integration-run.md): how the bare metal TAP suite is built and run.
