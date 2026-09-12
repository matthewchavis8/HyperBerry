API Reference
=============

Core
----

The hypervisor entry point and boot-time initialization logic.

.. doxygengroup:: core
   :project: HyperBerry
   :members:

Memory Management
-----------------

Boot-time physical memory discovery and contiguous page allocation.

PMM
~~~

Physical page allocator using the buddy allocation algorithm.

.. doxygengroup:: pmm
   :project: HyperBerry
   :members:

MMU
~~~

Host stage-1 and guest stage-2 translation interfaces.

.. doxygengroup:: mmu
   :project: HyperBerry
   :members:

Heap
~~~~

PMM-backed kernel heap lifecycle used by global C++ allocation.

.. doxygenfile:: heap.h
   :project: HyperBerry
   :sections: func

Page Tables
~~~~~~~~~~~

Shared page-table walk and allocation helpers used by both MMU paths.

.. doxygenfile:: pageTable.h
   :project: HyperBerry
   :sections: func innerclass define

Exception Handling
~~~~~~~~~~~~~~~~~~

EL2 exception vector table, context save/restore, and handler stubs.

.. doxygengroup:: exceptions
   :project: HyperBerry
   :members:

Hypercalls
~~~~~~~~~~

AArch64 HVC dispatch for lower-EL guest exits. The guest-provided SMCCC
function ID is read from ``x0``; supported standard-service calls are currently
handled as PSCI requests.

.. doxygenfile:: hvc.h
   :project: HyperBerry
   :sections: enum typedef func

SMCCC
~~~~~

Function-ID field definitions, Owner Entity Numbers, standard return codes, and
small helpers used by HVC dispatch.

.. doxygennamespace:: SMCCC
   :project: HyperBerry
   :members:

Virtualization
--------------

vCPU
~~~~

Guest CPU context, EL1 register save/restore helpers, and guest entry glue.

.. doxygengroup:: vcpu
   :project: HyperBerry
   :members:

VM
~~

Per-guest container that owns one stage-2 MMU context and one guest vCPU.

.. doxygenclass:: Vm
   :project: HyperBerry
   :members:

Drivers
-------

Uart
~~~~

PL011 UART driver for early debug output. Supports QEMU virt and
physical Raspberry Pi 5 targets via a compile-time base address.

.. doxygengroup:: drivers_uart
   :project: HyperBerry
   :members:

Gic
~~~

ARM GICv2 Distributor and CPU Interface driver.

.. doxygengroup:: gic
   :project: HyperBerry
   :members:

HV Library
----------

Freestanding C++ utility headers with no standard library dependency.

Panic
~~~~~

Fatal exception reporting for unrecoverable EL2 errors.

``hv_panic()`` prints an error message, emits a full exception register dump,
and halts the current CPU indefinitely.

.. doxygenfile:: panic.h
   :project: HyperBerry
   :sections: func

Memory Primitives
~~~~~~~~~~~~~~~~~

Freestanding C memory helpers used when no hosted libc is available,
including ``memcpy()`` and ``memset()``.

``memcpy()`` copies ``n`` bytes from ``src`` to ``dest`` and returns
``dest``.

``memset()`` fills ``n`` bytes at ``dest`` with the low byte of ``c`` and
returns ``dest``.

.. doxygenfile:: strings.h
   :project: HyperBerry
   :sections: func

Array
~~~~~

Freestanding fixed-size container used in exception context state.

.. doxygenstruct:: hv::array
   :project: HyperBerry
   :members:

registerDump()
~~~~~~~~~~~~~~

Prints a full exception-state register dump to UART for diagnostics.

.. doxygenfile:: registerDump.h
   :project: HyperBerry

BSP
---

Per-board hardware description. This is a top-level hardware group, not part of
the freestanding helper library.

Addresses come from a board's device tree, never from a literal in a header.
Two paths exist, and which one a value takes depends on when it is needed.

``tools/bspgen`` reads a board's host device tree at configure time and emits
``regs.inc``, a plain ``#define`` per address and size. It covers only what has
to be a compile time constant: the PL011 base, because the early console must
work before any tree has been parsed, and the four GIC bases the driver's
register tables are built from. ``verifyBspAgainstDtb`` rechecks every one of
them against the firmware tree at boot and panics on a mismatch, which is what
guards against a board's checked in blob drifting from its source.

Everything else is read from a tree at runtime. ``dtbHostMmio`` and
``dtbGuestMmio`` derive the device windows the two MMU layers map, so the EL2
self map covers exactly the peripherals the host tree declares and a guest
reaches exactly what its own tree declares.

``bsp/fvp/platform.inc`` is the exception that proves the rule: the load
addresses are where the model is *told* to place a blob, so no tree can state
them and they are written by hand.

There is no board macro. The build puts ``bsp/<board>`` and that board's
generated directory on the include path, so code includes ``"regs.inc"`` and
the board selects itself.

Generated Addresses
~~~~~~~~~~~~~~~~~~~

.. doxygenfile:: regs.inc
   :project: HyperBerry
   :sections: define

MMIO Windows
~~~~~~~~~~~~

.. doxygenfile:: dtbMmio.h
   :project: HyperBerry

.. doxygenfile:: mmioMap.h
   :project: HyperBerry
