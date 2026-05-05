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

namespace {
constexpr uint64_t PSCI_VERSION      = 0x84000000ULL;
constexpr uint64_t PSCI_SYSTEM_OFF   = 0x84000008ULL;
constexpr uint64_t PSCI_SYSTEM_RESET = 0x84000009ULL;
constexpr uint64_t PSCI_FEATURES     = 0x8400000AULL;

constexpr uint64_t PSCI_VERSION_1_0 = 0x00010000ULL;
constexpr uint64_t PSCI_SUCCESS     = 0ULL;
constexpr uint64_t PSCI_NOT_SUPPORTED = static_cast<uint64_t>(-1);

bool isSupportedPsciCall(uint64_t functionId) {
  return functionId == PSCI_VERSION
      || functionId == PSCI_SYSTEM_OFF
      || functionId == PSCI_SYSTEM_RESET
      || functionId == PSCI_FEATURES;
}

void handlePsciHvc(Vcpu* vcpu) {
  uint64_t functionId = vcpu->getGpReg(VCPU_GPREG_X0);

  switch (functionId) {
    case PSCI_VERSION:
      Uart::println("[Guest][PSCI] VERSION");
      vcpu->setGpReg(VCPU_GPREG_X0, PSCI_VERSION_1_0);
      break;

    case PSCI_FEATURES: {
      uint64_t queriedFunction = vcpu->getGpReg(VCPU_GPREG_X1);
      Uart::println("[Guest][PSCI] FEATURES function={:x}", queriedFunction);
      vcpu->setGpReg(VCPU_GPREG_X0,
                     isSupportedPsciCall(queriedFunction) ? PSCI_SUCCESS
                                                          : PSCI_NOT_SUPPORTED);
      break;
    }

    case PSCI_SYSTEM_OFF:
      Uart::println("[Guest][PSCI] SYSTEM_OFF");
      for (;;) asm volatile("wfe");

    case PSCI_SYSTEM_RESET:
      Uart::println("[Guest][PSCI] SYSTEM_RESET");
      for (;;) asm volatile("wfe");

    default:
      Uart::println("[Guest][PSCI] Unsupported HVC call ID={:x}", functionId);
      vcpu->setGpReg(VCPU_GPREG_X0, PSCI_NOT_SUPPORTED);
      break;
  }

  vcpu->skipInstruction();
  vcpu_enter(vcpu);
}
}

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

  switch (exceptionClass) {

    case EsrEc::HvcAarch64:
      Uart::println("[Guest][HVC] Handling HVC call from guest, call ID={:x}",
                    vcpu->getGpReg(VCPU_GPREG_X0));
      handlePsciHvc(vcpu);
      hv_panic("[HvcAarch64]");

    case EsrEc::SmcAarch64:
      Uart::println("[Guest][SMC] Handling SMC call from guest, call ID={:x}",
                    vcpu->getGpReg(VCPU_GPREG_X0));
      vcpu->skipInstruction();
      break;

    case EsrEc::DataAbortLower: {
      hv_panic("[DataAbortLower] unhandled");
    }

    default:
      Uart::println("[Guest][ERROR] Unhandled guest exit EC={:x} ESR={:x}",
                    exceptionClass,
                    esr);
      break;
  }
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
