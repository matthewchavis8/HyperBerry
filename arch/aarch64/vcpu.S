/**
 * @file vcpu_enter.S
 * @brief vCPU entry/exit trampoline (EL2).
 * @ingroup vcpu
 */

#include "core/vcpu/vcpu.h"

.section .text

.global vcpu_enter
vcpu_enter:
    msr     tpidr_el2, x0 // Save the current VCPU

    // Save Hypervisor State before swapping to el1
    add     x1, x0, #VCPU_HVCTX_OFFSET
    mov     x2, sp
    str     x2,       [x1, #VCPU_HVCTX_SP]
    str     lr,       [x1, #VCPU_HVCTX_LR]
    stp     x19, x20, [x1, #VCPU_HVCTX_X19]
    stp     x21, x22, [x1, #VCPU_HVCTX_X21]
    stp     x23, x24, [x1, #VCPU_HVCTX_X23]
    stp     x25, x26, [x1, #VCPU_HVCTX_X25]
    stp     x27, x28, [x1, #VCPU_HVCTX_X27]
    str     x29,      [x1, #VCPU_HVCTX_X29]
    
    // Restore guest system registers
    bl      vcpu_restore_el1_sysregs

    // Resore elr and spsr
    mrs     x0, tpidr_el2
    add     x1, x0, #VCPU_EL2STATE_OFFSET
    ldr     x2, [x1, #VCPU_ELR_EL2]
    ldr     x3, [x1, #VCPU_SPSR_EL2]
    msr     elr_el2,  x2
    msr     spsr_el2, x3

    // Load guest GPRs; x0 holds base so it must be restored last
    add     x0, x0, #VCPU_GPREGS_OFFSET
    ldp     x2,  x3,  [x0, #VCPU_GPREG_X2]
    ldp     x4,  x5,  [x0, #VCPU_GPREG_X4]
    ldp     x6,  x7,  [x0, #VCPU_GPREG_X6]
    ldp     x8,  x9,  [x0, #VCPU_GPREG_X8]
    ldp     x10, x11, [x0, #VCPU_GPREG_X10]
    ldp     x12, x13, [x0, #VCPU_GPREG_X12]
    ldp     x14, x15, [x0, #VCPU_GPREG_X14]
    ldp     x16, x17, [x0, #VCPU_GPREG_X16]
    ldp     x18, x19, [x0, #VCPU_GPREG_X18]
    ldp     x20, x21, [x0, #VCPU_GPREG_X20]
    ldp     x22, x23, [x0, #VCPU_GPREG_X22]
    ldp     x24, x25, [x0, #VCPU_GPREG_X24]
    ldp     x26, x27, [x0, #VCPU_GPREG_X26]
    ldp     x28, x29, [x0, #VCPU_GPREG_X28]
    ldr     x30,      [x0, #VCPU_GPREG_LR]
    ldr     x2,       [x0, #VCPU_GPREG_SP_EL0]
    msr     sp_el0, x2
    ldp     x0,  x1,  [x0, #VCPU_GPREG_X0]  // clobbers base — must be last

    eret


// Branched to from lower_el_* vector entries.
// Vector entry: stp x0,x1 [sp-16]!  stp x2,x3 [sp-16]!
// Stack on entry: sp+0=x2, sp+8=x3, sp+16=x0, sp+24=x1
.global vcpu_exit
vcpu_exit:
    mrs     x2, tpidr_el2
    add     x2, x2, #VCPU_GPREGS_OFFSET

    // Recover x0–x3 from the vector-entry stack frame
    ldr     x3, [sp, #16]
    str     x3, [x2, #VCPU_GPREG_X0]
    ldr     x3, [sp, #24]
    str     x3, [x2, #VCPU_GPREG_X1]
    ldr     x3, [sp, #0]
    str     x3, [x2, #VCPU_GPREG_X2]
    ldr     x3, [sp, #8]
    str     x3, [x2, #VCPU_GPREG_X3]

    stp     x4,  x5,  [x2, #VCPU_GPREG_X4]
    stp     x6,  x7,  [x2, #VCPU_GPREG_X6]
    stp     x8,  x9,  [x2, #VCPU_GPREG_X8]
    stp     x10, x11, [x2, #VCPU_GPREG_X10]
    stp     x12, x13, [x2, #VCPU_GPREG_X12]
    stp     x14, x15, [x2, #VCPU_GPREG_X14]
    stp     x16, x17, [x2, #VCPU_GPREG_X16]
    stp     x18, x19, [x2, #VCPU_GPREG_X18]
    stp     x20, x21, [x2, #VCPU_GPREG_X20]
    stp     x22, x23, [x2, #VCPU_GPREG_X22]
    stp     x24, x25, [x2, #VCPU_GPREG_X24]
    stp     x26, x27, [x2, #VCPU_GPREG_X26]
    stp     x28, x29, [x2, #VCPU_GPREG_X28]
    str     x30,      [x2, #VCPU_GPREG_LR]
    mrs     x3, sp_el0
    str     x3,       [x2, #VCPU_GPREG_SP_EL0]

    mrs     x0, tpidr_el2
    add     x1, x0, #VCPU_EL2STATE_OFFSET
    mrs     x2, elr_el2
    mrs     x3, spsr_el2
    stp     x2, x3, [x1, #VCPU_ELR_EL2]

    bl      vcpu_save_el1_sysregs

    // Restore HV callee-saved state from m_hvCtx
    mrs     x0, tpidr_el2
    add     x1, x0, #VCPU_HVCTX_OFFSET
    ldr     x2,       [x1, #VCPU_HVCTX_SP]
    ldr     lr,       [x1, #VCPU_HVCTX_LR]
    mov     sp, x2
    ldp     x19, x20, [x1, #VCPU_HVCTX_X19]
    ldp     x21, x22, [x1, #VCPU_HVCTX_X21]
    ldp     x23, x24, [x1, #VCPU_HVCTX_X23]
    ldp     x25, x26, [x1, #VCPU_HVCTX_X25]
    ldp     x27, x28, [x1, #VCPU_HVCTX_X27]
    ldr     x29,      [x1, #VCPU_HVCTX_X29]

    ret

// Save all guest state into the Vcpu pointed to by tpidr_el2.
// Expects the vector-entry stack frame: sp+0=x2, sp+8=x3, sp+16=x0, sp+24=x1.
// Pops the 32-byte frame. Clobbers x0-x3 and x30.
.macro save_guest_state
    mrs     x2, tpidr_el2
    add     x2, x2, #VCPU_GPREGS_OFFSET
    ldr     x3, [sp, #16];  str x3, [x2, #VCPU_GPREG_X0]
    ldr     x3, [sp, #24];  str x3, [x2, #VCPU_GPREG_X1]
    ldr     x3, [sp, #0];   str x3, [x2, #VCPU_GPREG_X2]
    ldr     x3, [sp, #8];   str x3, [x2, #VCPU_GPREG_X3]
    add     sp, sp, #32
    stp     x4,  x5,  [x2, #VCPU_GPREG_X4]
    stp     x6,  x7,  [x2, #VCPU_GPREG_X6]
    stp     x8,  x9,  [x2, #VCPU_GPREG_X8]
    stp     x10, x11, [x2, #VCPU_GPREG_X10]
    stp     x12, x13, [x2, #VCPU_GPREG_X12]
    stp     x14, x15, [x2, #VCPU_GPREG_X14]
    stp     x16, x17, [x2, #VCPU_GPREG_X16]
    stp     x18, x19, [x2, #VCPU_GPREG_X18]
    stp     x20, x21, [x2, #VCPU_GPREG_X20]
    stp     x22, x23, [x2, #VCPU_GPREG_X22]
    stp     x24, x25, [x2, #VCPU_GPREG_X24]
    stp     x26, x27, [x2, #VCPU_GPREG_X26]
    stp     x28, x29, [x2, #VCPU_GPREG_X28]
    str     x30,      [x2, #VCPU_GPREG_LR]
    mrs     x3, sp_el0
    str     x3,       [x2, #VCPU_GPREG_SP_EL0]
    mrs     x0, tpidr_el2
    add     x1, x0, #VCPU_EL2STATE_OFFSET
    mrs     x2, elr_el2
    mrs     x3, spsr_el2
    stp     x2, x3, [x1, #VCPU_ELR_EL2]
    bl      vcpu_save_el1_sysregs
.endm

.global vcpu_exit_sync
.global vcpu_exit_irq
.global vcpu_exit_fiq
.global vcpu_exit_serror

vcpu_exit_sync:
    // x0 = ESR_EL2 (set by vector entry: mrs x0, esr_el2)
    mrs     x2, tpidr_el2
    str     x0, [x2, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]
    save_guest_state
    mrs     x0, tpidr_el2
    ldr     x1, [x0, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]
    bl      handle_lower_el_sync
    b       .

vcpu_exit_irq:
    save_guest_state
    mrs     x0, tpidr_el2
    mov     x1, xzr
    bl      handle_lower_el_irq
    b       .

vcpu_exit_fiq:
    save_guest_state
    mrs     x0, tpidr_el2
    mov     x1, xzr
    bl      handle_lower_el_fiq
    b       .

vcpu_exit_serror:
    // x0 = ESR_EL2 (set by vector entry: mrs x0, esr_el2)
    mrs     x2, tpidr_el2
    str     x0, [x2, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]
    save_guest_state
    mrs     x0, tpidr_el2
    ldr     x1, [x0, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]
    bl      handle_lower_el_serror
    b       .
