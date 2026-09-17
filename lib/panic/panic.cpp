// @file panic.cpp
// @brief Fatal hypervisor panic implementation.
// @ingroup lib

#include "panic.h"
#include "lib/log/log.h"
#include "lib/registerDump/registerDump.h"
#include "drivers/uart/uart.h"

namespace {

void uartSink(char ch) {
    Uart::GetInstance().Putc(ch);
}

} // namespace

// @brief Print panic diagnostics and stop execution permanently.
// @ingroup lib
// @param msg Optional panic message to print.
// @param ctx Saved exception context for diagnostic output.
[[noreturn]] void HvPanic(const char* msg, const std::array<uint64_t, 31>& ctx) {
    log::detail::FormatLineToSink(uartSink, "=======================================");
    log::detail::FormatLineToSink(uartSink, "=             HV PANIC                =");
    log::detail::FormatLineToSink(uartSink, "=======================================");

    if (msg) log::detail::FormatLineToSink(uartSink, "[ERROR] {}", msg);

    RegisterDump(ctx);

    for (;;) {
        asm volatile("wfe");
    }
}

// @brief Print panic diagnostics and stop execution permanently.
// @ingroup lib
// @param msg Optional panic message to print.
[[noreturn]] void HvPanic(const char* msg) {
    log::detail::FormatLineToSink(uartSink, "=======================================");
    log::detail::FormatLineToSink(uartSink, "=             HV PANIC                =");
    log::detail::FormatLineToSink(uartSink, "=======================================");

    if (msg) log::detail::FormatLineToSink(uartSink, "[ERROR] {}", msg);

    // Dump System Register State
    uint64_t esr {};
    uint64_t elr {};
    uint64_t far {};
    uint64_t spsr {};
    uint64_t hpfar {};
    uint64_t vttbr {};
    uint64_t vtcr {};

    asm volatile("mrs %0, esr_el2" : "=r"(esr));
    asm volatile("mrs %0, elr_el2" : "=r"(elr));
    asm volatile("mrs %0, far_el2" : "=r"(far));
    asm volatile("mrs %0, spsr_el2" : "=r"(spsr));
    asm volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
    asm volatile("mrs %0, vttbr_el2" : "=r"(vttbr));
    asm volatile("mrs %0, vtcr_el2" : "=r"(vtcr));

    uint64_t ec { (esr >> 26) & 0x3F };
    uint64_t iss { esr & 0x1FFFFFF };
    // HPFAR_EL2[39:4] holds IPA[47:12] of the stage-2 fault.
    uint64_t fault_ipa { (hpfar & 0xFFFFFFFFF0ULL) << 8 };

    log::detail::FormatLineToSink(uartSink, "[ESR_EL2]   {:x} EC={:x} ISS={:x}", esr, ec, iss);
    log::detail::FormatLineToSink(uartSink, "[ELR_EL2]   {:x}", elr);
    log::detail::FormatLineToSink(uartSink, "[FAR_EL2]   {:x}", far);
    log::detail::FormatLineToSink(uartSink, "[SPSR_EL2]  {:x}", spsr);
    log::detail::FormatLineToSink(uartSink, "[HPFAR_EL2] {:x} IPA={:x}", hpfar, fault_ipa);
    log::detail::FormatLineToSink(uartSink, "[VTTBR_EL2] {:x}", vttbr);
    log::detail::FormatLineToSink(uartSink, "[VTCR_EL2]  {:x}", vtcr);

    for (;;) {
        asm volatile("wfe");
    }
}
