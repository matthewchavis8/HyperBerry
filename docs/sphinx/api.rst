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

Exception Handling
~~~~~~~~~~~~~~~~~~

EL2 exception vector table, context save/restore, and handler stubs.

.. doxygengroup:: exceptions
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
