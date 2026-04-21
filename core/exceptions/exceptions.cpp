/**
 * @file exceptions.cpp
 * @brief C++ exception handlers for EL2 and lower-EL (guest) exits.
 * @ingroup exceptions
 *
 * EL2 handlers receive an ExceptionContext built by exceptions.S.
 * Lower-EL handlers receive the Vcpu* parked in TPIDR_EL2 and the
 * ESR_EL2 value passed through by vcpu.S, then re-enter the guest
 * via vcpu_enter().
 *
 * @note Uses C linkage (@c extern "C") so assembly can branch to
 *       handlers by unmangled symbol name.
 */

#include "exceptions.h"
#include "core/vcpu/vcpu.h"
#include "lib/panic/panic.h"
#include "uart.h"

// Hypervisor EL2 exception handlers

extern "C" void handle_el2_sync(ExceptionContext& ctx) {
  hv_panic("[EL2 Synchronous exception] was triggered", ctx);
}

extern "C" void handle_el2_irq(ExceptionContext& ctx) {
  hv_panic("[EL2 irq exception] was triggered", ctx);
}

extern "C" void handle_el2_fiq(ExceptionContext& ctx) {
  hv_panic("[EL2 fiq exception] was triggered", ctx);
}

extern "C" void handle_el2_serror(ExceptionContext& ctx) {
  hv_panic("[EL2 SError exception] was triggered", ctx);
}

extern "C" void handle_unhandled(ExceptionContext& ctx) {
  hv_panic("[EL2 mysterious exception?] was triggered", ctx);
}

// Guest Exception Handlers

extern "C" void handle_lower_el_sync(Vcpu* vcpu, uint64_t esr) {
  uint64_t ec = static_cast<uint64_t>(esrEc(esr));

  switch (esrEc(esr)) {
    case EsrEc::HvcAarch64:
      Uart::print("HVC from guest, x0=");
      Uart::writeHex(vcpu->gpReg(VCPU_GPREG_X0));
      Uart::println("");
      vcpu->skipInstruction();
      break;
    case EsrEc::SmcAarch64:
      vcpu->skipInstruction();
      break;
    default:
      Uart::print("Unhandled guest exit EC=");
      Uart::writeHex(ec);
      Uart::print(" ESR=");
      Uart::writeHex(esr);
      Uart::println("");
      break;
  }

  vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_irq(Vcpu* vcpu, uint64_t) {
  vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_fiq(Vcpu* vcpu, uint64_t) {
  vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_serror(Vcpu* vcpu, uint64_t esr) {
  (void)vcpu;
  (void)esr;
  Uart::println("[Lower EL SError exception] was triggered");
  for (;;) { asm volatile("wfe"); }
}
