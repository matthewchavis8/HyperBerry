---
type: Guide
title: Glossary
description: Every term of art the pages use, in one or two sentences each.
resource: ../
tags: [overview]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../AGENTS.md
---

# Arm

- **EL0, EL1, EL2**: Arm exception levels. Guest applications run at EL0, the guest kernel at EL1, HyperBerry at EL2.
- **HCR_EL2**: the EL2 control register that decides what traps to the hypervisor. Its RW bit forces guests into AArch64.
- **Stage 1**: the translation a kernel controls, virtual address to intermediate physical address. HyperBerry's own EL2 map is a stage 1 regime too ([host MMU](subsystems/mm.md)).
- **Stage 2**: the translation the hypervisor controls, IPA to physical address. One table set per guest.
- **IPA**: intermediate physical address, what the guest believes is physical memory.
- **VTTBR_EL2 / VTCR_EL2**: the stage 2 root pointer with its VMID, and the stage 2 configuration.
- **VMID**: the number tagging a guest's TLB entries; nonzero and unique across live VMs.
- **ESR_EL2, FAR_EL2, HPFAR_EL2**: the syndrome, faulting virtual address and faulting IPA of an exception taken to EL2.
- **HVC / SMC**: instructions a guest uses to call the hypervisor or firmware.
- **SMCCC**: the Arm calling convention for HVC and SMC. Function IDs carry an owner field.
- **PSCI**: the power state interface a guest uses to turn itself off or reset. The guest tree declares it with method `hvc`.
- **GIC**: the generic interrupt controller. The QEMU and Pi 5 trees describe a GIC 400, a GICv2 part with hypervisor frames.
- **GICH / GICV**: the GIC's hypervisor control frame and virtual CPU interface frame. Only EL2 may touch GICH.

# The project

- **FDT / DTB / DTS**: a flattened device tree, its binary form, and the source it compiles from.
- **Host tree**: the device tree firmware gives the hypervisor. **Guest tree**: the one handed to Linux, built from `guest-linux.dtsi` plus a board file.
- **BSP**: board support package, the `bsp/<board>/` folder.
- **bspgen**: `scripts/bspgen`, which turns a host tree into `regs.inc`.
- **regs.inc**: generated defines for the few addresses that must be compile time constants.
- **Guest archive**: the CPIO file with `linux/Image`, `linux/guest.dtb` and optional `linux/initrd`. See [guest archive](guides/GUEST_ARCHIVE.md).
- **PMM**: the physical memory manager, a buddy allocator.
- **TAP**: the Test Anything Protocol; the bare metal integration suites print it over the UART.
- **Singleton**: the UART, the GIC, the PMM, the heap and the host MMU each expose one instance through `GetInstance()`. The timer does not.
