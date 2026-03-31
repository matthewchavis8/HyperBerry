/**
 * @file vectors.S
 * @brief EL2 exception vector table for HyperBerry.
 * @ingroup exceptions
 *
 * Defines the 16-entry vector table installed in VBAR_EL2 by boot.S.
 * The table is aligned to 0x800 bytes with each entry spaced at 0x80
 * bytes, as required by the ARMv8-A architecture.
 *
 * @par Vector Table Layout
 * - **Group 1 (offset 0x000):** Current EL with SP_EL0 -- stubbed,
 *   branches to @c el2_unhandled.
 * - **Group 2 (offset 0x200):** Current EL with SP_ELx --
 *   active hypervisor handlers (sync, IRQ, FIQ, SError).
 * - **Group 3 (offset 0x400):** Lower EL using AArch64 --
 *   guest trap handlers (sync, IRQ, FIQ, SError).
 * - **Group 4 (offset 0x600):** Lower EL using AArch32 -- stubbed,
 *   branches to @c el2_unhandled (32-bit guests unsupported).
 *
 * @note Each entry is a single branch instruction. The actual
 *       save/restore logic lives in exceptions.S.
 */

.section .text.vectors
.balign 0x800
.global el2_vectors

el2_vectors:
  // Group 1: User space (STUBBED for now)
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled

  // Group 2: Hypervisor context
  .balign 0x80; b el2_sync
  .balign 0x80; b el2_irq
  .balign 0x80; b el2_fiq
  .balign 0x80; b el2_serror
  
  // Group 3: Lower EL using AArch64
  .balign 0x80; b lower_el_sync
  .balign 0x80; b lower_el_irq
  .balign 0x80; b lower_el_fiq
  .balign 0x80; b lower_el_serror

  

  // Group 4: 32 Bit guest kernels (STUBBED for now)
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
