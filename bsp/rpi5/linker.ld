/**
 * @file linker.ld
 * @brief Linker script for Raspberry Pi 5 hardware (8 GB).
 *
 * Memory layout: firmware loads kernel8.img to 0x00200000.
 * Defines text, rodata, data, bss, stack (4 MB), and uncached (1 MB)
 * regions. First 32 KB reserved.
 */
OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY
{
  /*
   * Pi firmware (TF-A BL33 / bare-metal) loads kernel8.img to 0x00080000.
   * AArch64 TEXT_OFFSET = 0x80000; RAM base = 0x00000000 on BCM2712.
   * Physical memory size: 8GB
   */
  RESERVED (r)  : ORIGIN = 0x00000000, LENGTH = 512K
  RAM      (xrw): ORIGIN = 0x00080000, LENGTH = (0x200000000 - 0x00080000)
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
