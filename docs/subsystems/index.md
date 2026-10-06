# Subsystems

One page per unit of code, lowest layer first where the layers apply. Each page says what the unit is, what it
depends on, what uses it, and the gotchas.

* [Lib](lib.md) - The bottom layer, holding console logging, typed MMIO access, the fatal panic path, freestanding memory helpers, the CPIO reader, global constructor support and the register dump.
* [Drivers](drivers.md) - The PL011 UART console, the GICv2 interrupt controller with its virtual interface, and the EL2 physical timer.
* [Mm](mm.md) - The buddy page allocator, the heap behind new and delete, the shared page table walk, the EL2 stage 1 map and the per guest stage 2 map.
* [Device tree](device-tree.md) - The flattened device tree parser that finds RAM, the guest archive and the MMIO windows of the host and guest.
* [Boot loader](boot-loader.md) - Loads the Linux kernel, guest tree and initrd out of the firmware archive into fresh guest RAM and patches the guest tree, plus the hmain entry that drives it.
* [Vcpu](vcpu.md) - One guest virtual CPU, its saved registers, and the assembly that enters the guest and returns on the next exception.
* [Vm](vm.md) - One guest, its stage 2 address space, its virtual CPU and its lifecycle state, configured by VmConfig.
* [Vmm](vmm.md) - The monitor that runs a Vm, decodes each guest exit and answers HVC and SMC calls, plus the fatal EL2 exception handlers.
* [Boards (bsp)](bsp.md) - What each board folder owns, the boot assembly and linker script every image starts from, the device trees and Pi firmware, and the shared guest tree.
* [Scripts](scripts.md) - The build time tools under scripts/, including bspgen, which turns a host device tree into regs.inc.
