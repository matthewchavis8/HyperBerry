---
type: Subsystem
title: Drivers
description: The three hardware drivers, the PL011 UART console, the GICv2 interrupt controller with its virtual interface, and the EL2 physical timer.
resource: ../../drivers/
tags: [drivers, hardware]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../drivers/CMakeLists.txt
  - resource: ../../drivers/uart/uart.h
  - resource: ../../drivers/uart/uart.cpp
  - resource: ../../drivers/gic/gic.h
  - resource: ../../drivers/gic/gic.cpp
  - resource: ../../drivers/timer/timer.h
  - resource: ../../drivers/timer/timer.cpp
  - resource: ../../core/deviceTree/deviceTree.cpp
  - resource: ../../core/vmm/exceptions/exceptions.cpp
  - resource: ../../tests/integration/gic/test_gic.cpp
  - resource: ../../tests/integration/timer/test_timer.cpp
---

# What it is

The code that talks to hardware. `Uart` drives the PL011 console and is what `Log` and `HvPanic`
print through. `Gic` drives a GICv2 such as the GIC 400: the distributor, the physical CPU interface,
and the hypervisor control interface whose list registers inject virtual interrupts into a guest.
`Timer` wraps the EL2 physical timer (CNTHP). The CMake target is `Drivers${BSP_SUFFIX}`, a static
library of `uart.cpp`, `gic.cpp` and `timer.cpp` that links `Lib${BSP_SUFFIX}` as PUBLIC. Its public include paths are the
three driver folders. Register addresses come from the board's generated `regs.inc`, never from
source. See [device tree to regs.inc](../flows/device-tree-to-regs.md).

# Depends on

- [Lib](lib.md): `mmio.h` for every register access in `uart.cpp` and `gic.cpp`.
- The board's `regs.inc`, included by `uart.cpp` and `gic.cpp` for `BSP_UART_BASE` and the four `BSP_GIC_*_BASE` values.
- The timer uses system registers directly through inline assembly and needs nothing else.

# Used by

- `Uart`: `lib/log/log.cpp`, `lib/panic/panic.cpp` and `lib/registerDump/registerDump.h` print through it.
  `core/deviceTree/deviceTree.cpp` repoints it with `SetBase`.
- `Gic`: `core/deviceTree/deviceTree.cpp` calls `SetBases` from `TreeParser::GetHostMmio`. Nothing else outside
  the tests calls it. `tests/integration/gic/test_gic.cpp` and `tests/integration/timer/test_timer.cpp` exercise the rest.
- `Timer`: `tests/integration/timer/test_timer.cpp` only. No production file includes `timer.h`.
- Flows: [GIC interrupt](../flows/gic-interrupt.md) and [timer tick](../flows/timer-tick.md).

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `drivers/CMakeLists.txt` | Builds `Drivers${BSP_SUFFIX}` and exports the three folders as public include paths | (none) | `core/mm/CMakeLists.txt` links it |
| `drivers/uart/uart.h`, `drivers/uart/uart.cpp` | PL011 console: enable TX and RX, send one character | `Uart`, `UART_REG`, `Uart::GetInstance`, `Uart::SetBase`, `Uart::GetBase`, `Uart::Putc` | `lib/log/log.cpp`, `lib/panic/panic.cpp`, `lib/registerDump/registerDump.h`, `core/deviceTree/deviceTree.cpp` |
| `drivers/gic/gic.h`, `drivers/gic/gic.cpp` | GICv2 distributor, CPU interface and hypervisor interface; physical enable, acknowledge and end of interrupt; virtual injection through list registers | `Gic`, `Gic::IrqAck`, `GetInstance`, `SetBases`, `Reset`, `CpuReset`, `EnableIrq`, `DisableIrq`, `AckIrq`, `EndIrq`, `InjectIrq`, `HasPendingIrq`, `EnableMainIrq`, `SetPriorityLevel`, `GetDistBase`, `GetHvBase`, `GetVcpuBase` | `core/deviceTree/deviceTree.cpp`, `tests/integration/gic/test_gic.cpp`, `tests/integration/timer/test_timer.cpp` |
| `drivers/timer/timer.h`, `drivers/timer/timer.cpp` | EL2 physical timer: arm a down counter, reload it on each interrupt, call a callback | `Timer`, `Timer::IRQ`, `Start`, `Stop`, `HandleIrq`, `SetIntervalTicks`, `SetCallback`, `GetFrequency`, `GetRawCount`, `GetElapsedTicks` | `tests/integration/timer/test_timer.cpp` |

# Entry points

## uart

`Uart::GetInstance().Putc(ch)`. The first call constructs the console at `BSP_UART_BASE`, clears
all pending interrupts (ICR = 0x7FF) and enables the UART, TX and RX by writing 0x301 to CR. In a debug
build it then writes a banner straight through `Putc`. `Putc` spins while the TX FIFO full bit in FR
(bit 5) is set, then writes DR. `SetBase(base)` rebinds and reconfigures when the address differs; it
does nothing if the address is unchanged.

