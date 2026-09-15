/**
 * @file panic.h
 * @brief Fatal hypervisor panic interface.
 * @ingroup lib
 */

#ifndef __PANIC_H__
#define __PANIC_H__

// @brief Print a fatal error banner and the EL2 fault registers, then halt.
// @param msg Optional panic message to print before the register dump.
// @return Does not return.
[[noreturn]] void hv_panic(const char* msg);

#endif // !__PANIC_H__
