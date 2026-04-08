#include "panic.h"
#include "drivers/uart/uart.h"
#include "lib/registerDump/registerDump.h"
#include "core/exceptions/exceptions.h"

[[noreturn]] void hv_panic(const char *msg, ExceptionContext& ctx) {
  Uart::putc('\n');
  Uart::println("=======================================");
  Uart::println("=             HV PANIC                =");
  Uart::println("=======================================");
  Uart::putc('\n');

  if (msg) {
    Uart::print("[ERROR]");
    Uart::println(msg);
  }

  registerDump(ctx);

  for (;;) {
    asm volatile("wfe");
  }
}
