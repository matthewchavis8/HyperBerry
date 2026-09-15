.section .text
.global guest_entry
guest_entry:
    movz x0, #0x1234
    movz x5, #0xbeef
    adr x6, guest_resume
    hvc #0
guest_resume:
    b .
