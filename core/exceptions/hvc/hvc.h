/**
 * @file hvc.h
 * @brief AArch64 HVC call dispatch for lower-EL guest exits.
 * @ingroup hypercalls
 */

#ifndef __HVC_H__
#define __HVC_H__

#include <stdint.h>
#include "core/exceptions/exceptions.h"
#include "lib/array/array.h"

/**
 * @brief Result returned after dispatching a guest HVC call.
 */
enum class HvcResult : uint64_t {
  /** The call was handled and the guest can resume. */
  Handled,
  /** The call was recognized as an HVC exit but is not implemented. */
  Unhandled,
  /** The guest requested shutdown, such as PSCI SYSTEM_OFF. */
  Halt,
  /** The guest requested reset, such as PSCI SYSTEM_RESET. */
  Reset,
};

using ExceptionContext = hv::array<uint64_t, 31>;

/**
 * @brief Dispatch an AArch64 HVC call from the guest register context.
 *
 * The function ID is read from ``x0`` in the saved general-purpose register
 * array. Supported standard-service calls are routed to the local PSCI handler;
 * unsupported owners return HvcResult::Unhandled.
 *
 * @param gpr Saved guest registers x0-x30. Return values are written back into
 *            this array using the SMCCC register convention.
 * @return Dispatch status describing whether the guest can resume or must stop.
 */
HvcResult handleHvcAarch64(ExceptionContext& gpr);

#endif // __HVC_H__
