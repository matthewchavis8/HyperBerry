// @file vcpu.S
// @brief Enter a guest and return to the caller on its next exception.

#include "vcpuOffsets.h"

.section .text

.equ HOST_FRAME_SIZE, 144
.equ HOST_TPIDR_DAIF, 96
.equ HOST_SP_ELR, 112
.equ HOST_SPSR, 128

.global vcpu_read_sctlr
vcpu_read_sctlr:
    mrs x0, sctlr_el1
    ret

.global vcpu_run
vcpu_run:
    mrs x2, daif
    msr daifset, #0xf
    sub sp, sp, #HOST_FRAME_SIZE
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    mrs x1, tpidr_el2
    stp x1, x2, [sp, #HOST_TPIDR_DAIF]
    mrs x1, sp_el0
    mrs x2, elr_el2
    stp x1, x2, [sp, #HOST_SP_ELR]
    mrs x1, spsr_el2
    str x1, [sp, #HOST_SPSR]
    msr tpidr_el2, x0

.macro restore_register reg, offset
    ldr x1, [x0, #\offset]
    msr \reg, x1
.endm

    restore_register TTBR0_EL1, VCPU_TTBR0_EL1
    restore_register TTBR1_EL1, VCPU_TTBR1_EL1
    restore_register TCR_EL1, VCPU_TCR_EL1
    restore_register MAIR_EL1, VCPU_MAIR_EL1
    restore_register AMAIR_EL1, VCPU_AMAIR_EL1
    restore_register VBAR_EL1, VCPU_VBAR_EL1
    restore_register ELR_EL1, VCPU_ELR_EL1
    restore_register SPSR_EL1, VCPU_SPSR_EL1
    restore_register ESR_EL1, VCPU_ESR_EL1
    restore_register FAR_EL1, VCPU_FAR_EL1
    restore_register AFSR0_EL1, VCPU_AFSR0_EL1
    restore_register AFSR1_EL1, VCPU_AFSR1_EL1
    restore_register CONTEXTIDR_EL1, VCPU_CONTEXTIDR_EL1
    restore_register TPIDR_EL1, VCPU_TPIDR_EL1
    restore_register TPIDR_EL0, VCPU_TPIDR_EL0
    restore_register TPIDRRO_EL0, VCPU_TPIDRRO_EL0
    restore_register CNTKCTL_EL1, VCPU_CNTKCTL_EL1
    restore_register CPACR_EL1, VCPU_CPACR_EL1
    restore_register PAR_EL1, VCPU_PAR_EL1
    restore_register CSSELR_EL1, VCPU_CSSELR_EL1
    restore_register SP_EL0, VCPU_SP_EL0
    restore_register SP_EL1, VCPU_SP_EL1
    isb
    restore_register SCTLR_EL1, VCPU_SCTLR_EL1
    isb
    ldr x1, [x0, #VCPU_PC]
    msr elr_el2, x1
    ldr x1, [x0, #VCPU_PSTATE]
    msr spsr_el2, x1

    ldp x2, x3, [x0, #VCPU_X2]
    ldp x4, x5, [x0, #VCPU_X4]
    ldp x6, x7, [x0, #VCPU_X6]
    ldp x8, x9, [x0, #VCPU_X8]
    ldp x10, x11, [x0, #VCPU_X10]
    ldp x12, x13, [x0, #VCPU_X12]
    ldp x14, x15, [x0, #VCPU_X14]
    ldp x16, x17, [x0, #VCPU_X16]
    ldp x18, x19, [x0, #VCPU_X18]
    ldp x20, x21, [x0, #VCPU_X20]
    ldp x22, x23, [x0, #VCPU_X22]
    ldp x24, x25, [x0, #VCPU_X24]
    ldp x26, x27, [x0, #VCPU_X26]
    ldp x28, x29, [x0, #VCPU_X28]
    ldr x30, [x0, #VCPU_X30]
    ldp x0, x1, [x0, #VCPU_X0]
    eret

// Vectors save x0 to x3 and supply the syndrome in x0 for sync and SError.

.global vcpu_exit_sync
vcpu_exit_sync:
    mov x1, #VCPU_EXIT_SYNC
    b vcpu_exit

.global vcpu_exit_irq
vcpu_exit_irq:
    mov x0, xzr
    mov x1, #VCPU_EXIT_IRQ
    b vcpu_exit

.global vcpu_exit_fiq
vcpu_exit_fiq:
    mov x0, xzr
    mov x1, #VCPU_EXIT_FIQ
    b vcpu_exit

.global vcpu_exit_serror
vcpu_exit_serror:
    mov x1, #VCPU_EXIT_SERROR
    b vcpu_exit

vcpu_exit:
    stp x1, x0, [sp, #-16]!
    mrs x2, tpidr_el2
    ldr x3, [sp, #32]
    str x3, [x2, #VCPU_X0]
    ldr x3, [sp, #40]
    str x3, [x2, #VCPU_X1]
    ldr x3, [sp, #16]
    str x3, [x2, #VCPU_X2]
    ldr x3, [sp, #24]
    str x3, [x2, #VCPU_X3]
    stp x4, x5, [x2, #VCPU_X4]
    stp x6, x7, [x2, #VCPU_X6]
    stp x8, x9, [x2, #VCPU_X8]
    stp x10, x11, [x2, #VCPU_X10]
    stp x12, x13, [x2, #VCPU_X12]
    stp x14, x15, [x2, #VCPU_X14]
    stp x16, x17, [x2, #VCPU_X16]
    stp x18, x19, [x2, #VCPU_X18]
    stp x20, x21, [x2, #VCPU_X20]
    stp x22, x23, [x2, #VCPU_X22]
    stp x24, x25, [x2, #VCPU_X24]
    stp x26, x27, [x2, #VCPU_X26]
    stp x28, x29, [x2, #VCPU_X28]
    str x30, [x2, #VCPU_X30]
    mov x0, x2

.macro save_register reg, offset
    mrs x1, \reg
    str x1, [x0, #\offset]
.endm

    save_register SCTLR_EL1, VCPU_SCTLR_EL1
    save_register TTBR0_EL1, VCPU_TTBR0_EL1
    save_register TTBR1_EL1, VCPU_TTBR1_EL1
    save_register TCR_EL1, VCPU_TCR_EL1
    save_register MAIR_EL1, VCPU_MAIR_EL1
    save_register AMAIR_EL1, VCPU_AMAIR_EL1
    save_register VBAR_EL1, VCPU_VBAR_EL1
    save_register ELR_EL1, VCPU_ELR_EL1
    save_register SPSR_EL1, VCPU_SPSR_EL1
    save_register ESR_EL1, VCPU_ESR_EL1
    save_register FAR_EL1, VCPU_FAR_EL1
    save_register AFSR0_EL1, VCPU_AFSR0_EL1
    save_register AFSR1_EL1, VCPU_AFSR1_EL1
    save_register CONTEXTIDR_EL1, VCPU_CONTEXTIDR_EL1
    save_register TPIDR_EL1, VCPU_TPIDR_EL1
    save_register TPIDR_EL0, VCPU_TPIDR_EL0
    save_register TPIDRRO_EL0, VCPU_TPIDRRO_EL0
    save_register CNTKCTL_EL1, VCPU_CNTKCTL_EL1
    save_register CPACR_EL1, VCPU_CPACR_EL1
    save_register PAR_EL1, VCPU_PAR_EL1
    save_register CSSELR_EL1, VCPU_CSSELR_EL1
    save_register SP_EL0, VCPU_SP_EL0
    save_register SP_EL1, VCPU_SP_EL1
    mrs x1, elr_el2
    str x1, [x0, #VCPU_PC]
    mrs x1, spsr_el2
    str x1, [x0, #VCPU_PSTATE]

    ldp x0, x1, [sp]
    add sp, sp, #48
    ldp x2, x3, [sp, #HOST_SP_ELR]
    msr sp_el0, x2
    msr elr_el2, x3
    ldr x2, [sp, #HOST_SPSR]
    msr spsr_el2, x2
    ldp x2, x3, [sp, #HOST_TPIDR_DAIF]
    msr tpidr_el2, x2
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #HOST_FRAME_SIZE
    msr daif, x3
    ret
