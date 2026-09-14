/**
 * @file registerDump.h
 * @brief Human-readable register dump for exception diagnostics.
 * @ingroup lib
 */

#ifndef __REGISTERDUMP_H__
#define __REGISTERDUMP_H__

#include "core/exceptions/exceptions.h"
#include "lib/log/log.h"

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
 * @note Output goes directly to the PL011 UART via Log::write()
 *       and Log::writeHex().  Intended for fatal-exception debugging
 *       only -- not suitable for production logging.
 */
inline void registerDump(ExceptionContext& ctx) {
    uint64_t esr {};
    uint64_t far {};
    uint64_t elr {};
    uint64_t spsr {};

    asm volatile("mrs %0, esr_el2" : "=r"(esr));
    asm volatile("mrs %0, far_el2" : "=r"(far));
    asm volatile("mrs %0, elr_el2" : "=r"(elr));
    asm volatile("mrs %0, spsr_el2" : "=r"(spsr));

    uint32_t ec = (esr >> 26) & 0x3F;
    uint32_t iss = esr & 0xFFFFFF;

    Log::writeLine("==========[EXCEPTION DUMP]============");
    // ESR
    Log::write("ESR_EL2(Syndrome): 0x");
    Log::writeHex(esr);
    Log::writeCh('\r');
    Log::writeCh('\n');

    Log::write("EC(Class):         0x");
    Log::writeHex(static_cast<uint64_t>(ec));
    Log::writeCh('\r');
    Log::writeCh('\n');

    Log::write("ISS(Subclass):     0x");
    Log::writeHex(static_cast<uint64_t>(iss));
    Log::writeCh('\r');
    Log::writeCh('\n');

    // System Registers
    Log::write("ELR_EL2(Return):   0x");
    Log::writeHex(elr);
    Log::writeCh('\r');
    Log::writeCh('\n');

    Log::write("SPSR(Status):      0x");
    Log::writeHex(spsr);
    Log::writeCh('\r');
    Log::writeCh('\n');

    Log::write("FAR_EL2(Fault):    0x");
    Log::writeHex(far);
    Log::writeCh('\r');
    Log::writeCh('\n');

    for (size_t i {}; i < 31; i++) {
        Log::writeCh('x');
        if (i >= 10) {
            Log::writeCh('0' + static_cast<char>(i / 10));
        }
        Log::writeCh('0' + static_cast<char>(i % 10));
        Log::write(i < 10 ? ":  0x" : ": 0x");
        Log::writeHex(ctx[i]);
        Log::writeCh('\r');
        Log::writeCh('\n');
    }
    Log::writeLine("======================================");
}

#endif // __cplusplus
#endif // !__REGISTERDUMP_H__
