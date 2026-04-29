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
  hv_panic("[HV sync] was triggered", ctx);
}

extern "C" void handle_el2_irq(ExceptionContext& ctx) {
  hv_panic("[HV irq] was triggered", ctx);
}

extern "C" void handle_el2_fiq(ExceptionContext& ctx) {
  hv_panic("[HV fiq] was triggered", ctx);
}

extern "C" void handle_el2_serror(ExceptionContext& ctx) {
  hv_panic("[HV SError] was triggered", ctx);
}

extern "C" void handle_unhandled(ExceptionContext& ctx) {
  hv_panic("[HV mysterious exception?] was triggered", ctx);
}

// Guest Exception Handlers

extern "C" void handle_lower_el_sync(Vcpu* vcpu, uint64_t esr) {
  EsrEc exceptionClass = getEsrEc(esr);
  bool resumeGuest = true;

  switch (exceptionClass) {

    case EsrEc::HvcAarch64:
      Uart::println("[Guest][HVC] Handling HVC call from guest, call ID={:x}",
                    vcpu->getGpReg(VCPU_GPREG_X0));
      vcpu->skipInstruction();
      break;

    case EsrEc::SmcAarch64:
      Uart::println("[Guest][SMC] Handling SMC call from guest, call ID={:x}",
                    vcpu->getGpReg(VCPU_GPREG_X0));
      vcpu->skipInstruction();
      break;

    case EsrEc::DataAbortLower: {
      uint64_t far;
      uint64_t hpfar;
      asm volatile("mrs %0, far_el2" : "=r"(far));
      asm volatile("mrs %0, hpfar_el2" : "=r"(hpfar));

      uint64_t iss = esr & 0x01FFFFFFULL;
      uint64_t dfsc = iss & 0x3FULL;
      uint64_t faultIpa = ((hpfar & 0xFFFFFFFFF0ULL) << 8) | (far & 0xFFFULL);

      Uart::println("[Guest][DATA ABORT] ESR={:x} ISS={:x} DFSC={:x}",
                    esr, iss, dfsc);
      Uart::println("[Guest][DATA ABORT] ELR={:x} FAR={:x} HPFAR={:x}",
                    vcpu->getElr(), far, hpfar);
      Uart::println("[Guest][DATA ABORT] IPA={:x} WnR={} S1PTW={}",
                    faultIpa, (iss >> 6) & 1ULL, (iss >> 7) & 1ULL);
      resumeGuest = false;
      break;
    }

    default:
      Uart::println("[Guest][ERROR] Unhandled guest exit EC={:x} ESR={:x}",
                    exceptionClass,
                    esr);
      resumeGuest = false;
      break;
  }

  if (resumeGuest)
    vcpu_enter(vcpu);

  for (;;) { asm volatile("wfe"); }
}

extern "C" void handle_lower_el_irq(Vcpu* vcpu, uint64_t esr) {
  (void) esr; // Unused for now will add some debugging later
  vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_fiq(Vcpu* vcpu, uint64_t esr) {
  (void) esr;
  vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_serror(Vcpu* vcpu, uint64_t esr) {
  (void) vcpu;
  (void) esr;
  Uart::println("[Guest EL SError] was triggered");
  for (;;) { asm volatile("wfe"); }
}
