.section .text
.global guest_entry
guest_entry:
    movz x0, #0xdead, lsl #16
    ldr x1, [x0]
    hvc #0x42
