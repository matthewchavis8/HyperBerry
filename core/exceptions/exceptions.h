/**
 * @file exceptions.h
 * @brief Exception context structure shared between assembly and C++.
 * @ingroup exceptions
 */

#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stdint.h>
#include "lib/array/array.h"

/**
 * @brief Saved CPU state at the moment an exception is taken.
 * @ingroup exceptions
 *
 * Populated by the @c save_context macro in exceptions.S.
 * The layout must match the assembly stack frame exactly:
 * gpr[0..30] at offsets 0-240, elr at 248, spsr at 256.
 */
struct ExceptionContext {
  hv::array<uint64_t, 31> gpr;  /**< General-purpose registers x0-x30. */
  uint64_t elr;                  /**< Exception Link Register (ELR_EL2). */
  uint64_t spsr;                 /**< Saved Program Status Register (SPSR_EL2). */
};

#endif // __EXCEPTIONS_H__
