/**
 * @file boot.S
 * @brief EL2 hypervisor bootstrap for Arm FVP Base RevC-2xAEMvA.
 * @ingroup core
 *
 * @par Boot Sequence
 * 1. Park non-zero MPIDR cores in a wfe loop.
 * 2. Inspect CurrentEL:
 *    - EL3: program SCR_EL3 / SPSR_EL3 / ELR_EL3 and ERET into the EL2 path.
 *    - EL2: jump straight into the EL2 path.
 *    - other: park.
 * 3. EL2 path mirrors the QEMU bring-up.
 *
 * @note FVP does not pass a device tree. parseDtb() will panic shortly after
 *       the first UART line in this milestone; that is expected.
 */

.section .text.start
.global _start

_start:
    mrs   x0, MPIDR_EL1
    and   x0, x0, #0xFF
    cbnz  x0, .Lblock

    mrs   x0, CurrentEL
    and   x0, x0, #0xC
    cmp   x0, #(3 << 2)
    b.eq  .Lel3_to_el2
    cmp   x0, #(2 << 2)
    b.eq  .Lel2_entry
    b     .Lblock

.Lel3_to_el2:
    mov   x0, #(1 << 0)
    orr   x0, x0, #(1 << 7)
    orr   x0, x0, #(1 << 8)
    orr   x0, x0, #(1 << 10)
    msr   SCR_EL3, x0

    mov   x0, #0x3C9
    msr   SPSR_EL3, x0

    adr   x0, .Lel2_entry
    msr   ELR_EL3, x0
    isb
    eret

.Lel2_entry:
    mov   x0, #(1 << 31)
    msr   HCR_EL2, x0

    mrs   x0, SCTLR_EL2
    bic   x0, x0, #(1 << 0)
    bic   x0, x0, #(1 << 1)
    bic   x0, x0, #(1 << 2)
    bic   x0, x0, #(1 << 12)
    msr   SCTLR_EL2, x0
    isb

    mrs   x0, CNTHCTL_EL2
    orr   x0, x0, #(1 << 0)
    orr   x0, x0, #(1 << 1)
    msr   CNTHCTL_EL2, x0
    msr   CNTVOFF_EL2, xzr

    mrs   x0, MIDR_EL1
    msr   VPIDR_EL2, x0
    mrs   x0, MPIDR_EL1
    msr   VMPIDR_EL2, x0

    ldr   x0, =__bss_start
    ldr   x1, =__bss_end
.Lbss_loop:
    cmp   x0, x1
    b.hs  .Lbss_done
    stp   xzr, xzr, [x0], #16
    b     .Lbss_loop
.Lbss_done:

    mov   x0, #1
    msr   SPSel, x0
    isb

    ldr   x0, =__stack_end
    mov   sp, x0

    adr   x0, el2_vectors
    msr   vbar_el2, x0
    isb

    ldr   x0, =0x88000000
    bl    hmain

.Lblock:
    wfe
    b    .Lblock
