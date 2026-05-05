/**
 * @file hvc.cpp
 * @brief AArch64 HVC call dispatch and private PSCI-over-HVC handling.
 * @ingroup exceptions
 */

#include "hvc.h"
#include "core/exceptions/exceptions.h"
#include "drivers/uart/uart.h"

namespace {

namespace PSCI {

constexpr uint64_t VERSION      = 0x84000000ULL;
constexpr uint64_t SYSTEM_OFF   = 0x84000008ULL;
constexpr uint64_t SYSTEM_RESET = 0x84000009ULL;
constexpr uint64_t FEATURES     = 0x8400000AULL;

constexpr uint64_t VERSION_1_0   = 0x00010000ULL;
constexpr uint64_t SUCCESS       = 0ULL;
constexpr uint64_t NOT_SUPPORTED = static_cast<uint64_t>(-1);

bool isSupportedCall(uint64_t callId) {
  return callId == VERSION
      || callId == SYSTEM_OFF
      || callId == SYSTEM_RESET
      || callId == FEATURES;
}

// TODO: I do not agree with this being inside PSCI we should just have one handler
HvcResult handlePsci(ExceptionContext& regs) {
  uint64_t callId = regs[0];

  switch (callId) {

    case VERSION:
      Uart::println("[Guest][PSCI] VERSION");
      regs[0] = VERSION_1_0;
      return HvcResult::Handled;

    case FEATURES: {
      uint64_t queriedCall = regs[1];
      Uart::println("[Guest][PSCI] FEATURES function={:x}", queriedCall);
      regs[0] = isSupportedCall(queriedCall) ? SUCCESS
                                             : NOT_SUPPORTED;
      return HvcResult::Handled;
    }

    case SYSTEM_OFF:
      Uart::println("[Guest][PSCI] SYSTEM_OFF");
      return HvcResult::Halt;

    case SYSTEM_RESET:
      Uart::println("[Guest][PSCI] SYSTEM_RESET");
      return HvcResult::Reset;

    default:
      return HvcResult::Unhandled;
  }
}

} // namespace PSCI

} // anonymous namespace

HvcResult handleHvcAarch64(ExceptionContext& regs) {
  uint64_t callId = regs[0];

  if (PSCI::isSupportedCall(callId))
    return PSCI::handlePsci(regs);

  Uart::println("[Guest][HVC] Unsupported call ID={:x}", callId);
  return HvcResult::Unhandled;
}
