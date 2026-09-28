// @file vmm.h
// @brief Hypervisor exception entries and guest exit handling.
// @ingroup vmm
//
// Hypervisor exception entries use C linkage for assembly.

#ifndef __VMM_H__
#define __VMM_H__

#include <cstdint>
#include "core/vmm/esr.h"

struct GuestRegisters;
struct VcpuExit;

// @brief Apply guest exit policy to saved state before the next run.
// @return Nothing. Fatal exits panic.
void HandleGuestExit(GuestRegisters& registers, VcpuExit exit);

extern "C" {

// @brief Synchronous exception taken while the hypervisor was running.
// @param ctx Frame saved by vmm.S.
// @return Does not return.
[[noreturn]] void handle_el2_sync(ExceptionContext& ctx);

// @brief IRQ taken while the hypervisor was running.
// @param ctx Frame saved by vmm.S.
// @return Does not return.
[[noreturn]] void handle_el2_irq(ExceptionContext& ctx);

// @brief FIQ taken while the hypervisor was running.
// @param ctx Frame saved by vmm.S.
// @return Does not return.
[[noreturn]] void handle_el2_fiq(ExceptionContext& ctx);

// @brief SError taken while the hypervisor was running.
// @param ctx Frame saved by vmm.S.
// @return Does not return.
[[noreturn]] void handle_el2_serror(ExceptionContext& ctx);

// @brief Any vector the table does not service.
// @param ctx Frame saved by vmm.S.
// @return Does not return.
[[noreturn]] void handle_unhandled(ExceptionContext& ctx);

} // extern "C"

#endif // __VMM_H__
