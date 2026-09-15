// @file dtb.cpp
// @brief Flattened Device Tree parser used during early boot.
// @ingroup core

#include "dtb.h"
#include "fdt.h"
#include <stdint.h>

// @brief Decode a 64-bit base/size pair from a DTB `reg` property.
// @param data Pointer to the first 32-bit cell of the property payload.
// @param base Output physical base address.
// @param size Output region size in bytes.
//
// Uses four 32-bit big-endian cells: two address cells followed by two
// size cells. The @c volatile qualifier prevents the compiler from
// widening accesses on Device-nGnRnE memory before the MMU is enabled.
static void readReg64(const volatile uint32_t* data, uint64_t& base, uint64_t& size) {
    base = (static_cast<uint64_t>(Be32(data[0])) << 32) | static_cast<uint64_t>(Be32(data[1]));
    size = (static_cast<uint64_t>(Be32(data[2])) << 32) | static_cast<uint64_t>(Be32(data[3]));
}

static uint64_t readU64Cells(const volatile uint32_t* data) {
    return (static_cast<uint64_t>(Be32(data[0])) << 32) | static_cast<uint64_t>(Be32(data[1]));
}

static uint64_t readInitrdAddress(const volatile uint32_t* data, uint32_t dataLen) {
    if (dataLen >= 8) return readU64Cells(data);

    return static_cast<uint64_t>(Be32(data[0]));
}

// All DTB pointers use volatile to prevent the compiler from widening
// 32-bit reads into larger accesses. With the MMU disabled on Cortex-A76,
// the DTB resides in Device-nGnRnE memory where natural alignment must be
// respected, and the DTB format only guarantees 4-byte alignment.
MemoryMap ParseDtb(uintptr_t dtb) {
    MemoryMap map {};

    if (dtb == 0) return map;

    const volatile FdtHeader* hdr { reinterpret_cast<const volatile FdtHeader*>(dtb) };

    if (Be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC)) return map;

    map.dtbBase = dtb;
    map.dtbSize = Be32(hdr->totalSize);

    const volatile uint32_t* structs { reinterpret_cast<const volatile uint32_t*>(
            dtb + Be32(hdr->structOff)) };
    const char* strings { reinterpret_cast<const char*>(dtb + Be32(hdr->stringsOff)) };

    bool foundMem { false };
    bool foundAtf { false };

    bool inReservedMemory { false };
    bool inMemory { false };
    bool inAtf { false };
    bool inChosen { false };

    int depth {};

    const volatile uint32_t* tok { structs };

    while (true) {
        uint32_t token { Be32(*tok) };
        tok++;

        switch (static_cast<FDT>(token)) {
            case FDT::BEGIN_NODE: {
                const char* name { reinterpret_cast<const char*>(
                        const_cast<const uint32_t*>(tok)) };
                const uint8_t* nameB { reinterpret_cast<const uint8_t*>(
                        const_cast<const uint32_t*>(tok)) };

                // depth == 1: inside root "/", entering a top-level node
                if (depth == 1) {
                    inMemory = StrStartsWith(name, "memory");
                    inReservedMemory = StrEq(name, "reserved-memory");
                    inChosen = StrEq(name, "chosen");
                    // depth == 2: inside reserved-memory, entering a child node
                } else if (depth == 2 && inReservedMemory) {
                    inAtf = StrEq(name, "atf") || StrEq(name, "bl31") || StrEq(name, "secmon") ||
                            StrEq(name, "optee") || StrEq(name, "tee");
                }

                uint32_t nameLen {};
                while (nameB[nameLen] != 0)
                    nameLen++;

                tok = reinterpret_cast<const volatile uint32_t*>(FdtAlign(nameB, nameLen + 1));
                depth++;
                break;
            }

            case FDT::END_NODE: {
                depth--;
                if (depth == 1) {
                    inMemory = false;
                    inReservedMemory = false;
                    inChosen = false;
                } else if (depth == 2) {
                    inAtf = false;
                }

                if (foundMem && foundAtf) goto done;
                break;
            }

            case FDT::PROP: {
                uint32_t dataLen { Be32(tok[0]) };
                uint32_t nameOff { Be32(tok[1]) };
                const char* propName { strings + nameOff };
                const volatile uint32_t* propData { tok + 2 }; // skip dataLen + nameOff

                if (StrEq(propName, "reg") && dataLen >= 16) {
                    if (inMemory && !foundMem) {
                        readReg64(propData, map.memBase, map.memSize);
                        foundMem = true;
                    } else if (inAtf && !foundAtf) {
                        readReg64(propData, map.atfBase, map.atfSize);
                        foundAtf = true;
                    }
                } else if (inChosen && dataLen >= 4) {
                    if (StrEq(propName, "linux,initrd-start")) {
                        map.bootPackageBase = readInitrdAddress(propData, dataLen);
                    } else if (StrEq(propName, "linux,initrd-end")) {
                        uint64_t end { readInitrdAddress(propData, dataLen) };
                        if (end > map.bootPackageBase)
                            map.bootPackageSize = end - map.bootPackageBase;
                    }
                }

                tok = reinterpret_cast<const volatile uint32_t*>(
                        FdtAlign(const_cast<const uint32_t*>(propData), dataLen));
                break;
            }

            case FDT::NOP:
                break;

            case FDT::END:
                goto done;

            default:
                return map;
        }
    }

