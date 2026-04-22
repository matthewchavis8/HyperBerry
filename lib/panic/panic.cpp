/**
 * @file panic.cpp
 * @brief Fatal hypervisor panic implementation.
 * @ingroup lib
 */

#include "panic.h"
#include "drivers/uart/uart.h"
#include "lib/registerDump/registerDump.h"
#include "core/exceptions/exceptions.h"

/**
 * @brief Print panic diagnostics and stop execution permanently.
 * @ingroup lib
 * @param msg Optional panic message to print.
 * @param ctx Saved exception context for diagnostic output.
 */
[[noreturn]] void hv_panic(const char *msg, ExceptionContext& ctx) {
  Uart::putc('\n');
  Uart::println("=======================================");
  Uart::println("=             HV PANIC                =");
  Uart::println("=======================================");
  Uart::putc('\n');

  if (msg) {
    Uart::println("[ERROR]{}", msg);
  }

  registerDump(ctx);

  for (;;) {
    asm volatile("wfe");
  }
}
