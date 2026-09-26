// @file deviceTree.cpp
// @brief Flattened Device Tree parser used during early boot.
// @ingroup core

#include "deviceTree.h"
#include "fdt.h"
#include "regs.inc"
#include "lib/log/log.h"
#include "drivers/uart/uart.h"
#include "drivers/gic/gic.h"
#include "lib/panic/panic.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <span>

namespace {

using Cell = const volatile uint32_t;

constexpr uint32_t MAX_DEPTH { 16 };
constexpr uint32_t DEFAULT_ADDRESS_CELLS { 2 };
constexpr uint32_t DEFAULT_SIZE_CELLS { 1 };
constexpr uint32_t CELL_SIZE_BYTES { 4 };

alignas(16) constexpr std::string_view kMemory { "memory" };
alignas(16) constexpr std::string_view kReservedMemory { "reserved-memory" };
alignas(16) constexpr std::string_view kChosen { "chosen" };
alignas(16) constexpr std::string_view kAtf { "atf" };
alignas(16) constexpr std::string_view kBl31 { "bl31" };
alignas(16) constexpr std::string_view kSecmon { "secmon" };
alignas(16) constexpr std::string_view kOptee { "optee" };
alignas(16) constexpr std::string_view kTee { "tee" };
alignas(16) constexpr std::string_view kReg { "reg" };
alignas(16) constexpr std::string_view kInitrdStart { "linux,initrd-start" };
alignas(16) constexpr std::string_view kInitrdEnd { "linux,initrd-end" };
alignas(16) constexpr std::string_view kAddressCells { "#address-cells" };
alignas(16) constexpr std::string_view kSizeCells { "#size-cells" };
alignas(16) constexpr std::string_view kRanges { "ranges" };
alignas(16) constexpr std::string_view kCompatible { "compatible" };

using ByteSpan = std::span<const volatile uint8_t>;

// Combine big endian Device Tree cells into one host endian number.
uint64_t getCombinedCell(std::span<Cell> cells) {
    uint64_t combinedCell {};

    for (Cell cell : cells) {
        combinedCell = (combinedCell << 32) | Be32(cell);
    }

    return combinedCell;
}

bool stringStartsWith(ByteSpan actual, std::string_view expected) {
    if (actual.size() < expected.size())
        return false;

    for (size_t i {}; i < expected.size(); ++i) {
        if (actual[i] != static_cast<uint8_t>(expected[i]))
            return false;
    }

    return true;
}

bool stringEquals(ByteSpan actual, std::string_view expected) {
    return actual.size() > expected.size() && stringStartsWith(actual, expected) &&
            actual[expected.size()] == 0;
}

bool matchesCompatible(ByteSpan list, std::span<const std::string_view> compatibleCandidates) {
    size_t off {};

    while (off < list.size()) {
        ByteSpan remaining { list.subspan(off) };
        size_t entryLen {};

        while (entryLen < remaining.size() && remaining[entryLen] != 0) {
            ++entryLen;
        }

        for (size_t i {}; i < compatibleCandidates.size(); ++i) {
            const std::string_view candidate { compatibleCandidates[i] };

            if (candidate.size() != entryLen)
                continue;

            bool isMatch { true };

            for (uint32_t i {}; i < entryLen; ++i) {
                if (remaining[i] != candidate[i]) {
                    isMatch = false;
                    break;
                }
            }

            if (isMatch)
                return true;
        }

        off += entryLen < remaining.size() ? entryLen + 1 : entryLen;
    }

    return false;
}

struct BusLevel {
    uint32_t addressCells;
    uint32_t sizeCells;
    std::span<Cell> ranges;
    bool matched;
    std::span<Cell> reg;
};

struct MemoryParseState {
    bool memory {};
    bool reserved {};
    bool atf {};
    bool chosen {};
    bool foundMemory {};
    uint64_t initrdStart {};
    uint64_t initrdEnd {};
    bool hasInitrdStart {};
    bool hasInitrdEnd {};
    uint32_t depth {};
};

uint64_t translate(const BusLevel* stack, uint32_t depth, uint64_t addr) {
    for (uint32_t level { depth }; level > 0; --level) {
        const BusLevel& bus { stack[level - 1] };

        if (bus.ranges.empty() || level < 2)
            continue;

        uint32_t childCells { bus.addressCells };
        uint32_t parentCells { stack[level - 2].addressCells };
        uint32_t stride { (childCells + parentCells + bus.sizeCells) * CELL_SIZE_BYTES };

        if (stride == 0 || bus.ranges.size_bytes() % stride != 0)
            continue;

        for (uint32_t off {}; off + stride <= bus.ranges.size_bytes(); off += stride) {
            std::span<Cell> entry { bus.ranges.subspan(off / CELL_SIZE_BYTES) };
            uint64_t child { getCombinedCell(entry.subspan(0, childCells)) };
            uint64_t parent { getCombinedCell(entry.subspan(childCells, parentCells)) };
            uint64_t length { getCombinedCell(
                    entry.subspan(childCells + parentCells, bus.sizeCells)) };

            if (addr >= child && addr - child < length) {
                addr = addr - child + parent;
                break;
            }
        }
    }

    return addr;
}

alignas(16) constexpr std::array<std::string_view, 2> kUartCompatible {
    "arm,pl011",
    "brcm,bcm2835-aux-uart",
};

alignas(16) constexpr std::array<std::string_view, 5> kGicCompatible {
    "arm,gic-400",
    "arm,cortex-a15-gic",
    "arm,gic-v2",
    "arm,arm11mp-gic",
    "arm,gic-v3",
};

} // namespace