done:
    map.isValid = foundMem;
    return map;
}

// ---------------------------------------------------------------------------
// Compatible-string lookup
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t MAX_DEPTH { 16 };

// Per-depth bus context needed to decode and translate a child's `reg`.
struct BusLevel {
    uint32_t addressCells;
    uint32_t sizeCells;
    const volatile uint32_t* ranges;
    uint32_t rangesLen;
    bool matched;
    const volatile uint32_t* reg;
    uint32_t regLen;
};

uint64_t readCells(const volatile uint32_t* data, uint32_t cells) {
    uint64_t value { 0 };
    for (uint32_t i { 0 }; i < cells; ++i)
        value = (value << 32) | Be32(data[i]);

    return value;
}

bool compatibleMatches(const char* list, uint32_t len, const char* const* wanted, uint32_t count) {
    uint32_t off { 0 };
    while (off < len) {
        const char* entry { list + off };
        for (uint32_t i { 0 }; i < count; ++i) {
            if (StrEq(entry, wanted[i])) return true;
        }

        while (off < len && list[off] != 0)
            off++;
        off++;
    }

    return false;
}

// Walk a bus-local address up through each ancestor's `ranges`.
uint64_t translate(const BusLevel* stack, uint32_t depth, uint64_t addr) {
    // stack[depth] is the matched node; its parents are below it.
    for (uint32_t level { depth }; level > 0; --level) {
        const BusLevel& bus { stack[level - 1] };
        if (bus.ranges == nullptr || bus.rangesLen == 0 || level < 2) continue;

        uint32_t childCells { bus.addressCells };
        uint32_t parentCells { stack[level - 2].addressCells };
        uint32_t sizeCells { bus.sizeCells };
        uint32_t stride { (childCells + parentCells + sizeCells) * 4 };
        if (stride == 0 || (bus.rangesLen % stride) != 0) continue;

        for (uint32_t off { 0 }; off + stride <= bus.rangesLen; off += stride) {
            const volatile uint32_t* entry { bus.ranges + (off / 4) };
            uint64_t child { readCells(entry, childCells) };
            uint64_t parent { readCells(entry + childCells, parentCells) };
            uint64_t length { readCells(entry + childCells + parentCells, sizeCells) };
            if (addr >= child && addr - child < length) {
                addr = addr - child + parent;
                break;
            }
        }
    }

    return addr;
}

} // namespace

