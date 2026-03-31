API Reference
=============

Core
----

The hypervisor entry point and boot-time initialization logic.

.. doxygengroup:: core
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