void TreeParser::validateHeader() const {
    if (m_dtb == 0)
        HvPanic("[ERROR][DTB] null device tree");

    const volatile FdtHeader* hdr { reinterpret_cast<const volatile FdtHeader*>(m_dtb) };
    uint32_t total { Be32(hdr->totalSize) };
    uint32_t structOff { Be32(hdr->structOff) };
    uint32_t structSize { Be32(hdr->sizeStructs) };
    uint32_t stringsOff { Be32(hdr->stringsOff) };
    uint32_t stringsSize { Be32(hdr->sizeStrings) };
    auto inBounds = [total](uint32_t off, uint32_t size) {
        return off <= total && size <= total - off;
    };

    if (Be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC) || total < sizeof(FdtHeader) ||
            !inBounds(structOff, structSize) || !inBounds(stringsOff, stringsSize) ||
            !inBounds(Be32(hdr->memRsvMapOff), 16))
        HvPanic("[ERROR][DTB] invalid device tree header");

    const uintptr_t end { m_dtb + total };
    const volatile uint8_t* cursor { reinterpret_cast<const volatile uint8_t*>(m_dtb + structOff) };
    const volatile uint8_t* structEnd { cursor + structSize };
    uint32_t depth {};
    bool hasEnded {};

    while (cursor < structEnd) {
        if (structEnd - cursor < 4)
            HvPanic("[ERROR][DTB] truncated structure block");

        FDT token { static_cast<FDT>(Be32(*reinterpret_cast<const volatile uint32_t*>(cursor))) };
        cursor += 4;

        if (hasEnded)
            HvPanic("[ERROR][DTB] data follows structure end");

        if (token == FDT::BEGIN_NODE) {
            const volatile uint8_t* name { cursor };

            while (cursor < structEnd && *cursor != 0) {
                ++cursor;
            }

            if (cursor == structEnd)
                HvPanic("[ERROR][DTB] unterminated node name");

            ++cursor;
            cursor = reinterpret_cast<const volatile uint8_t*>(FdtAlign(cursor, 0));
            ++depth;

            if (depth > MAX_DEPTH)
                HvPanic("[ERROR][DTB] device tree nesting is too deep");

            (void)name;
        } else if (token == FDT::END_NODE) {
            if (depth == 0)
                HvPanic("[ERROR][DTB] unmatched end node");

            --depth;
        } else if (token == FDT::PROP) {
            if (structEnd - cursor < 8)
                HvPanic("[ERROR][DTB] truncated property header");

            uint32_t len { Be32(*reinterpret_cast<const volatile uint32_t*>(cursor)) };
            uint32_t nameOff { Be32(*reinterpret_cast<const volatile uint32_t*>(cursor + 4)) };

            if (nameOff >= stringsSize)
                HvPanic("[ERROR][DTB] invalid property name");

            const volatile char* string { reinterpret_cast<const volatile char*>(
                    m_dtb + stringsOff + nameOff) };
            uint32_t remaining { stringsSize - nameOff };
            uint32_t i {};

            while (i < remaining && string[i] != 0) {
                ++i;
            }

            if (i == remaining || len > static_cast<uint32_t>(structEnd - cursor - 8))
                HvPanic("[ERROR][DTB] malformed property");

            cursor += 8 + ((len + 3) & ~3U);

            if (cursor > structEnd)
                HvPanic("[ERROR][DTB] truncated property data");
        } else if (token == FDT::NOP) {
        } else if (token == FDT::END) {
            if (depth != 0)
                HvPanic("[ERROR][DTB] unclosed node");

            hasEnded = true;
        } else {
            HvPanic("[ERROR][DTB] invalid structure token");
        }
    }

    if (!hasEnded || cursor != structEnd || end < m_dtb)
        HvPanic("[ERROR][DTB] malformed structure block");
}

