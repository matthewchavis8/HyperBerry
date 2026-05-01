/**
 * @file test_vectors.S
 * @brief Test-only EL2 vector table for the Gic integration suite.
 *
 * Current-EL IRQ catches the physical SPI, and lower-EL/AArch64 sync catches
 * the guest's HVC before unwinding to the test caller via Vcpu::HvContext.
 */

#include "core/vcpu/vcpu.h"

.section .text.test_gic_vectors
.balign 0x800
.global test_gic_vectors
.global test_gic_unhandled

test_gic_vectors:
  // Group 1: Current EL with SP_EL0 (unused)
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled

  // Group 2: Current EL with SP_ELx (unused)
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_el2_irq
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled

  // Group 3: Lower EL using AArch64
  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    ldr x4, =gTestGicVectorExitKind
    mov x5, #1
    str x5, [x4]
    mrs x0, esr_el2
    b test_gic_exit_sync

  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    ldr x4, =gTestGicVectorExitKind
    mov x5, #2
    str x5, [x4]
    mov x0, #0
    b test_gic_exit_sync

  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    ldr x4, =gTestGicVectorExitKind
    mov x5, #3
    str x5, [x4]
    mov x0, #0
    b test_gic_exit_sync

  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    ldr x4, =gTestGicVectorExitKind
    mov x5, #4
    str x5, [x4]
    mrs x0, esr_el2
    b test_gic_exit_sync

  // Group 4: Lower EL using AArch32 (unused)
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled
  .balign 0x80; b test_gic_unhandled

test_gic_exit_sync:
    // Stash ESR_EL2 into HvContext.exitEsr.
    mrs     x2, tpidr_el2
    str     x0, [x2, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]

    // Mirror save_guest_state from arch/aarch64/vcpu.S so the saved Vcpu
    // reflects the guest's exit-time GPRs.
    mrs     x2, tpidr_el2
    add     x2, x2, #VCPU_GPREGS_OFFSET
    ldr     x3, [sp, #32]
    str     x3, [x2, #VCPU_GPREG_X0]
    ldr     x3, [sp, #40]
    str     x3, [x2, #VCPU_GPREG_X1]
    ldr     x3, [sp, #16]
    str     x3, [x2, #VCPU_GPREG_X2]
    ldr     x3, [sp, #24]
    str     x3, [x2, #VCPU_GPREG_X3]
    ldp     x4, x5, [sp, #0]
    stp     x4, x5, [x2, #VCPU_GPREG_X4]
    add     sp, sp, #48
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

    mrs     x0, tpidr_el2
    add     x1, x0, #VCPU_EL2STATE_OFFSET
    mrs     x2, elr_el2
    mrs     x3, spsr_el2
    stp     x2, x3, [x1, #VCPU_ELR_EL2]
    bl      vcpu_save_el1_sysregs

    // Unwind to the test caller via HvContext that vcpu_enter populated.
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

test_gic_el2_irq:
    sub     sp, sp,   #272
    stp     x0, x1,    [sp, #0]
    stp     x2, x3,    [sp, #16]
    stp     x4, x5,    [sp, #32]
    stp     x6, x7,    [sp, #48]
    stp     x8, x9,    [sp, #64]
    stp     x10, x11,  [sp, #80]
    stp     x12, x13,  [sp, #96]
    stp     x14, x15,  [sp, #112]
    stp     x16, x17,  [sp, #128]
    stp     x18, x19,  [sp, #144]
    stp     x20, x21,  [sp, #160]
    stp     x22, x23,  [sp, #176]
    stp     x24, x25,  [sp, #192]
    stp     x26, x27,  [sp, #208]
    stp     x28, x29,  [sp, #224]
    str     x30,       [sp, #240]
    mrs     x0, elr_el2
    mrs     x1, spsr_el2
    stp     x0, x1,    [sp, #248]

    mov     x0, sp
    bl      handle_test_gic_el2_irq

    ldp     x0, x1,    [sp, #248]
    msr     elr_el2, x0
    msr     spsr_el2, x1
    ldp     x0, x1,    [sp, #0]
    ldp     x2, x3,    [sp, #16]
    ldp     x4, x5,    [sp, #32]
    ldp     x6, x7,    [sp, #48]
    ldp     x8, x9,    [sp, #64]
    ldp     x10, x11,  [sp, #80]
    ldp     x12, x13,  [sp, #96]
    ldp     x14, x15,  [sp, #112]
    ldp     x16, x17,  [sp, #128]
    ldp     x18, x19,  [sp, #144]
    ldp     x20, x21,  [sp, #160]
    ldp     x22, x23,  [sp, #176]
    ldp     x24, x25,  [sp, #192]
    ldp     x26, x27,  [sp, #208]
    ldp     x28, x29,  [sp, #224]
    ldr     x30,       [sp, #240]
    add     sp, sp,    #272
    eret

test_gic_unhandled:
    b .
