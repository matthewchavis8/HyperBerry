#include "exceptions.h"
#include "lib/registerDump.h"
#include "uart.h"

// Hypervisor (EL2) exception handlers

extern "C" void handle_el2_sync(ExceptionContext& ex) {
  Uart::print("[handle_el2_sync] called\n");
  registerDump(ex);

  for (;;) {
    asm volatile("wfe");
  }
}

extern "C" void handle_el2_irq(ExceptionContext& ex) {
  Uart::print("[handle_el2_irq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

extern "C" void handle_el2_fiq(ExceptionContext& ex) {
  Uart::print("[handle_el2_fiq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

extern "C" void handle_el2_serror(ExceptionContext& ex) {
  Uart::print("[handle_el2_serror] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

// Lower EL (guest) exception handlers
extern "C" void handle_lower_el_sync(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_sync] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

extern "C" void handle_lower_el_irq(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_irq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

extern "C" void handle_lower_el_fiq(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_fiq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

extern "C" void handle_lower_el_serror(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_serror] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

// Catch-all for unhandled exceptions
extern "C" void handle_unhandled(ExceptionContext& ex) {
  Uart::print("[handle_unhandled called]\n");
  for (;;) {
    asm volatile("wfe");
  }
}