MemoryMap TreeParser::ParseMemoryMap() const {
    validateHeader();

    MemoryMap map { .dtbBase = m_dtb };
    const volatile FdtHeader* hdr { reinterpret_cast<const volatile FdtHeader*>(m_dtb) };
    map.dtbSize = Be32(hdr->totalSize);
    const volatile uint32_t* tok { reinterpret_cast<const volatile uint32_t*>(
            m_dtb + Be32(hdr->structOff)) };
    const volatile uint8_t* strings { reinterpret_cast<const volatile uint8_t*>(
            m_dtb + Be32(hdr->stringsOff)) };
    const volatile uint8_t* structEnd { reinterpret_cast<const volatile uint8_t*>(
            m_dtb + Be32(hdr->structOff) + Be32(hdr->sizeStructs)) };
    MemoryParseState state {};

    // Decode a one or two cell initial RAM disk physical address.
    auto getInitrdAddress = [](std::span<Cell> cells) {
        return cells.size() == 2 ? getCombinedCell(cells) : Be32(cells[0]);
    };

    while (true) {
        switch (static_cast<FDT>(Be32(*tok++))) {
            case FDT::BEGIN_NODE: {
                const volatile uint8_t* name { reinterpret_cast<const volatile uint8_t*>(tok) };
                uint32_t len {};

                while (name + len < structEnd && name[len] != 0) {
                    ++len;
                }

                if (name + len == structEnd)
                    HvPanic("[ERROR][DTB] unterminated node name");

                ByteSpan nodeName { name, static_cast<size_t>(structEnd - name) };

                if (state.depth == 1) {
                    state.memory = stringStartsWith(nodeName, kMemory);
                    state.reserved = stringEquals(nodeName, kReservedMemory);
                    state.chosen = stringEquals(nodeName, kChosen);
                } else if (state.depth == 2 && state.reserved) {
                    state.atf = stringEquals(nodeName, kAtf) || stringEquals(nodeName, kBl31) ||
                            stringEquals(nodeName, kSecmon) || stringEquals(nodeName, kOptee) ||
                            stringEquals(nodeName, kTee);
                }

                tok = reinterpret_cast<const volatile uint32_t*>(FdtAlign(name, len + 1));
                ++state.depth;
                break;
            }
            case FDT::END_NODE:
                --state.depth;
                if (state.depth == 1) {
                    state.memory = state.reserved = state.chosen = false;
                } else if (state.depth == 2) {
                    state.atf = false;
                }
                break;
            case FDT::PROP: {
                uint32_t len { Be32(tok[0]) };
                uint32_t off { Be32(tok[1]) };
                const volatile uint32_t* data { tok + 2 };
                ByteSpan name { strings + off, Be32(hdr->sizeStrings) - off };

                if (stringEquals(name, kReg) && len >= 16) {
                    if (state.memory && !state.foundMemory) {
                        map.memBase = getCombinedCell({ data, 2 });
                        map.memSize = getCombinedCell({ data + 2, 2 });
                        state.foundMemory = true;
                    } else if (state.atf) {
                        map.atfBase = getCombinedCell({ data, 2 });
                        map.atfSize = getCombinedCell({ data + 2, 2 });
                    }
                } else if (state.chosen && state.depth == 2 && (len == 4 || len == 8)) {
                    if (stringEquals(name, kInitrdStart)) {
                        state.initrdStart = getInitrdAddress({ data, len / sizeof(*data) });
                        state.hasInitrdStart = true;
                    } else if (stringEquals(name, kInitrdEnd)) {
                        state.initrdEnd = getInitrdAddress({ data, len / sizeof(*data) });
                        state.hasInitrdEnd = true;
                    }
                }

                tok = reinterpret_cast<const volatile uint32_t*>(FdtAlign(data, len));
                break;
            }
            case FDT::NOP:
                break;
            case FDT::END:
                if (!state.foundMemory)
                    HvPanic("[ERROR][DTB] memory is missing");

                if (state.hasInitrdStart && state.hasInitrdEnd &&
                        state.initrdEnd > state.initrdStart) {
                    map.cpioArchiveBase = state.initrdStart;
                    map.cpioArchiveSize = state.initrdEnd - state.initrdStart;
                }
                return map;
            default:
                HvPanic("[ERROR][DTB] invalid structure token");
        }
    }
}

