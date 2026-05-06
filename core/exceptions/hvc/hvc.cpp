/**
 * @file hvc.cpp
 * @brief AArch64 HVC call dispatch and private PSCI-over-HVC handling.
 * @ingroup exceptions
 */

#include "hvc.h"
#include "core/exceptions/exceptions.h"
#include "core/exceptions/smccc/smccc.h"
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

bool isSupportedPsciCall(uint64_t callId) {
  return callId == VERSION
      || callId == SYSTEM_OFF
      || callId == SYSTEM_RESET
      || callId == FEATURES;
}

HvcResult handlePsci(ExceptionContext& gpr) {
  uint64_t callId = gpr[0];

  switch (callId) {

    case VERSION:
      Uart::println("[Guest][PSCI] VERSION");
      gpr[0] = VERSION_1_0;
      return HvcResult::Handled;

    case FEATURES: {
      uint64_t queriedCall = gpr[1];
      Uart::println("[Guest][PSCI] FEATURES function={:x}", queriedCall);
      gpr[0] = isSupportedPsciCall(queriedCall) ? SUCCESS
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

HvcResult handleHvcAarch64(ExceptionContext& gpr) {
  uint64_t callId = gpr[0];
  uint32_t owner = SMCCC::getOwner(callId);

  switch (owner) {
    case SMCCC::OWNER_STANDARD:
      return PSCI::handlePsci(gpr);

    case SMCCC::OWNER_VENDOR_HYP:
      return HvcResult::Unhandled;

    default:
      Uart::println("[Guest][HVC] Unsupported call ID={:x}", callId);
      return HvcResult::Unhandled;
  }
}
