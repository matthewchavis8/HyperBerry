/**
 * @file exceptions.cpp
 * @brief C++ exception handler stubs for EL2 and lower-EL exceptions.
 * @ingroup exceptions
 *
 * Each handler receives an ExceptionContext reference built by the
 * assembly entry points in exceptions.S.  All handlers are currently
 * stub implementations that print a diagnostic message and enter an
 * infinite @c wfe loop.
 *
 * @note Uses C linkage (@c extern "C") so the assembly code can
 *       branch to the handlers by unmangled symbol name.
 */

#include "exceptions.h"
#include "lib/registerDump.h"
#include "uart.h"

// Hypervisor (EL2) exception handlers

/**
 * @brief Handle a synchronous exception taken at EL2.
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Calls registerDump() to print full CPU state, then spins.
 */
extern "C" void handle_el2_sync(ExceptionContext& ex) {
  Uart::print("[handle_el2_sync] called\n");
  registerDump(ex);

  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Handle an IRQ taken at EL2.
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 */
extern "C" void handle_el2_irq(ExceptionContext& ex) {
  Uart::print("[handle_el2_irq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Handle an FIQ taken at EL2.
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 */
extern "C" void handle_el2_fiq(ExceptionContext& ex) {
  Uart::print("[handle_el2_fiq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Handle an SError taken at EL2.
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 */
extern "C" void handle_el2_serror(ExceptionContext& ex) {
  Uart::print("[handle_el2_serror] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

// Lower EL (guest) exception handlers

/**
 * @brief Handle a synchronous exception from a lower EL (guest).
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 *       Future phases will decode ESR_EL2 to handle HVC, SMC,
 *       and Stage-2 faults.
 */
extern "C" void handle_lower_el_sync(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_sync] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Handle an IRQ from a lower EL (guest).
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 */
extern "C" void handle_lower_el_irq(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_irq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Handle an FIQ from a lower EL (guest).
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 */
extern "C" void handle_lower_el_fiq(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_fiq] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Handle an SError from a lower EL (guest).
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @note Stub -- prints a message and enters a @c wfe spin loop.
 */
extern "C" void handle_lower_el_serror(ExceptionContext& ex) {
  Uart::print("[handle_lower_el_serror] called\n");
  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Catch-all for exceptions with no dedicated handler.
 *
 * @param ex  Saved register context from the exception frame.
 *
 * @warning This handler never returns. Called from the
 *          @c el2_unhandled assembly entry point, which itself
 *          spins after this function returns.
 */
extern "C" void handle_unhandled(ExceptionContext& ex) {
  Uart::print("[handle_unhandled called]\n");
  for (;;) {
    asm volatile("wfe");
  }
}