DeviceNode DtbFindCompatible(uintptr_t dtb, const char* const* compatibles, uint32_t count) {
    DeviceNode out {};

    if (dtb == 0 || compatibles == nullptr || count == 0) return out;

    const volatile FdtHeader* hdr { reinterpret_cast<const volatile FdtHeader*>(dtb) };
    if (Be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC)) return out;

    const volatile uint32_t* tok { reinterpret_cast<const volatile uint32_t*>(
            dtb + Be32(hdr->structOff)) };
    const char* strings { reinterpret_cast<const char*>(dtb + Be32(hdr->stringsOff)) };

    BusLevel stack[MAX_DEPTH] {};
    uint32_t depth { 0 };

    while (true) {
        uint32_t token { Be32(*tok) };
        tok++;

        switch (static_cast<FDT>(token)) {
            case FDT::BEGIN_NODE: {
                const uint8_t* nameB { reinterpret_cast<const uint8_t*>(
                        const_cast<const uint32_t*>(tok)) };
                uint32_t nameLen { 0 };
                while (nameB[nameLen] != 0)
                    nameLen++;
                tok = reinterpret_cast<const volatile uint32_t*>(FdtAlign(nameB, nameLen + 1));

                if (depth < MAX_DEPTH) {
                    // Defaults per the DT spec when a node omits the cells.
                    stack[depth] = { 2, 1, nullptr, 0, false, nullptr, 0 };
                }
                depth++;
                break;
            }

            case FDT::END_NODE: {
                if (depth > 0) depth--;

                if (depth < MAX_DEPTH && stack[depth].matched) {
                    const BusLevel& node { stack[depth] };
                    uint32_t addressCells { depth > 0 ? stack[depth - 1].addressCells : 2 };
                    uint32_t sizeCells { depth > 0 ? stack[depth - 1].sizeCells : 1 };
                    uint32_t stride { (addressCells + sizeCells) * 4 };

                    if (node.reg != nullptr && stride != 0 && (node.regLen % stride) == 0) {
                        for (uint32_t off { 0 };
                                off + stride <= node.regLen && out.regionCount < DT_MAX_REGIONS;
                                off += stride) {
                            const volatile uint32_t* entry { node.reg + (off / 4) };
                            uint64_t base { readCells(entry, addressCells) };
                            out.regions[out.regionCount].base = translate(stack, depth, base);
                            out.regions[out.regionCount].size =
                                    readCells(entry + addressCells, sizeCells);
                            out.regionCount++;
                        }
                        out.found = true;
                        return out;
                    }
                }
                break;
            }

            case FDT::PROP: {
                uint32_t dataLen { Be32(tok[0]) };
                uint32_t nameOff { Be32(tok[1]) };
                const char* propName { strings + nameOff };
                const volatile uint32_t* propData { tok + 2 };
                uint32_t level { depth > 0 ? depth - 1 : 0 };

                if (level < MAX_DEPTH) {
                    BusLevel& node { stack[level] };
                    if (StrEq(propName, "#address-cells") && dataLen >= 4) {
                        node.addressCells = Be32(propData[0]);
                    } else if (StrEq(propName, "#size-cells") && dataLen >= 4) {
                        node.sizeCells = Be32(propData[0]);
                    } else if (StrEq(propName, "ranges")) {
                        node.ranges = propData;
                        node.rangesLen = dataLen;
                    } else if (StrEq(propName, "reg")) {
                        node.reg = propData;
                        node.regLen = dataLen;
                    } else if (StrEq(propName, "compatible") && dataLen > 0) {
                        const char* list { reinterpret_cast<const char*>(
                                const_cast<const uint32_t*>(propData)) };
                        node.matched = compatibleMatches(list, dataLen, compatibles, count);
                    }
                }

                tok = reinterpret_cast<const volatile uint32_t*>(
                        FdtAlign(const_cast<const uint32_t*>(propData), dataLen));
                break;
            }

            case FDT::NOP:
                break;

            case FDT::END:
                return out;

            default:
                return out;
        }
    }
}

namespace {

const char* const kUartCompatible[] { "arm,pl011", "brcm,bcm2835-aux-uart" };

const char* const kGicCompatible[] {
    "arm,gic-400",
    "arm,cortex-a15-gic",
    "arm,gic-v2",
    "arm,arm11mp-gic",
    "arm,gic-v3",
};

} // namespace

DeviceNode DtbFindUart(uintptr_t dtb) {
    return DtbFindCompatible(dtb, kUartCompatible, 2);
}

DeviceNode DtbFindGic(uintptr_t dtb) {
    return DtbFindCompatible(dtb, kGicCompatible, 5);
}
