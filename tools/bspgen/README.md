# bspgen

Emits `regs.inc` — assembler-safe `#define`s for the peripheral addresses a
board's device tree declares — so the BSP header and `boot.S` share one
definition instead of each hardcoding a literal.

    bspgen.py --dtb <host.dtb> --board <name> --out <regs.inc>

Input is the board's *host* device tree: the real firmware blob for rpi5 or a
`dumpdtb` capture for qemu.
