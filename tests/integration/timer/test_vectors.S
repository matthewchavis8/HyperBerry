/**
 * @file test_vectors.S
 * @brief Test-only EL2 vector table for physical timer IRQ coverage.
 */

.section .text.test_timer_vectors
.balign 0x800
.global test_timer_vectors

test_timer_vectors:
  // Group 1: Current EL with SP_EL0 (unused)
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled

  // Group 2: Current EL with SP_ELx
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_el2_irq
  .balign 0x80; b test_timer_el2_irq
  .balign 0x80; b test_timer_unhandled

  // Group 3: Lower EL using AArch64 (unused)
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled

  // Group 4: Lower EL using AArch32 (unused)
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled
  .balign 0x80; b test_timer_unhandled

test_timer_el2_irq:
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
    bl      handle_test_timer_el2_irq

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

test_timer_unhandled:
    b .
