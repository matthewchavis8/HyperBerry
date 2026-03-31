/**
 * @file registerDump.h
 * @brief Human-readable register dump for exception diagnostics.
 * @ingroup exceptions
 */

#ifndef __REGISTERDUMP_H__
#define __REGISTERDUMP_H__

#include "core/exceptions/exceptions.h"
#include "drivers/uart/uart.h"

#include "stddef.h"

#ifdef __cplusplus

/**
 * @brief Print a full register dump to the UART console.
 * @ingroup exceptions
 *
 * Reads ESR_EL2 and FAR_EL2, decodes the Exception Class (EC) and
 * Instruction-Specific Syndrome (ISS) fields, then prints all 31
 * general-purpose registers plus the saved ELR and SPSR from the
 * exception context.
 *
 * @param ctx  Reference to the saved exception context populated by
 *             the assembly @c save_context macro.
 *
 * @note Output goes directly to the PL011 UART via Uart::print()
 *       and Uart::writeHex().  Intended for fatal-exception debugging
 *       only -- not suitable for production logging.
 */
inline void registerDump(ExceptionContext& ctx) {
  uint64_t esr {};
  uint64_t far {};

  asm volatile("mrs %0, esr_el2" : "=r"(esr));
  asm volatile("mrs %0, far_el2" : "=r"(far));

  uint32_t ec  = (esr >> 26) & 0x3F;
  uint32_t iss = esr & 0xFFFFFF;

  Uart::print("\n==========[EXCEPTION DUMP]============\n");
  // ESR
  Uart::print("ESR_EL2: 0x");
  Uart::writeHex(esr);
  Uart::print("\n");

  Uart::print("EC:      0x");
  Uart::writeHex(static_cast<uint64_t>(ec));
  Uart::print("\n");

  Uart::print("ISS:     0x");
  Uart::writeHex(static_cast<uint64_t>(iss));
  Uart::print("\n");

  // System Registers
  Uart::print("ELR_EL2: 0x");
  Uart::writeHex(ctx.elr);
  Uart::print("\n");

  Uart::print("SPSR:    0x");
  Uart::writeHex(ctx.spsr);
  Uart::print("\n");

  Uart::print("FAR_EL2: 0x");
  Uart::writeHex(far);
  Uart::print("\n");

  for (size_t i{}; i < 30; i++) {
    Uart::putc('x');
    if (i >= 10) {
      Uart::putc('0' + static_cast<char>(i / 10));
    }
    Uart::putc('0' + static_cast<char>(i % 10));
    Uart::print(i < 10 ? ":  0x" : ": 0x");
    Uart::writeHex(ctx.gpr[i]);
    Uart::print("\n");
  }

  Uart::print("x30: 0x");
  Uart::writeHex(ctx.lr);
  Uart::print("\n");
  Uart::print("======================================\n");
}

#endif // __cplusplus
#endif // !__REGISTERDUMP_H__


