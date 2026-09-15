// @file hvc.cpp
// @brief AArch64 HVC dispatch and PSCI over HVC.
// @ingroup hypercalls

#include "hvc.h"
#include "core/vmm/smccc/smccc.h"
#include "lib/log/log.h"

namespace {

namespace PSCI {

    constexpr uint64_t VERSION { 0x84000000ULL };
    constexpr uint64_t SYSTEM_OFF { 0x84000008ULL };
    constexpr uint64_t SYSTEM_RESET { 0x84000009ULL };
    constexpr uint64_t FEATURES { 0x8400000AULL };

    constexpr uint64_t VERSION_1_0 { 0x00010000ULL };
    constexpr uint64_t SUCCESS { 0ULL };
    constexpr uint64_t NOT_SUPPORTED { static_cast<uint64_t>(-1) };

    // @brief Check whether a PSCI function ID is one this handler implements.
    // @param callId PSCI function ID.
    // @return true when handlePsci services it.
    bool isSupportedPsciCall(uint64_t callId) {
        return callId == VERSION || callId == SYSTEM_OFF || callId == SYSTEM_RESET ||
                callId == FEATURES;
    }

    // @brief Service a PSCI call.
    // @param gpr Saved guest registers; x0 carries the result.
    // @return Whether the guest can resume or asked to stop.
    HvcResult handlePsci(ExceptionContext& gpr) {
        uint64_t callId { gpr[0] };

        switch (callId) {
            case VERSION:
                Log::println("[Guest][PSCI] VERSION");
                gpr[0] = VERSION_1_0;
                return HvcResult::HANDLED;

            case FEATURES: {
                uint64_t queriedCall { gpr[1] };
                Log::println("[Guest][PSCI] FEATURES function={:x}", queriedCall);
                gpr[0] = isSupportedPsciCall(queriedCall) ? SUCCESS : NOT_SUPPORTED;
                return HvcResult::HANDLED;
            }

            case SYSTEM_OFF:
                Log::println("[Guest][PSCI] SYSTEM_OFF");
                return HvcResult::HALT;

            case SYSTEM_RESET:
                Log::println("[Guest][PSCI] SYSTEM_RESET");
                return HvcResult::RESET;

            default:
                return HvcResult::UNHANDLED;
        }
    }

} // namespace PSCI

} // namespace

HvcResult handleHvcAarch64(ExceptionContext& gpr) {
    uint64_t callId { gpr[0] };

    switch (SMCCC::getOwner(callId)) {
        case SMCCC::OWNER_STANDARD:
            return PSCI::handlePsci(gpr);

        case SMCCC::OWNER_VENDOR_HYP:
            return HvcResult::UNHANDLED;

        default:
            Log::println("[Guest][HVC] Unsupported call ID={:x}", callId);
            return HvcResult::UNHANDLED;
    }
}
