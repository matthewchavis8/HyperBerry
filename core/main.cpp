/**
 * @file main.cpp
 * @brief Hypervisor entry point.
 * @ingroup core
 *
 * Contains hmain(), the C++ entry called from boot.S after
 * EL2 initialization, BSS zeroing, and stack setup.
 */

#include "uart.h"

/**
 * @brief Main hypervisor entry point (called from boot.S).
 * @ingroup core
 *
 * Uses C linkage so the assembly boot code can branch to it by name
 * without C++ name mangling.
 *
 * @warning Must never return. The assembly boot stub has no return
 *          address — falling off the end of hmain() is undefined behaviour.
 * @note Currently a Phase 0 boot stub. Future phases will replace the
 *       infinite loop with vCPU scheduling and Stage-2 MMU setup.
 */
extern "C" void hmain() {
  for (;;) {
    Uart::print("I have no mouth and I must scream\n");
    asm volatile("wfe");
  }
}
