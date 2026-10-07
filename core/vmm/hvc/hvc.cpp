#include "hvc.h"
#include "core/vmm/smc/smccc.h"
#include "lib/log/log.h"

namespace {
namespace Psci {
    constexpr uint64_t VERSION { 0x84000000ULL };
    constexpr uint64_t SYSTEM_OFF { 0x84000008ULL };
    constexpr uint64_t SYSTEM_RESET { 0x84000009ULL };
    constexpr uint64_t FEATURES { 0x8400000AULL };
    constexpr uint64_t VERSION_1_0 { 0x00010000ULL };

    bool Supports(uint64_t call) {
        return call == VERSION || call == SYSTEM_OFF || call == SYSTEM_RESET || call == FEATURES;
    }
} // namespace Psci
} // namespace

Hvc::Action Hvc::Handle(GuestRegisters& registers, uint32_t immediate) {
    auto& x { registers.x };
    if (immediate != 0 || SMCCC::GetOwner(x[0]) != SMCCC::OWNER_STANDARD) {
        x[0] = SMCCC::ToRegister(SMCCC::NOT_SUPPORTED);
        return Action::RESUME;
    }

    switch (x[0]) {
        case Psci::VERSION:
            x[0] = Psci::VERSION_1_0;
            return Action::RESUME;
        case Psci::FEATURES:
            x[0] = Psci::Supports(x[1]) ? 0 : SMCCC::ToRegister(SMCCC::NOT_SUPPORTED);
            return Action::RESUME;
        case Psci::SYSTEM_OFF:
            Log::Println("[Guest][PSCI] SYSTEM_OFF");
            return Action::SHUTDOWN;
        case Psci::SYSTEM_RESET:
            Log::Println("[Guest][PSCI] SYSTEM_RESET");
            return Action::RESET;
        default:
            x[0] = SMCCC::ToRegister(SMCCC::NOT_SUPPORTED);
            return Action::RESUME;
    }
}
