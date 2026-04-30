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

.. doxygengroup:: mm
   :project: HyperBerry
   :members:

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
The public API currently consists of:

- ``Vm::init(ipaBase, sizeBytes, vmid, guestEntry)`` to build stage-2
  mappings and seed first-entry guest CPU state.
- ``Vm::run()`` to commit the VM's stage-2 context and enter the guest.

.. doxygengroup:: vm
   :project: HyperBerry
   :members:

.. doxygenclass:: Vm
   :project: HyperBerry
   :members:

Drivers
-------

UART Driver
~~~~~~~~~~~

PL011 UART driver for early debug output. Supports QEMU virt and
physical Raspberry Pi 5 targets via a compile-time base address.

.. doxygengroup:: drivers_uart
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

Compile-time board constants selected by the active target. This is a
top-level hardware description group, not part of the freestanding helper
library.

Selection Wrapper
~~~~~~~~~~~~~~~~~

``bsp.h`` picks exactly one board definition based on the build:
``BSP_QEMU`` or ``BSP_RPI5``.

.. doxygenfile:: bsp.h
   :project: HyperBerry
   :sections: define

QEMU virt
~~~~~~~~~

Base addresses exposed today:

- ``b::UartBase = 0x09000000``
- ``b::HvMmioBase = 0x08000000``
- ``b::HvMmioSize = 0x38000000``

.. doxygenfile:: qemu.h
   :project: HyperBerry
   :sections: var

Raspberry Pi 5
~~~~~~~~~~~~~~

Base addresses exposed today:

- ``b::UartBase = 0x107D001000``
- ``b::HvMmioBase = 0x107D000000``
- ``b::HvMmioSize = 0x200000000``

.. doxygenfile:: rpi5.h
   :project: HyperBerry
   :sections: var