## gic

`Gic::GetInstance()` constructs the driver with the four `BSP_GIC_*_BASE` addresses and calls `Reset()`, which:

- disables the distributor, reads TYPER to learn how many 32 interrupt groups exist,
- puts every interrupt in Group 1, disables all of them and clears their pending and active state,
- sets every priority to 0x80, routes every SPI to CPU 0 (0x01010101 in ITARGETSR), makes every SPI level triggered,
- enables the distributor with GRPEN1 only, then runs `cpuInit`.

`cpuInit` reads GICH_VTR to learn the list register count, sets the CPU interface to Group 1 with EOImode on, unmasks
all priorities (PMR 0xFF), sets the binary point to 0, programs GICH_VMCR, enables the virtual interface through GICH_HCR, and zeroes every list register and APR.
Day to day calls are `EnableIrq(id)`, `DisableIrq(id)`, `SetPriorityLevel(id, prio)`, `AckIrq()` returning an `IrqAck`
(the raw IAR and the 10 bit id), `EndIrq(ack)`, `InjectIrq(virtId, physId)` and `HasPendingIrq()`.

## timer

Construct a `Timer`, call `SetIntervalTicks` and `SetCallback`, then `Start()`. The interrupt line is the constant
`Timer::IRQ` (26). The IRQ handler must call `HandleIrq()` when the acknowledged id equals it.

# Gotchas

- **`Timer` is not a singleton.** The glossary says the drivers each expose `GetInstance()`, but only `Uart` and
  `Gic` do. `Timer` has a public constructor and is copy and move deleted, and the tests simply make local objects.
- **Nothing in the shipping path services a physical interrupt at EL2.** `handle_el2_irq` in
  `core/vmm/exceptions/exceptions.cpp` calls `HvPanic`. A guest IRQ exit makes `Vmm::Run` loop again without touching the
  `Gic`. `AckIrq`, `EndIrq`, `InjectIrq`, `EnableIrq` and the whole of `Timer` are exercised only by the integration suites,
  which install their own vector tables.
- **EOImode is on and the driver never writes GICC_DIR.** `cpuInit` sets bit 9 of the CPU interface control register, so `EndIrq` only drops
  priority. Deactivation of an interrupt that is injected with `InjectIrq` happens when the guest signals its virtual
  EOI, because the list register is written with the hardware bit and the physical id. `GicReg::Cpu` has no DIR offset, so an
  interrupt that is not handed to a guest this way is never deactivated by this driver.
- **`InjectIrq` always sets the hardware bit.** The list register is written pending, hardware and with `physId`
  shifted to bit 10, so a virtual interrupt must be backed by a real physical one. It returns -1 both when no list
  register is free and when `virtId` already appears in any list register. An empty list register reads as id 0, so
  `virtId` 0 is always rejected as a duplicate.
- **`Reset()` undoes configuration.** It disables every interrupt, including the PPI bank, puts all of them in
  Group 1 and enables only Group 1 at the distributor. The timer test therefore calls `Reset()` first, then moves
  the timer PPI to Group 0, sets bits 0 and 1 of the distributor control register, and only then calls `EnableIrq`.
  The GIC test does the same for its SPI, with a comment that QEMU's security state leaves a Group 1 software pended
  SPI stuck. Enable interrupts after `Reset()`, not before.
- **`SetBases` resets the controller when any base changes.** `GetHostMmio` calls it after parsing the tree, so
  the GIC is reprogrammed a second time at that point. If the addresses already match, it returns early.
- **There is no `GetCpuBase`.** Only the distributor, hypervisor and virtual CPU bases have getters.
- **`CpuReset()` is not `Reset()`.** It clears the list registers, APR and VMCR (to zero) and leaves GICH_HCR
  enabled. Use `Reset()` to rebuild the full state.
- **`SetPriorityLevel` writes a single byte** straight to `IPRIORITYR + id` from the distributor base.
- **`Uart` programs no baud rate or line control.** `UART_REG` names IBRD, FBRD and LCRH, but `configure()`
  writes only ICR and CR, so the line settings are whatever firmware or QEMU left.
- **`Uart::Putc` busy waits** and the header warns against calling it from interrupt context. The
  debug banner in the constructor bypasses `Log` on purpose, because `Log` calls `GetInstance()` and the
  function local static guard is not set until the constructor returns.
- **The panic path uses `Uart` too.** If `SetBase` points at an address the MMU has not mapped, every later log
  line and panic faults. That is why `GetHostMmio` adds the console to the host MMIO map.
- **`Timer` timing details.** The constructor reads CNTFRQ_EL0, sets the default interval to one second (a value equal to the frequency)
  and disarms. `Start` and `HandleIrq` both write CNTHP_TVAL_EL2 with the interval, so the period is the interval and
  does not account for handler latency. `GetRawCount` and `GetElapsedTicks` read CNTVCT_EL0, the virtual count, while the header calls it the physical counter.
  Taking the interrupt also needs HCR_EL2 IMO and FMO set and DAIF unmasked, which only the test does.
