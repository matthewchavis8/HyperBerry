/**
 * @file linker.ld
 * @brief Linker script for QEMU virt machine (Cortex-A76, 4 GB).
 *
 * Memory layout: RAM starts at 0x40000000 (QEMU virt load address).
 * Defines text, rodata, data, bss, stack (4 MB), and uncached (1 MB)
 * regions.
 */
OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY
{
  /*
   * QEMU virt machine: RAM starts at 0x40000000 but the -kernel
   * loader applies the arm64 TEXT_OFFSET (0x80000), so the image
   * is placed at 0x40080000.  Physical memory size: 4 GB.
   */
  RAM (xrw): ORIGIN = 0x40080000, LENGTH = (0xC0000000 - 0x80000)
}

SECTIONS {
  __start = .;

  /* text region for Hypervisor */
  .text : ALIGN(0x1000) {
    __text_start = .;
    KEEP(*(.text.start))
    KEEP(*(.text.vectors))
    *(.text*)
    . = ALIGN(0x1000);
    __text_end = .;
  } > RAM
  __text_size = __text_end - __text_start;

  /* rodata region for Hypervisor */
  .rodata : ALIGN(0x1000) {
    __rodata_start = .;
    *(.rodata*)
    . = ALIGN(0x1000);
    __rodata_end = .;
  } > RAM
  __rodata_size = __rodata_end - __rodata_start;

  /* Integration test suite registration (linker auto-collect) */
  .hyperberry_tests : ALIGN(0x1000) {
    __test_suites_start = .;
    KEEP(*(.hyperberry_tests))
    __test_suites_end = .;
  } > RAM

  /* data region for Hypervisor */
  .data : ALIGN(0x1000) {
    __data_start = .;
    *(.data*)
    . = ALIGN(0x1000);
    __data_end = .;
  } > RAM
  __data_size = __data_end - __data_start;

  /* bss region for Hypervisor */
  .bss : ALIGN(0x1000) {
    __bss_start = .;
    *(.bss*)
    . = ALIGN(0x1000);
    __bss_end = .;
  } > RAM
  __bss_size = __bss_end - __bss_start;

  /* 4MB stack region for Hypervisor */
  .stack : ALIGN(0x1000) {
    __stack_start = .;
    __stack_end = __stack_start + (1024 * 1024) * 4;
  } > RAM
  __stack_size = __stack_end - __stack_start;

  /* 1MB uncached region for Hypervisor */
  .uncached_space : ALIGN(0x1000) {
    __uncached_space_start = .;
    . = . + (1024 * 1024);
    . = ALIGN(0x1000);
    __uncached_space_end = .;
  } > RAM
  __uncached_space_size = __uncached_space_end - __uncached_space_start;
}
