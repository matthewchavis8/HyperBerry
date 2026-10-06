---
type: Decision
title: Drivers and allocators are singletons
description: Why the Uart, Gic, Pmm, Heap and HostMmu are reached through GetInstance and set up by their constructors.
resource: ../../drivers/
tags: [decision, drivers, style]
status: unverified
decided: 2026-09-25
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../docs/guides/STYLES.md
  - resource: ../../drivers/uart/uart.h
  - resource: ../../drivers/gic/gic.h
  - resource: ../../core/mm/pmm/pmm.h
  - resource: ../../core/mm/heap/heap.h
  - resource: ../../core/mm/mmu/hostMmu/hostMmu.h
  - resource: ../../drivers/timer/timer.h
---

# The decision

One instance of a device or allocator is a function local static returned by `GetInstance()`. `Uart`, `Gic`, `Pmm`, `Heap` and `HostMmu` follow it. Each has a private constructor, and the constructor does the setup that an `Init` function did before. The style rules are in [STYLES.md](../guides/STYLES.md).

# Why

- The static is built on first call and not at static init time, so it works before the heap exists.
- There is one physical PL011 and one physical GIC. A private constructor means no code can build a second object that drives the same registers, and STYLES.md names the copy `Uart u = Uart::GetInstance();` as the case to stop.
- Setting up in the constructor means a caller cannot forget `Init`. After commit c313223 `main` no longer brings the GIC up by hand, and `Heap` comes up on the first `new`.
- The `Uart` came first. The commits of 2026-09-25 moved the PMM (d9329c3), the heap (c54aff7), `HostMmu` (c6911ac) and the GIC (c313223) to the same pattern, and the messages for the PMM and the GIC say they follow the `Uart` pattern.
- Where the hardware value is learned later, a setter repoints the instance: `Uart::SetBase`, `Gic::SetBases` and `Pmm::SetMemoryMap`.

# Consequences

- Two rules come with it, from STYLES.md. The type should be trivially destructible, and a constructor must never reach back through its own accessor, because `-fno-threadsafe-statics` removes the guard that would catch the recursion.
- Tests reach these through `GetInstance`, and the integration tests call `Gic::Reset` to re arm the controller.
- `Timer` is not one of them. It has a public constructor that reads `CNTFRQ_EL0` and leaves the timer disarmed, so it does not have `GetInstance`.
