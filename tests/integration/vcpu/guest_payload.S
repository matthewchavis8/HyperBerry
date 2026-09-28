.section .text
.global guest_entry
guest_entry:
    mov x0, #0x1234
    mov x5, #0xbeef
    adr x6, guest_resume
    mov x7, sp
    mov x8, #0x1350
    msr sp_el0, x8
    mov x9, #0x2468
    msr tpidr_el1, x9
    hvc #0x42
guest_resume:
    mrs x10, tpidr_el1
    mrs x11, sp_el0
guest_repeat:
    add x12, x12, #1
    hvc #0x43
    b guest_repeat