DeviceNode TreeParser::FindDevice(std::span<const std::string_view> devices) const {
    validateHeader();

    if (devices.empty())
        return {};

    const volatile FdtHeader* hdr { reinterpret_cast<const volatile FdtHeader*>(m_dtb) };
    const volatile uint32_t* tok { reinterpret_cast<const volatile uint32_t*>(
            m_dtb + Be32(hdr->structOff)) };
    const volatile uint8_t* strings { reinterpret_cast<const volatile uint8_t*>(
            m_dtb + Be32(hdr->stringsOff)) };
    const volatile uint8_t* structEnd { reinterpret_cast<const volatile uint8_t*>(
            m_dtb + Be32(hdr->structOff) + Be32(hdr->sizeStructs)) };
    BusLevel stack[MAX_DEPTH] {};
    uint32_t depth {};

    while (true) {
        switch (static_cast<FDT>(Be32(*tok++))) {
            case FDT::BEGIN_NODE: {
                const volatile uint8_t* name { reinterpret_cast<const volatile uint8_t*>(tok) };
                uint32_t len {};

                while (name + len < structEnd && name[len] != 0) {
                    ++len;
                }

                if (name + len == structEnd)
                    HvPanic("[ERROR][DTB] unterminated node name");

                tok = reinterpret_cast<const volatile uint32_t*>(FdtAlign(name, len + 1));

                if (depth < MAX_DEPTH)
                    stack[depth] = { DEFAULT_ADDRESS_CELLS, DEFAULT_SIZE_CELLS, {}, false, {} };

                ++depth;
                break;
            }
            case FDT::END_NODE:
                if (depth > 0)
                    --depth;

                if (depth < MAX_DEPTH && stack[depth].matched) {
                    const BusLevel& node { stack[depth] };
                    uint32_t addressCells { depth ? stack[depth - 1].addressCells
                                                  : DEFAULT_ADDRESS_CELLS };
                    uint32_t sizeCells { depth ? stack[depth - 1].sizeCells : DEFAULT_SIZE_CELLS };
                    uint32_t stride { (addressCells + sizeCells) * CELL_SIZE_BYTES };

                    if (!node.reg.empty() && stride && node.reg.size_bytes() % stride == 0) {
                        DeviceNode out {};

                        for (uint32_t off {}; off + stride <= node.reg.size_bytes() &&
                                out.regionCount < DT_MAX_REGIONS;
                                off += stride) {
                            std::span<Cell> entry { node.reg.subspan(off / CELL_SIZE_BYTES) };
                            uint64_t base { getCombinedCell(entry.subspan(0, addressCells)) };
                            uint64_t size { getCombinedCell(
                                    entry.subspan(addressCells, sizeCells)) };

                            out.regions[out.regionCount] = { translate(stack, depth, base), size };
                            ++out.regionCount;
                        }

                        out.isFound = true;
                        return out;
                    }
                }
                break;
            case FDT::PROP: {
                uint32_t len { Be32(tok[0]) };
                uint32_t off { Be32(tok[1]) };
                std::span<Cell> data { tok + 2, len / CELL_SIZE_BYTES };
                uint32_t level { depth ? depth - 1 : 0 };

                if (level < MAX_DEPTH) {
                    BusLevel& node { stack[level] };
                    ByteSpan name { strings + off, Be32(hdr->sizeStrings) - off };

                    if (stringEquals(name, kAddressCells) && len >= 4) {
                        node.addressCells = Be32(data[0]);
                    } else if (stringEquals(name, kSizeCells) && len >= 4) {
                        node.sizeCells = Be32(data[0]);
                    } else if (stringEquals(name, kRanges)) {
                        node.ranges = data;
                    } else if (stringEquals(name, kReg)) {
                        node.reg = data;
                    } else if (stringEquals(name, kCompatible) && len) {
                        ByteSpan list { reinterpret_cast<const volatile uint8_t*>(data.data()),
                            len };
                        node.matched = matchesCompatible(list, devices);
                    }
                }

                tok = reinterpret_cast<const volatile uint32_t*>(FdtAlign(data.data(), len));
                break;
            }
            case FDT::NOP:
                break;
            case FDT::END:
                return {};
            default:
                HvPanic("[ERROR][DTB] invalid structure token");
        }
    }
}

