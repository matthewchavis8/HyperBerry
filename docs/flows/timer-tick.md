---
type: Flow
title: Timer tick
description: How the EL2 physical timer is armed, fires as GIC interrupt 26, and calls its callback, as the timer integration suite runs it.
resource: ../../drivers/timer/
tags: [timer, interrupts]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../drivers/timer/timer.h
  - resource: ../../drivers/timer/timer.cpp
  - resource: ../../drivers/gic/gic.cpp
  - resource: ../../tests/integration/timer/test_timer.cpp
  - resource: ../../tests/integration/timer/test_vectors.S
  - resource: ../../core/vmm/exceptions/exceptions.cpp
---

# Summary

The EL2 physical timer counts down from an interval, raises the private peripheral interrupt `Timer::IRQ` (26)
through the GIC when it reaches zero, and the handler forwards that interrupt to `Timer::HandleIrq`, which reloads the
counter and calls the registered callback. Only `tests/integration/timer/` does this. No production code constructs a
`Timer`, and the production `handle_el2_irq` panics, so the hypervisor has no timer tick today. See [drivers](../subsystems/drivers.md).

# Steps

1. `drivers/timer/timer.cpp`, `Timer::Timer`: reads CNTFRQ_EL0 into the frequency, sets the interval to that many ticks (one second), reads CNTVCT_EL0 as the last arm time, and calls `Timer::Stop`.
2. `tests/integration/timer/test_timer.cpp`, `test_physical_timer_irq_invokes_callback`: calls `Timer::SetIntervalTicks` (one millisecond) and `Timer::SetCallback` with `timerCallback` and a context value.
3. The test calls `Gic::Reset`, then `configureTimerPpiGroup0` clears the PPI's bit in IGROUPR, `enableDistributorGroups` sets bits 0 and 1 of the distributor control register, `Gic::SetPriorityLevel(Timer::IRQ, 0x80)` and `Gic::EnableIrq(Timer::IRQ)` follow. The order matters because `Reset` disables every interrupt and puts them in Group 1.
4. `installTestVbar` points VBAR_EL2 at `test_timer_vectors`, `routePhysicalInterruptsToEl2` sets HCR_EL2.IMO and FMO.
5. `Timer::Start`: records the arm time, writes CNTHP_TVAL_EL2 with the interval and writes CNTHP_CTL_EL2 with ENABLE set and IMASK clear, followed by an `isb`.
6. The test unmasks IRQ and FIQ in DAIF and waits in `waitForTimerCallback`, which loops on `wfe`.
7. When the counter reaches zero the interrupt is taken at the current EL IRQ slot (or the FIQ slot, both go to `test_timer_el2_irq` in `tests/integration/timer/test_vectors.S`). That code saves the registers and calls `handle_test_timer_el2_irq`.
8. `handle_test_timer_el2_irq` calls `Gic::AckIrq`. If the id equals `Timer::IRQ` and a timer is active, it calls `Timer::HandleIrq`.
9. `Timer::HandleIrq`: refreshes the arm time, writes CNTHP_TVAL_EL2 again so the timer reloads, and calls the callback with its context, which was set earlier.
10. The handler calls `Gic::EndIrq` with the acknowledgement.
11. Cleanup: DAIF and HCR_EL2 are restored, `Timer::Stop` writes CNTHP_CTL_EL2 to 0, VBAR_EL2 is restored and `Gic::DisableIrq(Timer::IRQ)` runs.
