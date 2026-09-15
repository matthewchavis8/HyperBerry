// @file vmm.cpp
// @brief Trap handlers for exceptions taken at EL2 and for guest exits.
// @ingroup vmm
//
// EL2 handlers receive the ExceptionContext built by vmm.S. Guest handlers
// receive the Vcpu parked in TPIDR_EL2 and the ESR_EL2 value passed through by
// vcpu.S, then re-enter the guest through vcpu_enter().

#include "vmm.h"
#include "core/vmm/hvc/hvc.h"
#include "core/vmm/smccc/smccc.h"
#include "core/vcpu/vcpu.h"
#include "lib/panic/panic.h"
#include "lib/log/log.h"

namespace {

// @brief Print the saved general purpose registers, then panic.
// @param msg Panic message.
// @param ctx Frame saved by vmm.S.
// @return Does not return.
[[noreturn]] void panicWithFrame(const char* msg, ExceptionContext& ctx) {
    for (size_t i {}; i < ctx.size(); i++) {
        Log::writeLine("[x{}] {:x}", i, ctx[i]);
    }
    hv_panic(msg);
}

// @brief Dispatch a guest HVC and resume the guest unless it asked to stop.
// @param vcpu vCPU that trapped.
// @return Nothing.
void handleHvcExit(Vcpu& vcpu) {
    Log::println("[Guest][HVC] Handling HVC call from guest, call ID={:x}",
            vcpu.getGpReg(VCPU_GPREG_X0));

    switch (handleHvcAarch64(vcpu.m_gpr)) {
        case HvcResult::HANDLED:
        case HvcResult::UNHANDLED:
            vcpu_enter(&vcpu);
            break;

        case HvcResult::HALT:
            hv_panic("[HVC] guest requested halt");

        case HvcResult::RESET:
            hv_panic("[HVC] guest requested reset");
    }
}

} // namespace

extern "C" void handle_el2_sync(ExceptionContext& ctx) {
    panicWithFrame("[HV sync] was triggered", ctx);
}

extern "C" void handle_el2_irq(ExceptionContext& ctx) {
    panicWithFrame("[HV irq] was triggered", ctx);
}

extern "C" void handle_el2_fiq(ExceptionContext& ctx) {
    panicWithFrame("[HV fiq] was triggered", ctx);
}

extern "C" void handle_el2_serror(ExceptionContext& ctx) {
    panicWithFrame("[HV SError] was triggered", ctx);
}

extern "C" void handle_unhandled(ExceptionContext& ctx) {
    panicWithFrame("[HV mysterious exception?] was triggered", ctx);
}

extern "C" void handle_lower_el_sync(Vcpu* vcpu, uint64_t esr) {
    EsrEc exceptionClass { getEsrEc(esr) };

    switch (exceptionClass) {
        case EsrEc::HVC_AARCH64:
            handleHvcExit(*vcpu);
            break;

        case EsrEc::SMC_AARCH64:
            Log::println("[Guest][SMC] Handling SMC call from guest, call ID={:x}",
                    vcpu->getGpReg(VCPU_GPREG_X0));
            vcpu->setGpReg(VCPU_GPREG_X0, SMCCC::toRegister(SMCCC::NOT_SUPPORTED));
            vcpu->skipInstruction();
            vcpu_enter(vcpu);
            break;

        case EsrEc::DATA_ABORT_LOWER:
            hv_panic("[DataAbortLower] unhandled");

        default:
            Log::println("[Guest][ERROR] Unhandled guest exit EC={:x} ISS={:x} ESR={:x}",
                    exceptionClass,
                    getEsrIss(esr),
                    esr);
            hv_panic("[Guest] unhandled lower-EL sync exception");
    }
}

extern "C" void handle_lower_el_irq(Vcpu* vcpu, [[maybe_unused]] uint64_t esr) {
    vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_fiq(Vcpu* vcpu, [[maybe_unused]] uint64_t esr) {
    vcpu_enter(vcpu);
}

extern "C" void handle_lower_el_serror([[maybe_unused]] Vcpu* vcpu, [[maybe_unused]] uint64_t esr) {
    hv_panic("[Guest] SError taken from the guest");
}
