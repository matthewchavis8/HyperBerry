/**
 * @file registerDump.h
 * @brief Human-readable register dump for exception diagnostics.
 * @ingroup lib
 */

#ifndef __REGISTERDUMP_H__
#define __REGISTERDUMP_H__

#include "core/exceptions/exceptions.h"
#include "drivers/uart/uart.h"

#include "stddef.h"

#ifdef __cplusplus

/**
 * @brief Print a full register dump to the UART console.
 * @ingroup lib
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

  Uart::println("==========[EXCEPTION DUMP]============");
  // ESR
  Uart::print("ESR_EL2: 0x");
  Uart::writeHex(esr);
  Uart::putc('\n');

  Uart::print("EC:      0x");
  Uart::writeHex(static_cast<uint64_t>(ec));
  Uart::putc('\n');

  Uart::print("ISS:     0x");
  Uart::writeHex(static_cast<uint64_t>(iss));
  Uart::putc('\n');

  // System Registers
  Uart::print("ELR_EL2: 0x");
  Uart::writeHex(ctx.elr);
  Uart::putc('\n');

  Uart::print("SPSR:    0x");
  Uart::writeHex(ctx.spsr);
  Uart::putc('\n');

  Uart::print("FAR_EL2: 0x");
  Uart::writeHex(far);
  Uart::putc('\n');

  for (size_t i{}; i < 31; i++) {
    Uart::putc('x');
    if (i >= 10) {
      Uart::putc('0' + static_cast<char>(i / 10));
    }
    Uart::putc('0' + static_cast<char>(i % 10));
    Uart::print(i < 10 ? ":  0x" : ": 0x");
    Uart::writeHex(ctx.gpr[i]);
    Uart::putc('\n');
  }
  Uart::println("======================================");
}

#endif // __cplusplus
#endif // !__REGISTERDUMP_H__

