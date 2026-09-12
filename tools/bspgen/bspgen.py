#!/usr/bin/env python3
"""Emit address defines from a board's host device tree.

The tree is the source of truth for peripheral addresses, but a few of them are
needed as compile time constants: the early console has to work before the tree
has been parsed, and the GIC register tables are constexpr. This reads the
board's host DTB and writes a regs.inc of plain #defines covering those.
"""

import argparse
import os
import struct
import sys

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE, FDT_END_NODE, FDT_PROP, FDT_NOP, FDT_END = 1, 2, 3, 4, 9


class Node:
    def __init__(self, name, parent):
        self.name = name
        self.parent = parent
        self.children = []
        self.props = {}

    @property
    def path(self):
        if self.parent is None:
            return "/"
        prefix = self.parent.path
        return (prefix if prefix.endswith("/") else prefix + "/") + self.name

    def compatible(self):
        raw = self.props.get("compatible")
        if raw is None:
            return []
        return [s.decode("ascii", "replace") for s in raw.split(b"\0") if s]

    def cell(self, name, default):
        raw = self.props.get(name)
        if raw is None or len(raw) < 4:
            return default
        return struct.unpack(">I", raw[:4])[0]

    def walk(self):
        yield self
        for child in self.children:
            yield from child.walk()


def parse_dtb(blob):
    magic, _total, struct_off, strings_off = struct.unpack(">IIII", blob[:16])
    if magic != FDT_MAGIC:
        raise SystemExit(f"not a DTB: magic {magic:#x}")
    size_structs = struct.unpack(">I", blob[36:40])[0]
    body = blob[struct_off:struct_off + size_structs]
    strings = blob[strings_off:]

    root = None
    current = None
    pos = 0
    while pos < len(body):
        (token,) = struct.unpack(">I", body[pos:pos + 4])
        pos += 4

        if token == FDT_BEGIN_NODE:
            end = body.index(b"\0", pos)
            name = body[pos:end].decode("ascii", "replace")
            pos = (end + 4) & ~3
            node = Node(name, current)
            if root is None:
                root = node
            else:
                current.children.append(node)
            current = node

        elif token == FDT_END_NODE:
            current = current.parent

        elif token == FDT_PROP:
            length, name_off = struct.unpack(">II", body[pos:pos + 8])
            pos += 8
            key_end = strings.index(b"\0", name_off)
            key = strings[name_off:key_end].decode("ascii", "replace")
            current.props[key] = bytes(body[pos:pos + length])
            pos = (pos + length + 3) & ~3

        elif token == FDT_NOP:
            continue

        elif token == FDT_END:
            break

        else:
            raise SystemExit(f"bad FDT token {token} at {pos - 4}")

    return root


def read_reg(node):
    """Decode a node's reg into (base, size) pairs, honouring the parent's cells."""
    raw = node.props.get("reg")
    if raw is None:
        return []

    parent = node.parent
    addr_cells = parent.cell("#address-cells", 2) if parent else 2
    size_cells = parent.cell("#size-cells", 1) if parent else 1

    stride = (addr_cells + size_cells) * 4
    if stride == 0 or len(raw) % stride:
        return []

    out = []
    for off in range(0, len(raw), stride):
        entry = raw[off:off + stride]
        base = cells_to_int(entry[:addr_cells * 4])
        out.append((translate(node, base), cells_to_int(entry[addr_cells * 4:])))
    return out


def cells_to_int(chunk):
    value = 0
    for i in range(0, len(chunk), 4):
        value = (value << 32) | struct.unpack(">I", chunk[i:i + 4])[0]
    return value


def translate(node, addr):
    """Map a bus-local address up to CPU-physical via each parent's ranges.

    Peripherals on the Pi live under /soc@..., whose ranges adds the high bits;
    without this the generated constants are short by that offset.
    """
    bus = node.parent
    while bus is not None and bus.parent is not None:
        raw = bus.props.get("ranges")
        if raw is None:
            bus = bus.parent
            continue
        if len(raw) == 0:      # empty ranges: identity mapping
            bus = bus.parent
            continue

        child_cells = bus.cell("#address-cells", 2)
        parent_cells = bus.parent.cell("#address-cells", 2)
        size_cells = bus.cell("#size-cells", 1)
        stride = (child_cells + parent_cells + size_cells) * 4
        if stride == 0 or len(raw) % stride:
            bus = bus.parent
            continue

        for off in range(0, len(raw), stride):
            entry = raw[off:off + stride]
            child = cells_to_int(entry[:child_cells * 4])
            parent = cells_to_int(entry[child_cells * 4:(child_cells + parent_cells) * 4])
            length = cells_to_int(entry[(child_cells + parent_cells) * 4:])
            if child <= addr < child + length:
                addr = addr - child + parent
                break

        bus = bus.parent
    return addr


