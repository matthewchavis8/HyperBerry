/**
 * @file guest_payload.S
 * @brief Minimal EL1 payload used by the GIC integration test.
 */

#if defined(BSP_QEMU)
#define GICV_BASE 0x08040000
#elif defined(BSP_RPI5)
#define GICV_BASE 0x107fffe000
#else
#error "Unsupported BSP for GIC guest payload"
#endif

#define GICV_IAR  0x00c
#define GICV_EOIR 0x010

.section .text.test_gic_guest_payload
.balign 0x800
.global test_gic_guest_entry
.global test_gic_guest_hvc
.global test_gic_guest_vectors

test_gic_guest_entry:
    ldr     x0, =test_gic_guest_vectors
    msr     vbar_el1, x0
    isb

    msr     daifclr, #2
    isb

    // Keep the guest finite on vIRQ delivery regressions. QEMU does not
    // reliably wake an already-pending GICV IRQ from WFI in this test shape.
    movz    x3, #0xffff
1:
    subs    x3, x3, #1
    b.ne    1b
    movz    x0, #0xffff
    hvc     #0
    b       .

.balign 0x800
test_gic_guest_vectors:
  // Group 1: Current EL with SP_EL0 (unused)
  .balign 0x80; b test_gic_guest_unhandled_100
  .balign 0x80; b test_gic_guest_unhandled_101
  .balign 0x80; b test_gic_guest_unhandled_102
  .balign 0x80; b test_gic_guest_unhandled_103

  // Group 2: Current EL with SP_ELx
  .balign 0x80; b test_gic_guest_unhandled_200
  .balign 0x80; b test_gic_guest_irq
  .balign 0x80; b test_gic_guest_unhandled_202
  .balign 0x80; b test_gic_guest_unhandled_203

  // Group 3: Lower EL using AArch64 (unused)
  .balign 0x80; b test_gic_guest_unhandled_300
  .balign 0x80; b test_gic_guest_unhandled_301
  .balign 0x80; b test_gic_guest_unhandled_302
  .balign 0x80; b test_gic_guest_unhandled_303

  // Group 4: Lower EL using AArch32 (unused)
  .balign 0x80; b test_gic_guest_unhandled_400
  .balign 0x80; b test_gic_guest_unhandled_401
  .balign 0x80; b test_gic_guest_unhandled_402
  .balign 0x80; b test_gic_guest_unhandled_403

test_gic_guest_irq:
    ldr     x3, =test_gic_guest_progress
    movz    w4, #1
    str     w4, [x3]
    ldr     x1, =GICV_BASE
    movz    w4, #2
    str     w4, [x3]
    ldr     w0, [x1, #GICV_IAR]
    movz    w4, #3
    str     w4, [x3]
    ldr     x2, =test_gic_guest_iar
    str     w0, [x2]
    movz    w4, #4
    str     w4, [x3]
    str     w0, [x1, #GICV_EOIR]
    movz    w4, #5
    str     w4, [x3]
    dsb     sy
    isb

test_gic_guest_hvc:
    hvc     #0
    b       .

.macro GUEST_UNHANDLED name, code
\name:
    ldr     x3, =test_gic_guest_progress
    movz    w4, #\code
    str     w4, [x3]
    movz    x0, #\code
    hvc     #0
    b       .
.endm

GUEST_UNHANDLED test_gic_guest_unhandled_100, 0x100
GUEST_UNHANDLED test_gic_guest_unhandled_101, 0x101
GUEST_UNHANDLED test_gic_guest_unhandled_102, 0x102
GUEST_UNHANDLED test_gic_guest_unhandled_103, 0x103
GUEST_UNHANDLED test_gic_guest_unhandled_200, 0x200
GUEST_UNHANDLED test_gic_guest_unhandled_202, 0x202
GUEST_UNHANDLED test_gic_guest_unhandled_203, 0x203
GUEST_UNHANDLED test_gic_guest_unhandled_300, 0x300
GUEST_UNHANDLED test_gic_guest_unhandled_301, 0x301
GUEST_UNHANDLED test_gic_guest_unhandled_302, 0x302
GUEST_UNHANDLED test_gic_guest_unhandled_303, 0x303
GUEST_UNHANDLED test_gic_guest_unhandled_400, 0x400
GUEST_UNHANDLED test_gic_guest_unhandled_401, 0x401
GUEST_UNHANDLED test_gic_guest_unhandled_402, 0x402
GUEST_UNHANDLED test_gic_guest_unhandled_403, 0x403

.section .bss.test_gic_guest_payload
.balign 4
.global test_gic_guest_progress
test_gic_guest_progress:
    .word 0

.balign 4
.global test_gic_guest_iar
test_gic_guest_iar:
    .word 0
