/**
 * @file test_vectors.S
 * @brief Test-only EL2 vector table and guest stub for Vcpu integration tests.
 */

#include "core/vcpu/vcpu.h"

.section .text.test_vcpu_vectors
.balign 0x800
.global test_vcpu_vectors
.global test_vcpu_guest_entry
.global test_vcpu_guest_hvc
.global test_vcpu_guest_resume

test_vcpu_vectors:
  // Group 1: EL2/SP_EL0 (unused)
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled

  // Group 2: EL2/SP_EL2 (unused)
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled

  // Group 3: Lower EL / AArch64
  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    mrs x0, esr_el2
    b test_vcpu_exit_sync

  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled

  // Group 4: Lower EL / AArch32 (unused)
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled
  .balign 0x80; b test_vcpu_unhandled

test_vcpu_exit_sync:
    // x0 = ESR_EL2 from the vector entry.
    mrs     x2, tpidr_el2
    str     x0, [x2, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]

    // Mirror save_guest_state from arch/aarch64/vcpu.S so the test exercises
    // the real Vcpu layout and sysreg-save path before returning to the caller.
    mrs     x2, tpidr_el2
    add     x2, x2, #VCPU_GPREGS_OFFSET
    ldr     x3, [sp, #16]
    str     x3, [x2, #VCPU_GPREG_X0]
    ldr     x3, [sp, #24]
    str     x3, [x2, #VCPU_GPREG_X1]
    ldr     x3, [sp, #0]
    str     x3, [x2, #VCPU_GPREG_X2]
    ldr     x3, [sp, #8]
    str     x3, [x2, #VCPU_GPREG_X3]
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

    mrs     x0, tpidr_el2
    add     x1, x0, #VCPU_EL2STATE_OFFSET
    mrs     x2, elr_el2
    mrs     x3, spsr_el2
    stp     x2, x3, [x1, #VCPU_ELR_EL2]
    bl      vcpu_save_el1_sysregs

    mrs     x0, tpidr_el2
    ldr     x1, [x0, #(VCPU_HVCTX_OFFSET + VCPU_HVCTX_EXIT_ESR)]
    bl      handle_test_vcpu_guest_exit

    // Return to the original EL2 test caller via the HvContext saved by
    // vcpu_enter instead of routing back through the production handler loop.
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

test_vcpu_unhandled:
    b .

.section .text.test_vcpu_guest
.balign 4
test_vcpu_guest_entry:
    movz x0, #0x1234
    movz x5, #0xbeef
test_vcpu_guest_hvc:
    hvc #0
test_vcpu_guest_resume:
    b .
