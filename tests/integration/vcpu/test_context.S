// @file test_context.S
// @brief Check the host calling convention across a real guest run.

.section .text
.global test_vcpu_host_registers
test_vcpu_host_registers:
    sub sp, sp, #96
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    mov x19, #19
    mov x20, #20
    mov x21, #21
    mov x22, #22
    mov x23, #23
    mov x24, #24
    mov x25, #25
    mov x26, #26
    mov x27, #27
    mov x28, #28
    mov x29, #29
    bl vcpu_run
    mov x0, #0
    cmp x19, #19
    b.ne 1f
    cmp x20, #20
    b.ne 1f
    cmp x21, #21
    b.ne 1f
    cmp x22, #22
    b.ne 1f
    cmp x23, #23
    b.ne 1f
    cmp x24, #24
    b.ne 1f
    cmp x25, #25
    b.ne 1f
    cmp x26, #26
    b.ne 1f
    cmp x27, #27
    b.ne 1f
    cmp x28, #28
    b.ne 1f
    cmp x29, #29
    b.ne 1f
    mov x0, #1
1:
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #96
    ret
