---
type: Flow
title: GIC interrupt to guest
description: How a physical interrupt is acknowledged at EL2, injected into a guest through a list register and finished by the guest, as the GIC integration suite runs it.
resource: ../../drivers/gic/
tags: [gic, interrupts, guest]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../drivers/gic/gic.h
  - resource: ../../drivers/gic/gic.cpp
  - resource: ../../tests/integration/gic/test_gic.cpp
  - resource: ../../tests/integration/gic/test_vectors.S
  - resource: ../../tests/integration/gic/guest_payload.S
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../core/vmm/exceptions/exceptions.cpp
  - resource: ../../core/vmm/vmm.cpp
---

# Summary

A physical shared interrupt reaches EL2, the hypervisor acknowledges it, writes a virtual copy into a GICH list register
with the hardware bit set, and drops the priority. The guest then reads and ends the virtual interrupt through the GICV frame,
and the hardware bit makes that also deactivate the physical interrupt.

This path exists today only in `tests/integration/gic/`, which installs its own vector table. The production vector
table is different: `handle_el2_irq` in `core/vmm/exceptions/exceptions.cpp` calls `HvPanic`, and a guest IRQ exit makes
`Vmm::Run` in `core/vmm/vmm.cpp` run the guest again without calling the `Gic`. The steps below are the sequence the
test proves, using the driver functions a production handler would call. See [drivers](../subsystems/drivers.md) for the driver itself.

# Steps

1. `drivers/gic/gic.cpp`, `Gic::GetInstance`: the first call constructs the driver with the `BSP_GIC_*_BASE` values and calls `Gic::Reset`, which sets up the distributor and then runs `cpuInit` (CPU interface, GICH virtual interface, list registers cleared).
2. `core/deviceTree/deviceTree.cpp`, `TreeParser::GetHostMmio`: calls `Gic::SetBases` with the frames read from the host tree. If any base differs from the generated value, `Reset` runs again.
3. `tests/integration/gic/test_gic.cpp`, `runEndToEnd`: calls `Gic::Reset`, clears the test SPI (id 64), moves it to Group 0 in IGROUPR, routes it to CPU 0 with a byte write to ITARGETSR, sets bits 0 and 1 of the distributor control register, then calls `Gic::SetPriorityLevel` and `Gic::EnableIrq`.
4. `enableVirtualIrqRouting` sets HCR_EL2.IMO and RW, and `installTestVbar` points VBAR_EL2 at `test_gic_vectors`.
5. `injectFromPhysicalSpi` pends the SPI in ISPENDR and unmasks IRQ in DAIF. The interrupt is taken at the current EL IRQ slot of `tests/integration/gic/test_vectors.S`, which saves registers and calls `handle_test_gic_el2_irq`.
6. `handle_test_gic_el2_irq` calls `Gic::AckIrq`, which reads GICC_IAR and returns the raw value and the 10 bit id. An id of 1023 is spurious and is not injected.
7. `Gic::InjectIrq(virtId, id)`: reads GICH_ELSR0 and ELSR1, returns -1 if any list register already holds `virtId`, picks the first empty list register, and writes `virtId | PENDING | HW | (id << 10)`. It returns 0 on success.
8. `Gic::EndIrq(ack)` writes the raw IAR value to GICC_EOIR. EOImode is on, so this only drops the running priority.
9. `runEndToEnd` checks `Gic::HasPendingIrq` and the ELSR bit for slot 0, so a pending list register is visible before the guest runs.
10. `Vcpu::Run` enters the guest payload `tests/integration/gic/guest_payload.S`. The test passes the GICV base in x0 and a result buffer in x1.
11. The guest takes the virtual IRQ at its own vector, reads GICV IAR (offset 0x00C) and writes the value back to GICV EOIR (offset 0x010), then issues `hvc #0`.
12. Because the list register carried the hardware bit, the guest's EOI deactivates the physical interrupt. The test reads ISACTIVER to confirm it is inactive, and reads the list register state to confirm nothing is pending and slot 0 is free again.
13. Cleanup: `Gic::DisableIrq`, clear the SPI, `Gic::CpuReset` (list registers, APR and VMCR to zero) and restore HCR_EL2.
