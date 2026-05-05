/**
 * @file linker.ld
 * @brief Linker script for Arm FVP Base RevC-2xAEMvA.
 *
 * Memory layout: DRAM0 starts at 0x80000000 (FVP Base RevC). FVP loads
 * ELF segments directly via -a, so no TEXT_OFFSET skew is applied.
 */
OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY
{
  RAM (xrw): ORIGIN = 0x80000000, LENGTH = 0x80000000
}

SECTIONS {
  __start = .;

  .text : ALIGN(0x1000) {
    __text_start = .;
    KEEP(*(.text.start))
    KEEP(*(.text.vectors))
    *(.text*)
    . = ALIGN(0x1000);
    __text_end = .;
  } > RAM
  __text_size = __text_end - __text_start;

  .rodata : ALIGN(0x1000) {
    __rodata_start = .;
    *(.rodata*)
    . = ALIGN(0x1000);
    __rodata_end = .;
  } > RAM
  __rodata_size = __rodata_end - __rodata_start;

  .hyperberry_tests : ALIGN(0x1000) {
    __test_suites_start = .;
    KEEP(*(.hyperberry_tests))
    __test_suites_end = .;
  } > RAM

  .data : ALIGN(0x1000) {
    __data_start = .;
    *(.data*)
    . = ALIGN(0x1000);
    __data_end = .;
  } > RAM
  __data_size = __data_end - __data_start;

  .bss : ALIGN(0x1000) {
    __bss_start = .;
    *(.bss*)
    . = ALIGN(0x1000);
    __bss_end = .;
  } > RAM
  __bss_size = __bss_end - __bss_start;

  .stack : ALIGN(0x1000) {
    __stack_start = .;
    __stack_end = __stack_start + (1024 * 1024) * 4;
  } > RAM
  __stack_size = __stack_end - __stack_start;

  .heap : ALIGN(0x1000) {
    __heap_start = .;
    . = . + (1024 * 1024) * 64;
    . = ALIGN(0x1000);
    __heap_end = .;
  } > RAM
  __heap_size = __heap_end - __heap_start;

  .uncached_space : ALIGN(0x1000) {
    __uncached_space_start = .;
    . = . + (1024 * 1024);
    . = ALIGN(0x1000);
    __uncached_space_end = .;
  } > RAM
  __uncached_space_size = __uncached_space_end - __uncached_space_start;
}
