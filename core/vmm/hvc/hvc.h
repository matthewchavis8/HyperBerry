// @file hvc.h
// @brief AArch64 HVC dispatch for guest exits.
// @ingroup hypercalls

#ifndef __HVC_H__
#define __HVC_H__

#include <stdint.h>
#include "core/vmm/esr.h"

// @brief Outcome of dispatching a guest HVC.
enum class HvcResult : uint8_t {
    HANDLED,   // handled, the guest can resume
    UNHANDLED, // an HVC exit, but the call is not implemented
    HALT,      // the guest asked to power off, such as PSCI SYSTEM_OFF
    RESET,     // the guest asked to reset, such as PSCI SYSTEM_RESET
};

// @brief Dispatch an AArch64 HVC from the guest register context.
//
// The function ID is read from x0. Standard service calls go to the PSCI
// handler; other owners return HvcResult::UNHANDLED.
//
// @param gpr Saved guest registers x0 to x30. Results are written back using
//            the SMCCC register convention.
// @return Whether the guest can resume or asked to stop.
HvcResult handleHvcAarch64(ExceptionContext& gpr);

#endif // !__HVC_H__
