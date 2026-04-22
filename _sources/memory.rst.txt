Memory Management
=================

Overview
--------

HyperBerry now has an early-boot physical page allocator implemented with the
buddy allocation algorithm. It is responsible for turning the RAM range
reported by the firmware DTB into a pool of physically contiguous blocks that
later EL2 subsystems can allocate and free.

The allocator lives in ``core/mm/pmm/`` and is exposed through the ``pmm``
namespace (``pmm::init``, ``pmm::allocPages``, ``pmm::freePages``). Boot code
reaches it from ``hmain()`` after the DTB has been parsed successfully.

Boot Flow
---------

The allocator is brought up in this order:

1. ``boot.S`` transfers control to ``hmain()`` at EL2.
2. ``parseDtb()`` decodes the firmware DTB and returns a ``MemoryMap`` with:
   ``memBase``, ``memSize``, ``atfBase``, ``atfSize``, ``dtbBase``, and
   ``dtbSize``.
3. ``pmm::init(memoryMap)`` seeds the free lists from the RAM region.
4. The allocator reserves memory already occupied by:

   - the hypervisor image
   - TF-A / secure-monitor reserved memory
   - the DTB blob itself

5. Remaining RAM is left available for future EL2 subsystems such as page
   tables, guest images, and per-VM state.

Allocator Model
---------------

HyperBerry uses 4 KiB pages and supports orders ``0`` through ``11``:

- order 0 = 1 page = 4 KiB
- order 1 = 2 pages = 8 KiB
- order 11 = 2048 pages = 8 MiB

Each free block is stored in a per-order intrusive singly linked list. The
``FreeNode`` metadata lives inside the free page itself, so the allocator does
not need a separate heap or side allocation for bookkeeping.

A bitmap tracks buddy-pair state across all orders. On allocation and free,
the corresponding bit is toggled. During free, a zero bit means both buddies
are now free and the allocator attempts to merge them into the next higher
order.

Allocation Path
---------------

``allocPages(order)`` works as follows:

1. Find the lowest free list at or above the requested order.
2. Remove one block from that list.
3. Split it repeatedly until the requested order is reached.
4. Return the physical base address of the left-most block.

This gives the caller a physically contiguous region of ``PAGE_SIZE * 2^order``
bytes.

Free Path
---------

``freePages(addr, order)`` starts with the returned block and checks whether
its buddy is also free:

1. Compute the buddy address by XORing the block offset with the block size.
2. Consult the bitmap state for that order.
3. If the buddy is free and present in the free list, remove it and merge.
4. Repeat at the next higher order until no more merging is possible.

The final merged block is pushed back onto the appropriate free list.

Reservations
------------

During ``init()``, HyperBerry first seeds the allocator with the full RAM range
reported by the DTB, then removes regions that must never be handed out:

- the hypervisor image from ``__text_start`` through ``__uncached_space_end``
- the TF-A reserved region reported in the DTB
- the DTB blob itself

That reservation pass is what makes the allocator safe to use before an MMU or
virtual-memory subsystem exists.

Current Scope
-------------

What exists today:

- DTB-backed RAM discovery
- early boot allocator initialisation
- physically contiguous allocation by order
- free with buddy coalescing
- shared page-table helpers for 4 KiB tables and 2 MiB block mappings
- EL2 host stage-1 MMU bring-up for hypervisor DRAM and peripheral space
- per-VM stage-2 MMU setup with VMID-tagged VTTBR_EL2 contexts
- UART dump of free-list state for bring-up debugging

What does not exist yet:

- higher-level allocators built on top of the page allocator
- guest RAM loading and ownership management beyond simple identity maps
- fine-grained stage-2 permissions and richer guest memory layouts
- long-running multi-vCPU scheduling and reclamation paths

Source Locations
----------------

- ``core/dtb/``: boot-time DTB parsing and ``MemoryMap``
- ``core/mm/pmm/``: buddy allocator implementation
- ``core/mm/pageTable/``: shared page-table walk/allocation helpers
- ``core/mm/mmu/hostMmu/``: EL2 stage-1 MMU implementation
- ``core/mm/mmu/guestMmu/``: guest stage-2 MMU implementation
- ``core/vm/``: VM container owning stage-2 state and vCPU bring-up
- ``core/vcpu/``: guest CPU context save/restore and first-entry state
- ``core/main.cpp``: allocator bring-up during EL2 boot
