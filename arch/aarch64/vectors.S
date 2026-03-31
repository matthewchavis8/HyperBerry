/**
 * @file vectors.S
 * @brief vector table for the HyperBerry
 *
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
  
  // Group 3: Hypervisor context
  .balign 0x80; b lower_el2_sync
  .balign 0x80; b lower_el2_irq
  .balign 0x80; b lower_el2_fiq
  .balign 0x80; b lower_el2_serror

  

  // Group 4: 32 Bit guest kernels (STUBBED for now)
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
  .balign 0x80; b el2_unhandled
