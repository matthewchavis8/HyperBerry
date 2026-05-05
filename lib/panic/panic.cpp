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
  Uart::println("=======================================");
  Uart::println("=             HV PANIC                =");
  Uart::println("=======================================");

  if (msg)
    Uart::println("[ERROR] {}", msg);

  registerDump(ctx);

  for (;;) {
    asm volatile("wfe");
  }
}

/**
 * @brief Print panic diagnostics and stop execution permanently.
 * @ingroup lib
 * @param msg Optional panic message to print.
 */
[[noreturn]] void hv_panic(const char *msg) {
  Uart::println("=======================================");
  Uart::println("=             HV PANIC                =");
  Uart::println("=======================================");

  if (msg)
    Uart::println("[ERROR] {}", msg);

  // Dump System Register State
  uint64_t esr   {};
  uint64_t elr   {};
  uint64_t far   {};
  uint64_t spsr  {};
  uint64_t hpfar {};
  uint64_t vttbr {};
  uint64_t vtcr  {};

  asm volatile("mrs %0, esr_el2"   : "=r"(esr));
  asm volatile("mrs %0, elr_el2"   : "=r"(elr));
  asm volatile("mrs %0, far_el2"   : "=r"(far));
  asm volatile("mrs %0, spsr_el2"  : "=r"(spsr));
  asm volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
  asm volatile("mrs %0, vttbr_el2" : "=r"(vttbr));
  asm volatile("mrs %0, vtcr_el2"  : "=r"(vtcr));

  uint64_t ec  = (esr >> 26) & 0x3F;
  uint64_t iss = esr & 0x1FFFFFF;
  // HPFAR_EL2[39:4] holds IPA[47:12] of the stage-2 fault.
  uint64_t fault_ipa = (hpfar & 0xFFFFFFFFF0ULL) << 8;

  Uart::println("[ESR_EL2]   {:x} EC={:x} ISS={:x}", esr, ec, iss);
  Uart::println("[ELR_EL2]   {:x}", elr);
  Uart::println("[FAR_EL2]   {:x}", far);
  Uart::println("[SPSR_EL2]  {:x}", spsr);
  Uart::println("[HPFAR_EL2] {:x} IPA={:x}", hpfar, fault_ipa);
  Uart::println("[VTTBR_EL2] {:x}", vttbr);
  Uart::println("[VTCR_EL2]  {:x}", vtcr);

  for (;;) {
    asm volatile("wfe");
  }
}
