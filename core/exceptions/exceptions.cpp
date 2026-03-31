#include "exceptions.h"
#include "uart.h"

// Hypervisor (EL2) exception handlers
extern "C" void handle_el2_sync(ExceptionContext& ex) {}
extern "C" void handle_el2_irq(ExceptionContext& ex) {}
extern "C" void handle_el2_fiq(ExceptionContext& ex) {}
extern "C" void handle_el2_serror(ExceptionContext& ex) {}

// Lower EL (guest) exception handlers
extern "C" void handle_lower_el_sync(ExceptionContext& ex) {}
extern "C" void handle_lower_el_irq(ExceptionContext& ex) {}
extern "C" void handle_lower_el_fiq(ExceptionContext& ex) {}
extern "C" void handle_lower_el_serror(ExceptionContext& ex) {}

// Catch-all for unhandled exceptions
extern "C" void handle_unhandled(ExceptionContext& ex) {}
