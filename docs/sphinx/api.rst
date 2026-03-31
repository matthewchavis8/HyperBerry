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

.. doxygengroup:: lib
   :project: HyperBerry
   :members:

`lib/array.h` API
~~~~~~~~~~~~~~~~~

`hv::array<T, N>` is a fixed-size container used as a freestanding
replacement for `std::array`.

Type aliases:

- `value_type`, `size_type`, `difference_type`
- `reference`, `const_reference`
- `pointer`, `const_pointer`
- `iterator`, `const_iterator`

Core methods:

- `operator[](size_type i)` and `operator[](size_type i) const`:
  direct element access by index.
- `front()` / `front() const`:
  first element reference.
- `back()` / `back() const`:
  last element reference.
- `data()` / `data() const`:
  raw pointer to contiguous storage.
- `size()` / `max_size()` / `empty()`:
  capacity and emptiness queries.
- `begin()` / `end()` and const variants (`cbegin()`, `cend()`):
  iterator access.
- `fill(const T& value)`:
  assign one value to all elements.

.. doxygenfile:: lib/array.h
   :project: HyperBerry

`lib/registerDump.h` API
~~~~~~~~~~~~~~~~~~~~~~~~

`registerDump(ExceptionContext& ctx)` prints a full exception-state dump
to UART, including:

- `ESR_EL2`, decoded `EC`, and `ISS`
- `ELR_EL2`, `SPSR`, and `FAR_EL2`
- General-purpose registers (`x0` through `x30`)

.. doxygenfile:: lib/registerDump.h
   :project: HyperBerry
