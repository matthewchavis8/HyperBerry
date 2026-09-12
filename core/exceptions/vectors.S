/**
 * @file vectors.S
 * @brief EL2 exception vector table.
 * @ingroup exceptions
 *
 * Table installed in VBAR_EL2 by boot.S. 0x800-byte aligned, 0x80-byte entries.
 *
 * Group 1 (0x000): EL2 / SP_EL0       — stubbed
 * Group 2 (0x200): EL2 / SP_EL2       — active HV handlers
 * Group 3 (0x400): Lower EL / AArch64 — guest trap handlers
 * Group 4 (0x600): Lower EL / AArch32 — stubbed (32-bit guests unsupported)
 */

.section .text.vectors
.balign 0x800
.global el2_vectors

el2_vectors:
  // Group 1: EL2/SP_EL0 (stubbed)
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled

  // Group 2: EL2/SP_EL2
  .balign 0x80; b el2_sync
  .balign 0x80; b el2_irq
  .balign 0x80; b el2_fiq
  .balign 0x80; b el2_serror

  // Group 3: Lower EL / AArch64
  // x0–x3 pushed before branch; layout: sp+0=x2, sp+8=x3, sp+16=x0, sp+24=x1
  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    mrs x0, esr_el2
    b vcpu_exit_sync

  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    b vcpu_exit_irq

  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    b vcpu_exit_fiq

  .balign 0x80
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    mrs x0, esr_el2
    b vcpu_exit_serror

  // Group 4: Lower EL / AArch32 (stubbed)
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
