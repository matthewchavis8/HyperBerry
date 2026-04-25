/**
 * @file panic.h
 * @brief Fatal hypervisor panic interface.
 * @ingroup lib
 */

#ifndef __PANIC_H__
#define __PANIC_H__

#include "core/exceptions/exceptions.h"

/**
 * @brief Print a fatal error banner and halt the current CPU indefinitely.
 * @ingroup lib
 *
 * Emits the supplied message, prints a full exception register dump, then
 * enters a low-power wait loop that never returns.
 *
 * @param msg Optional panic message to print before the register dump.
 * @param ctx Saved exception context associated with the fatal condition.
 */
[[noreturn]] void hv_panic(const char* msg, ExceptionContext& ctx);
[[noreturn]] void hv_panic(const char* msg);

#endif // !__PANIC_H__