MmioMap TreeParser::GetHostMmio() const {
    DeviceNode uart { FindDevice(kUartCompatible) };
    DeviceNode gic { FindDevice(kGicCompatible) };

    if (!uart.isFound || uart.regionCount == 0)
        HvPanic("[ERROR][DTB] host UART is missing");

    if (!gic.isFound || gic.regionCount < 4)
        HvPanic("[ERROR][DTB] host GIC is missing required regions");

    constexpr std::array<uint64_t, 10> expected {
        BSP_UART_BASE,
        BSP_UART_SIZE,
        BSP_GIC_DISTRIBUTOR_BASE,
        BSP_GIC_DISTRIBUTOR_SIZE,
        BSP_GIC_CPU_BASE,
        BSP_GIC_CPU_SIZE,
        BSP_GIC_HV_BASE,
        BSP_GIC_HV_SIZE,
        BSP_GIC_VCPU_BASE,
        BSP_GIC_VCPU_SIZE,
    };
    const std::array<uint64_t, 10> actual {
        uart.regions[0].base,
        uart.regions[0].size,
        gic.regions[0].base,
        gic.regions[0].size,
        gic.regions[1].base,
        gic.regions[1].size,
        gic.regions[2].base,
        gic.regions[2].size,
        gic.regions[3].base,
        gic.regions[3].size,
    };
    const bool isMatch { std::equal(expected.begin(), expected.end(), actual.begin()) };

    Uart::GetInstance().SetBase(uart.regions[0].base);

    if (!isMatch)
        HvPanic("[ERROR][DTB] BSP constants do not match the firmware device tree");

    Gic::SetBases(
            gic.regions[0].base, gic.regions[1].base, gic.regions[2].base, gic.regions[3].base);

    MmioMap map {};

    if (!map.AddBlocks(uart.regions[0].base, uart.regions[0].size))
        HvPanic("[ERROR][DTB] host UART mapping failed");

    for (uint32_t i {}; i < gic.regionCount; ++i) {
        if (!map.AddBlocks(gic.regions[i].base, gic.regions[i].size))
            HvPanic("[ERROR][DTB] host GIC mapping failed");
    }

    return map;
}

MmioMap TreeParser::GetGuestMmio() const {
    MmioMap map {};
    DeviceNode gic { FindDevice(kGicCompatible) };
    DeviceNode uart { FindDevice(kUartCompatible) };

    if (!gic.isFound || !gic.regionCount) {
        Log::Println("[DTB][WARN] guest tree has no GIC; guest gets no interrupt controller");
    } else {
        for (uint32_t i {}; i < gic.regionCount && i < 2; ++i) {
            if (!map.AddPages(gic.regions[i].base, gic.regions[i].base, gic.regions[i].size))
                HvPanic("[ERROR][DTB] guest GIC mapping failed");
        }
    }

    if (!uart.isFound || !uart.regionCount) {
        Log::Println("[DTB][WARN] guest tree has no PL011; guest console not mapped");
    } else {
        if (!map.AddPages(uart.regions[0].base, uart.regions[0].base, uart.regions[0].size))
            HvPanic("[ERROR][DTB] guest console mapping failed");
    }

    // HACK:
    // Some bring up packages poke the Raspberry Pi 5 PL011 IPA whatever board
    // they are running on, before the guest DTB console path takes over. Back
    // that IPA with this board's PL011 so the early guest console reaches a
    // real device. Neither tree can express this: it is a property of the
    // payload, not of the hardware. On rpi5 the guest console already sits
    // here, which is what the Covers() check notices.
    // TODO: Replace this with package/DTB-specific device routing.
    constexpr uint64_t kBringUpUartIpa { 0x107D001000ULL };

    if (!map.Covers(kBringUpUartIpa))
        if (!map.AddPages(kBringUpUartIpa, BSP_UART_BASE, SIZE_4KB))
            HvPanic("[ERROR][DTB] bring up console mapping failed");

    return map;
}
