# Decisions

In the order they were made. Each page says what was decided, why, and what it means for the code.

* [There is no board macro](no-board-macro.md) - 2026-09-10. Why no code tests which board it is built for, and how the include path picks the board instead.
* [Layering is enforced by the link](layering-by-link.md) - 2026-09-10. Why the code is five libraries in a fixed order, and why CMake link lines state the order.
* [The device tree is the source of truth for addresses](device-tree-is-source-of-truth.md) - 2026-09-12. Why no address a device tree declares is written by hand, and the two paths that carry tree values into the code.
* [Drivers and allocators are singletons](singleton-drivers.md) - 2026-09-25. Why the Uart, Gic, Pmm, Heap and HostMmu are reached through GetInstance and set up by their constructors.
* [A VM is described by a VmConfig](vmconfig.md) - 2026-09-25. Why Vm takes one VmConfig struct in place of a list of positional arguments.
* [Assembly offsets come from the C++ layout](offsets-from-layout.md) - 2026-09-25. Why vcpu.S gets its struct offsets from the compiler through a generated header.
