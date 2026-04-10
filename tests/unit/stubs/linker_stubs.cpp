/**
 * @file linker_stubs.cpp
 * @brief Provides linker symbols expected by buddy.cpp for unit tests.
 *
 * buddy.cpp declares:
 *   extern uint8_t __text_start[];
 *   extern uint8_t __uncached_space_end[];
 *
 * On the real target these come from the linker script. For the host
 * unit-test build we define them here via inline asm to guarantee
 * __uncached_space_end > __text_start (address ordering).
 *
 * The buddy allocator computes kernelSize = __uncached_space_end - __text_start.
 * With this stub the "kernel" is 16 bytes, rounded to one page. Since the
 * test pool lives on the heap (far from BSS), reserveRegion for the kernel
 * becomes a harmless no-op.
 */

__asm__(
    ".data\n"
    ".global __text_start\n"
    ".global __uncached_space_end\n"
    ".balign 16\n"
    "__text_start:\n"
    "    .space 16\n"
    "__uncached_space_end:\n"
    "    .space 1\n"
);
