/**
 * @file exceptions.S
 * @brief Exception handler entry points and context save/restore macros.
 * @ingroup exceptions
 *
 * Defines the low-level assembly glue between the EL2 vector table
 * (vectors.S) and the C++ handler functions (exceptions.cpp).
 * Each entry point saves the full CPU context to the stack using
 * @c save_context, passes a pointer to the resulting ExceptionContext
 * frame in x0, and branches to the corresponding C++ handler.
 *
 * @par Stack Frame Layout (272 bytes)
 * | Offset   | Content            |
 * |----------|--------------------|
 * | 0-224    | x0-x29 (GPRs)      |
 * | 240      | x30 (LR)           |
 * | 248      | ELR_EL2            |
 * | 256      | SPSR_EL2           |
 *
 * @note The @c restore_context macro ends with @c eret, returning
 *       execution to the faulting address in ELR_EL2.
 * @warning The @c el2_unhandled entry point spins forever after
 *          calling handle_unhandled() -- it never erets.
 */

.section .text

.macro save_context
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
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30,      [sp, #240]
    mrs     x0, elr_el2
    mrs     x1, spsr_el2
    stp     x0,  x1,  [sp, #248]
.endm

.macro restore_context
    ldp     x0,  x1,  [sp, #248]
    msr     elr_el2, x0
    msr     spsr_el2, x1
    ldp     x0,  x1,  [sp, #0]
    ldp     x2,  x3,  [sp, #16]
    ldp     x4,  x5,  [sp, #32]
    ldp     x6,  x7,  [sp, #48]
    ldp     x8,  x9,  [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30,      [sp, #240]
    add     sp, sp, #272
    eret
.endm

// EL2 (Hypervisor) handlers
.global el2_sync
.global el2_irq
.global el2_fiq
.global el2_serror

el2_sync:
    save_context
    mov     x0, sp
    mrs     x1, esr_el2
    bl      handle_el2_sync
    restore_context

el2_irq:
    save_context
    mov     x0, sp
    bl      handle_el2_irq
    restore_context

el2_fiq:
    save_context
    mov     x0, sp
    bl      handle_el2_fiq
    restore_context

el2_serror:
    save_context
    mov     x0, sp
    mrs     x1, esr_el2
    bl      handle_el2_serror
    restore_context

// Catch-all for stubbed groups
.global el2_unhandled

el2_unhandled:
    save_context
    mov     x0, sp
    mrs     x1, esr_el2
    bl      handle_unhandled
    b       .                   // spin forever
