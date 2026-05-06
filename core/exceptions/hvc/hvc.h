/**
 * @file hvc.h
 * @brief AArch64 HVC call dispatch for lower-EL guest exits.
 * @ingroup exceptions
 */

#ifndef __HVC_H__
#define __HVC_H__

#include <stdint.h>
#include "core/exceptions/exceptions.h"
#include "lib/array/array.h"

enum class HvcResult : uint64_t {
  Handled,
  Unhandled,
  Halt,
  Reset,
};

using ExceptionContext = hv::array<uint64_t, 31>;

HvcResult handleHvcAarch64(ExceptionContext& gpr);

#endif // __HVC_H__
