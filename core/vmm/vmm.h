// @file vmm.h
// @brief Trap entry points the EL2 vector table and vcpu.S branch to.
// @ingroup vmm
//
// C linkage so the assembly reaches them by unmangled name.

#ifndef __VMM_H__
#define __VMM_H__

#include <cstdint>
#include "core/vmm/esr.h"

class Vcpu;

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

// @brief Synchronous trap from the guest.
// @param vcpu vCPU parked in TPIDR_EL2, guest state already saved.
// @param esr ESR_EL2 captured at the vector.
// @return Nothing.
void handle_lower_el_sync(Vcpu* vcpu, uint64_t esr);

// @brief IRQ taken while the guest was running.
// @param vcpu vCPU parked in TPIDR_EL2, guest state already saved.
// @param esr Unused, zero.
// @return Nothing.
void handle_lower_el_irq(Vcpu* vcpu, uint64_t esr);

// @brief FIQ taken while the guest was running.
// @param vcpu vCPU parked in TPIDR_EL2, guest state already saved.
// @param esr Unused, zero.
// @return Nothing.
void handle_lower_el_fiq(Vcpu* vcpu, uint64_t esr);

// @brief SError taken while the guest was running.
// @param vcpu vCPU parked in TPIDR_EL2, guest state already saved.
// @param esr ESR_EL2 captured at the vector.
// @return Nothing.
void handle_lower_el_serror(Vcpu* vcpu, uint64_t esr);

} // extern "C"

#endif // !__VMM_H__
