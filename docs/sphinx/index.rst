HyperBerry
==========

A Type-1 bare-metal hypervisor for the Raspberry Pi 5, written in
freestanding C++17 and AArch64 assembly. HyperBerry boots directly
into EL2 on the BCM2712 SoC and uses Armv8-A hardware-assisted
virtualization to host isolated guest operating systems.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   architecture
   memory
   api
