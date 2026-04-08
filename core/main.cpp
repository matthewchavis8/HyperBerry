/**
 * @defgroup core Core
 * @brief Hypervisor entry point and boot-time initialization.
 */

/**
 * @file main.cpp
 * @brief Hypervisor entry point.
 * @ingroup core
 *
 * Contains hmain(), the C++ entry called from boot.S after
 * EL2 initialization, BSS zeroing, and stack setup.
 */

#include "uart.h"
#include "stddef.h"

#ifdef INTEGRATION_TEST
#include "tests/integration/suite.h"
#endif

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
extern "C" void hmain(uintptr_t dtb) {
  Uart::init();

#ifdef INTEGRATION_TEST
  TestRunner::run_all();
#else
  Uart::println("[LOG] attempt to trigger el2_sync");

  asm volatile("brk #0");
#endif
}
