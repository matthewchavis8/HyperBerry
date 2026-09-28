// @file vmm.cpp
// @brief Hypervisor exception handlers and guest exit policy.

#include "vmm.h"
#include "core/vmm/hvc/hvc.h"
#include "core/vmm/smccc/smccc.h"
#include "core/vcpu/vcpu.h"
#include "lib/panic/panic.h"
#include "lib/log/log.h"

extern "C" void handle_el2_sync(ExceptionContext& ctx) {
    HvPanic("[HV sync] was triggered", ctx);
}

extern "C" void handle_el2_irq(ExceptionContext& ctx) {
    HvPanic("[HV irq] was triggered", ctx);
}

extern "C" void handle_el2_fiq(ExceptionContext& ctx) {
    HvPanic("[HV fiq] was triggered", ctx);
}

extern "C" void handle_el2_serror(ExceptionContext& ctx) {
    HvPanic("[HV SError] was triggered", ctx);
}

extern "C" void handle_unhandled(ExceptionContext& ctx) {
    HvPanic("[HV mysterious exception?] was triggered", ctx);
}

void HandleGuestExit(GuestRegisters& registers, VcpuExit exit) {
    switch (exit.reason) {
        case ExitReason::IRQ:
        case ExitReason::FIQ:
            return;

        case ExitReason::SERROR:
            HvPanic("[Guest] SError taken from the guest");

        case ExitReason::SYNC:
            break;
    }

    switch (GetEsrEc(exit.syndrome)) {
        case EsrEc::HVC_AARCH64:
            switch (HandleHvcAarch64(registers.x)) {
                case HvcResult::HANDLED:
                case HvcResult::UNHANDLED:
                    return;
                case HvcResult::HALT:
                    HvPanic("[HVC] guest requested halt");
                case HvcResult::RESET:
                    HvPanic("[HVC] guest requested reset");
            }
            break;

        case EsrEc::SMC_AARCH64:
            registers.x[0] = SMCCC::ToRegister(SMCCC::NOT_SUPPORTED);
            registers.pc += 4;
            return;

        case EsrEc::DATA_ABORT_LOWER:
            HvPanic("[DataAbortLower] unhandled");

        default:
            Log::Println("[Guest] Unhandled exception EC={:x} ISS={:x} ESR={:x}",
                    GetEsrEc(exit.syndrome), GetEsrIss(exit.syndrome), exit.syndrome);
            HvPanic("[Guest] unhandled synchronous exception");
    }
}
