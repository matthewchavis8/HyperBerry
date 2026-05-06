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

Compile-time board constants selected by the active target. This is a
top-level hardware description group, not part of the freestanding helper
library.

Selection Wrapper
~~~~~~~~~~~~~~~~~

``bsp.h`` picks exactly one board definition based on the build:
``BSP_QEMU``, ``BSP_RPI5``, or ``BSP_FVP``.

.. doxygenfile:: bsp.h
   :project: HyperBerry
   :sections: define

QEMU virt
~~~~~~~~~

Base addresses exposed today:

- ``b::UART_BASE = 0x09000000``
- ``b::GIC_DISTRIBUTOR_BASE = 0x08000000``
- ``b::GIC_CPU_BASE = 0x08010000``
- ``b::GIC_HV_BASE = 0x08030000``
- ``b::GIC_VCPU_BASE = 0x08040000``
- ``b::HV_MMIO_BASE = 0x08000000``
- ``b::HV_MMIO_SIZE = 0x38000000``

.. doxygenfile:: qemu.h
   :project: HyperBerry
   :sections: var

Raspberry Pi 5
~~~~~~~~~~~~~~

Base addresses exposed today:

- ``b::UART_BASE = 0x107D001000``
- ``b::GIC_DISTRIBUTOR_BASE = 0x107fff9000``
- ``b::GIC_CPU_BASE = 0x107fffa000``
- ``b::GIC_HV_BASE = 0x107fffc000``
- ``b::GIC_VCPU_BASE = 0x107fffe000``
- ``b::HV_MMIO_BASE = 0x107D000000``
- ``b::HV_MMIO_SIZE = 0x200000000``

.. doxygenfile:: rpi5.h
   :project: HyperBerry
   :sections: var

Arm FVP Base RevC
~~~~~~~~~~~~~~~~~

Base addresses exposed today:

- ``b::HOST_DTB_BASE = 0x88000000``
- ``b::UART_BASE = 0x1C090000``
- ``b::GIC_DISTRIBUTOR_BASE = 0x2F000000``
- ``b::GIC_CPU_BASE = 0x2C000000``
- ``b::GIC_HV_BASE = 0x2C010000``
- ``b::GIC_VCPU_BASE = 0x2C02F000``
- ``b::HV_MMIO_BASE = 0x2F000000``
- ``b::HV_MMIO_SIZE = 0x00200000``
- ``b::PLATFORM_MMIO_BASE = 0x1C000000``
- ``b::PLATFORM_MMIO_SIZE = 0x00200000``
- ``b::GIC_ITS_MMIO_BASE = 0x2F200000``
- ``b::GIC_ITS_MMIO_SIZE = 0x00200000``

.. doxygenfile:: fvp.h
   :project: HyperBerry
   :sections: var