def find(root, compatibles):
    for node in root.walk():
        have = node.compatible()
        for want in compatibles:
            if want in have:
                return node
    return None


# reg layout differs by GIC architecture version: v2 is
# GICD/GICC/GICH/GICV, v3 is GICD/GICR with no memory-mapped CPU interface
# unless the model also exposes the legacy window (extra reg entries).
GICV2_REGIONS = [("DISTRIBUTOR", 0), ("CPU", 1), ("HV", 2), ("VCPU", 3)]
GICV3_REGIONS = [("DISTRIBUTOR", 0), ("REDISTRIBUTOR", 1)]

GICV2_COMPATIBLE = ["arm,gic-400", "arm,cortex-a15-gic", "arm,gic-v2", "arm,arm11mp-gic"]
GICV3_COMPATIBLE = ["arm,gic-v3"]
GIC_COMPATIBLE = GICV2_COMPATIBLE + GICV3_COMPATIBLE
UART_COMPATIBLE = ["arm,pl011", "brcm,bcm2835-aux-uart"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dtb", required=True)
    ap.add_argument("--board", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    with open(args.dtb, "rb") as fh:
        root = parse_dtb(fh.read())

    defines = []
    notes = []

    gic = find(root, GIC_COMPATIBLE)
    if gic is None:
        notes.append("no GIC node matched")
    else:
        regions = read_reg(gic)
        have = gic.compatible()
        v3 = any(c in GICV3_COMPATIBLE for c in have)
        layout = GICV3_REGIONS if v3 else GICV2_REGIONS
        notes.append(f"GIC {gic.path} ({', '.join(have)}) -> {'v3' if v3 else 'v2'} layout")
        if v3:
            notes.append("GICv3: no memory-mapped CPU/HV/VCPU interface in reg")
        # GIC_BASE is deliberately not generated: on GIC-400 it is the
        # enclosing block base (GICD - 0x1000), which the DTB does not express.
        for suffix, index in layout:
            if index < len(regions):
                base, size = regions[index]
                defines.append((f"BSP_GIC_{suffix}_BASE", base))
                defines.append((f"BSP_GIC_{suffix}_SIZE", size))

    uart = find(root, UART_COMPATIBLE)
    if uart is None:
        notes.append("no UART node matched")
    else:
        regions = read_reg(uart)
        notes.append(f"UART {uart.path} ({', '.join(uart.compatible())})")
        if regions:
            defines.append(("BSP_UART_BASE", regions[0][0]))
            defines.append(("BSP_UART_SIZE", regions[0][1]))

    if not defines:
        raise SystemExit(f"bspgen: nothing extracted from {args.dtb}")

    width = max(len(name) for name, _ in defines)
    lines = [
        "/**",
        f" * @file bsp/{args.board}/regs.inc",
        f" * @brief Generated peripheral addresses for {args.board}. Do not edit.",
        " * @ingroup bsp",
        " *",
        f" * Generated by tools/bspgen from {args.dtb}.",
    ]
    lines += [f" *   {n}" for n in notes]
    lines += [
        " */",
        "",
        f"#ifndef __BSP_{args.board.upper()}_REGS_INC__",
        f"#define __BSP_{args.board.upper()}_REGS_INC__",
        "",
    ]
    lines += [f"#define {name.ljust(width)} {value:#x}ULL" for name, value in defines]
    lines += ["", f"#endif // __BSP_{args.board.upper()}_REGS_INC__", ""]

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    with open(args.out, "w") as fh:
        fh.write("\n".join(lines))

    print(f"[bspgen] {args.board}: {len(defines)} defines -> {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
