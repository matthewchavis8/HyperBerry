// EL2 hypervisor entry point for BCM2712 / Cortex-A76.

.section .text.start
.global _start

_start:
    // Core 0 will handle the boot process. If it is not core 0 then we block those cores
    // until core 0 finishes setting up the hypervisor image
    mrs   x0, MPIDR_EL1  // Read AF1 into x0
    and   x0, x0, #0xFF  // mask AF1 to get the current core identity
    cbnz  x0, .Lblock    // If it is not core zero then block the core

    // Make sure that core 0 is running in EL2
    mrs   x0, CurrentEL // Check what EL the current cpu is running in
    and   x0, x0, #0xC  // mask acquire EL identity
    cmp   x0, #(2 << 2) // Check if it matches EL2
    b.ne  .Lblock

    // Here we are configuring the EL1 guest vms they must run in 64bit context not 32
    mov   x0, #(1 << 31)
    msr   HCR_EL2, x0

    // SCTLR_EL2: MMU, D-cache, I-cache off until page tables are ready.
    mrs   x0, SCTLR_EL2
    bic   x0, x0, #(1 << 0)      // Disable MMU
    bic   x0, x0, #(1 << 1)      // Cache allignment disabled
    bic   x0, x0, #(1 << 2)      // Data cache disabled
    bic   x0, x0, #(1 << 12)     // Instruction cache disabled
    msr   SCTLR_EL2, x0
    isb

    // CNTHCTL_EL2: grant EL1/EL0 access to physical timer and counter.
    mrs   x0, CNTHCTL_EL2
    orr   x0, x0, #(1 << 0)      // EL1PCTEN
    orr   x0, x0, #(1 << 1)      // EL1PCEN
    msr   CNTHCTL_EL2, x0
    msr   CNTVOFF_EL2, xzr

    // Shadow MIDR/MPIDR so guests read real CPU identity.
    mrs   x0, MIDR_EL1
    msr   VPIDR_EL2, x0
    mrs   x0, MPIDR_EL1
    msr   VMPIDR_EL2, x0

    // Zero BSS.
    ldr   x0, =__bss_start
    ldr   x1, =__bss_end

.Lbss_loop:
    cmp   x0, x1
    b.hs  .Lbss_done
    stp   xzr, xzr, [x0], #16
    b     .Lbss_loop
.Lbss_done:
    // Set up EL2 stack and enter hypervisor.
    ldr   x0, =__stack_end
    mov   sp, x0
    bl    hmain
.Lblock:
    wfe
    b    .Lblock
