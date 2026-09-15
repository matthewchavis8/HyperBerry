// @file registerDump.h
// @brief Human-readable register dump for exception diagnostics.
// @ingroup lib

#ifndef __REGISTERDUMP_H__
#define __REGISTERDUMP_H__

#include <stdint.h>
#include "lib/array/array.h"
#include "drivers/uart/uart.h"
#include "lib/log/log.h"

#include "stddef.h"

#ifdef __cplusplus

// @brief Print a full register dump to the UART console.
// @ingroup lib
//
// Reads ESR_EL2 and FAR_EL2, decodes the Exception Class (EC) and
// Instruction-Specific Syndrome (ISS) fields, then prints all 31
// general-purpose registers plus the saved ELR and SPSR from the
// exception context.
//
// @param ctx  Reference to the saved exception context populated by
//             the assembly @c save_context macro.
//
// @note Writes straight to the UART rather than through Log, so the dump
//       survives a release build. Only hv_panic calls this.
inline void RegisterDump(const hv::array<uint64_t, 31>& ctx) {
    uint64_t esr {};
    uint64_t far {};
    uint64_t elr {};
    uint64_t spsr {};

    asm volatile("mrs %0, esr_el2" : "=r"(esr));
    asm volatile("mrs %0, far_el2" : "=r"(far));
    asm volatile("mrs %0, elr_el2" : "=r"(elr));
    asm volatile("mrs %0, spsr_el2" : "=r"(spsr));

    uint32_t ec = (esr >> 26) & 0x3F;
    uint32_t iss = esr & 0x1FFFFFF;

    auto sink = [](char ch) { Uart::GetInstance().Putc(ch); };

    log::detail::FormatLineToSink(sink, "==========[EXCEPTION DUMP]============");
    log::detail::FormatLineToSink(sink, "ESR_EL2(Syndrome): {:x}", esr);
    log::detail::FormatLineToSink(sink, "EC(Class):         {:x}", ec);
    log::detail::FormatLineToSink(sink, "ISS(Subclass):     {:x}", iss);
    log::detail::FormatLineToSink(sink, "ELR_EL2(Return):   {:x}", elr);
    log::detail::FormatLineToSink(sink, "SPSR(Status):      {:x}", spsr);
    log::detail::FormatLineToSink(sink, "FAR_EL2(Fault):    {:x}", far);

    for (size_t i {}; i < ctx.size(); i++) {
        log::detail::FormatLineToSink(sink, i < 10 ? "x{}:  {:x}" : "x{}: {:x}", i, ctx[i]);
    }
    log::detail::FormatLineToSink(sink, "======================================");
}

#endif // __cplusplus
#endif // !__REGISTERDUMP_H__
