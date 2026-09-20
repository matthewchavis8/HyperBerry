// @file test_deviceTree.cpp
// @brief Unit tests for DTB parsing and memory map extraction.

#include <gtest/gtest.h>

#include "drivers/uart/uart.h"
#include "drivers/gic/gic.h"
#include "lib/panic/panic.h"
#include "regs.inc"
#include "core/deviceTree/deviceTree.h"

#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <cstdlib>

namespace uart_test_support {

constexpr size_t kUartCaptureSize { 1024 };

char gUartCapture[kUartCaptureSize] {};
size_t gUartCaptureLen { 0 };

void Reset() {
    gUartCaptureLen = 0;
    gUartCapture[0] = '\0';
}

const char* Buffer() {
    return gUartCapture;
}

void Append(char ch) {
    if (gUartCaptureLen + 1 >= kUartCaptureSize) {
        return;
    }

    gUartCapture[gUartCaptureLen++] = ch;
    gUartCapture[gUartCaptureLen] = '\0';
}

} // namespace uart_test_support

Uart::Uart() : m_base { BSP_UART_BASE } {}

Uart& Uart::GetInstance() {
    static Uart console;
    return console;
}

void Uart::configure() const {}
void Uart::SetBase(uint64_t base) {
    m_base = base;
}
void Uart::Putc(const char ch) const {
    uart_test_support::Append(ch);
}

void Gic::SetBases(uint64_t, uint64_t, uint64_t, uint64_t) {}

[[noreturn]] void HvPanic(const char*) {
    std::abort();
}

class DtbBuilder {
    std::vector<uint8_t> m_structs;
    std::vector<char> m_strings;

    void pushBE32(uint32_t v) {
        m_structs.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        m_structs.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        m_structs.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        m_structs.push_back(static_cast<uint8_t>(v & 0xFF));
    }

public:
    uint32_t AddString(const char* s) {
        uint32_t off { static_cast<uint32_t>(m_strings.size()) };
        while (*s) {
            m_strings.push_back(*s++);
        }
        m_strings.push_back('\0');
        return off;
    }

    void BeginNode(const char* name) {
        pushBE32(1); // FDT_BEGIN_NODE
        while (*name) {
            m_structs.push_back(static_cast<uint8_t>(*name++));
        }
        m_structs.push_back(0); // null terminator
        while (m_structs.size() % 4 != 0) {
            m_structs.push_back(0);
        } // pad
    }

    void EndNode() { pushBE32(2); }

    // Generic property with raw data bytes.
    void Prop(uint32_t nameOff, const std::vector<uint8_t>& data) {
        pushBE32(3); // FDT_PROP
        pushBE32(static_cast<uint32_t>(data.size()));
        pushBE32(nameOff);
        m_structs.insert(m_structs.end(), data.begin(), data.end());
        while (m_structs.size() % 4 != 0) {
            m_structs.push_back(0);
        }
    }

    // 16-byte "reg" property: two 64-bit big-endian cells (base, size).
    void PropReg64(uint32_t nameOff, uint64_t base, uint64_t size) {
        pushBE32(3);  // FDT_PROP
        pushBE32(16); // dataLen
        pushBE32(nameOff);
        pushBE32(static_cast<uint32_t>(base >> 32));
        pushBE32(static_cast<uint32_t>(base));
        pushBE32(static_cast<uint32_t>(size >> 32));
        pushBE32(static_cast<uint32_t>(size));
    }

    void PropU64Cells(uint32_t nameOff, uint64_t value) {
        pushBE32(3); // FDT_PROP
        pushBE32(8); // dataLen
        pushBE32(nameOff);
        pushBE32(static_cast<uint32_t>(value >> 32));
        pushBE32(static_cast<uint32_t>(value));
    }

    void PropU32Cell(uint32_t nameOff, uint32_t value) {
        pushBE32(3); // FDT_PROP
        pushBE32(4); // dataLen
        pushBE32(nameOff);
        pushBE32(value);
    }

    void PropString(uint32_t nameOff, const char* value) {
        std::vector<uint8_t> data;
        while (*value) data.push_back(static_cast<uint8_t>(*value++));
        data.push_back(0);
        Prop(nameOff, data);
    }

    void PropReg32(uint32_t nameOff, uint32_t base, uint32_t size) {
        pushBE32(3);
        pushBE32(12);
        pushBE32(nameOff);
        pushBE32(0);
        pushBE32(base);
        pushBE32(size);
    }

    void Nop() { pushBE32(4); }
    void end() { pushBE32(9); }

    std::vector<uint8_t> Build() {
        constexpr uint32_t HEADER_SIZE { 40 };
        constexpr uint32_t MEMRSV_SIZE { 16 }; // just the (0,0) terminator

        uint32_t structOff { HEADER_SIZE + MEMRSV_SIZE };
        uint32_t structSz { static_cast<uint32_t>(m_structs.size()) };
        uint32_t stringsOff { structOff + structSz };
        uint32_t stringsSz { static_cast<uint32_t>(m_strings.size()) };
        uint32_t total { stringsOff + stringsSz };

        std::vector<uint8_t> blob(total, 0);

        auto w = [&](size_t off, uint32_t v) {
            blob[off + 0] = static_cast<uint8_t>((v >> 24) & 0xFF);
            blob[off + 1] = static_cast<uint8_t>((v >> 16) & 0xFF);
            blob[off + 2] = static_cast<uint8_t>((v >> 8) & 0xFF);
            blob[off + 3] = static_cast<uint8_t>(v & 0xFF);
        };

        w(0, 0xD00DFEED);   // magic
        w(4, total);        // totalSize
        w(8, structOff);    // structOff
        w(12, stringsOff);  // stringsOff
        w(16, HEADER_SIZE); // memRsvMapOff
        w(20, 17);          // version
        w(24, 16);          // lastCompVersion
        w(28, 0);           // bootCpuId
        w(32, stringsSz);   // sizeStrings
        w(36, structSz);    // sizeStructs

        // memRsvMap: 16 zero bytes (terminator) — already zeroed.

        std::copy(m_structs.begin(), m_structs.end(), blob.begin() + structOff);
        std::copy(m_strings.begin(), m_strings.end(), blob.begin() + stringsOff);

        return blob;
    }
};

static std::vector<uint8_t> buildStandardDtb(uint64_t memBase,
        uint64_t memSize,
        uint64_t atfBase,
        uint64_t atfSize,
        const char* atfNodeName = "atf") {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };

    b.BeginNode(""); // root
    b.BeginNode("memory");
    b.PropReg64(reg, memBase, memSize);
    b.EndNode();
    b.BeginNode("reserved-memory");
    b.BeginNode(atfNodeName);
    b.PropReg64(reg, atfBase, atfSize);
    b.EndNode();
    b.EndNode();
    b.EndNode();
    b.end();

    return b.Build();
}

TEST(DeviceTreeParser, NullDtbPanics) {
    EXPECT_DEATH(TreeParser { 0 }.ParseMemoryMap(), "");
}

TEST(DeviceTreeParser, BadMagicPanics) {
    uint8_t junk[64] {};
    junk[0] = 0xDE;
    junk[1] = 0xAD;
    EXPECT_DEATH(TreeParser { reinterpret_cast<uintptr_t>(junk) }.ParseMemoryMap(), "");
}

TEST(DeviceTreeParser, ValidMemoryAndAtf) {
    auto blob { buildStandardDtb(0x80000000ULL,
            0x40000000ULL, // 2GB RAM at 0x80000000
            0x80000000ULL,
            0x00080000ULL) }; // 512KB ATF

    MemoryMap map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };

    EXPECT_TRUE(map.memSize != 0);
    EXPECT_EQ(map.memBase, 0x80000000ULL);
    EXPECT_EQ(map.memSize, 0x40000000ULL);
    EXPECT_EQ(map.atfBase, 0x80000000ULL);
    EXPECT_EQ(map.atfSize, 0x00080000ULL);
    EXPECT_EQ(map.dtbBase, reinterpret_cast<uint64_t>(blob.data()));
}

TEST(DeviceTreeParser, ChosenInitrdBecomesBootArchiveRegion) {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };
    uint32_t initrdStart { b.AddString("linux,initrd-start") };
    uint32_t initrdEnd { b.AddString("linux,initrd-end") };

    b.BeginNode("");
    b.BeginNode("memory@0");
    b.PropReg64(reg, 0x0ULL, 0x40000000ULL);
    b.EndNode();
    b.BeginNode("chosen");
    b.PropU64Cells(initrdStart, 0x20000000ULL);
    b.PropU64Cells(initrdEnd, 0x20400000ULL);
    b.EndNode();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    MemoryMap map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };

    EXPECT_TRUE(map.memSize != 0);
    EXPECT_EQ(map.bootArchiveBase, 0x20000000ULL);
    EXPECT_EQ(map.bootArchiveSize, 0x400000ULL);
}

TEST(DeviceTreeParser, ChosenInitrdSupports32BitAddressCells) {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };
    uint32_t initrdStart { b.AddString("linux,initrd-start") };
    uint32_t initrdEnd { b.AddString("linux,initrd-end") };

    b.BeginNode("");
    b.BeginNode("memory@0");
    b.PropReg64(reg, 0x0ULL, 0x40000000ULL);
    b.EndNode();
    b.BeginNode("chosen");
    b.PropU32Cell(initrdStart, 0x20000000U);
    b.PropU32Cell(initrdEnd, 0x20400000U);
    b.EndNode();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    MemoryMap map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };

    EXPECT_TRUE(map.memSize != 0);
    EXPECT_EQ(map.bootArchiveBase, 0x20000000ULL);
    EXPECT_EQ(map.bootArchiveSize, 0x400000ULL);
}

TEST(DeviceTreeParser, ArchivePropertiesCanFollowMemoryAndAtfInEitherOrder) {
    for (bool reversed : { false, true }) {
        DtbBuilder b;
        auto reg { b.AddString("reg") };
        auto start { b.AddString("linux,initrd-start") };
        auto end { b.AddString("linux,initrd-end") };
        b.BeginNode("");
        b.BeginNode("memory@0");
        b.PropReg64(reg, 0, 0x100000000);
        b.EndNode();
        b.BeginNode("reserved-memory");
        b.BeginNode("atf");
        b.PropReg64(reg, 0, 0x80000);
        b.EndNode();
        b.EndNode();
        b.BeginNode("chosen");
        b.PropU64Cells(reversed ? end : start, reversed ? 0x20400000 : 0x20000000);
        b.PropU64Cells(reversed ? start : end, reversed ? 0x20000000 : 0x20400000);
        b.EndNode();
        b.EndNode();
        b.end();
        auto blob { b.Build() };
        auto map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };
        EXPECT_EQ(map.bootArchiveBase, 0x20000000);
        EXPECT_EQ(map.bootArchiveSize, 0x400000);
    }
}

TEST(DeviceTreeParser, InvalidArchiveEndpointsDoNotReserveMemory) {
    for (uint64_t end : { 0ULL, 0x1fffffffULL, 0x20000000ULL }) {
        DtbBuilder b;
        auto reg { b.AddString("reg") };
        auto startName { b.AddString("linux,initrd-start") };
        auto endName { b.AddString("linux,initrd-end") };
        b.BeginNode("");
        b.BeginNode("memory");
        b.PropReg64(reg, 0, 0x40000000);
        b.EndNode();
        b.BeginNode("chosen");
        b.PropU64Cells(startName, 0x20000000);
        if (end) b.PropU64Cells(endName, end);
        b.EndNode();
        b.EndNode();
        b.end();
        auto blob { b.Build() };
        auto map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };
        EXPECT_EQ(map.bootArchiveBase, 0);
        EXPECT_EQ(map.bootArchiveSize, 0);
    }
}

TEST(DeviceTreeParser, MemoryOnlyNoAtf) {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };

    b.BeginNode("");
    b.BeginNode("memory");
    b.PropReg64(reg, 0x40000000ULL, 0x20000000ULL);
    b.EndNode();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    MemoryMap map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };

    EXPECT_TRUE(map.memSize != 0);
    EXPECT_EQ(map.memBase, 0x40000000ULL);
    EXPECT_EQ(map.memSize, 0x20000000ULL);
    EXPECT_EQ(map.atfBase, 0ULL);
    EXPECT_EQ(map.atfSize, 0ULL);
}

TEST(DeviceTreeParser, Bl31NameMatchesAtf) {
    auto blob { buildStandardDtb(
            0x80000000ULL, 0x40000000ULL, 0x80000000ULL, 0x00080000ULL, "bl31") };

    MemoryMap map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };

    EXPECT_TRUE(map.memSize != 0);
    EXPECT_EQ(map.atfBase, 0x80000000ULL);
    EXPECT_EQ(map.atfSize, 0x00080000ULL);
}

TEST(DeviceTreeParser, NoMemoryNodePanics) {
    DtbBuilder b;
    b.AddString("reg");

    b.BeginNode("");
    b.BeginNode("cpus");
    b.EndNode();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    EXPECT_DEATH(TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap(), "");
}

TEST(DeviceTreeParser, NopTokensSkipped) {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };

    b.BeginNode("");
    b.Nop();
    b.BeginNode("memory");
    b.Nop();
    b.PropReg64(reg, 0x80000000ULL, 0x10000000ULL);
    b.Nop();
    b.EndNode();
    b.Nop();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    MemoryMap map { TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap() };

    EXPECT_TRUE(map.memSize != 0);
    EXPECT_EQ(map.memBase, 0x80000000ULL);
    EXPECT_EQ(map.memSize, 0x10000000ULL);
}

TEST(DeviceTreeParser, ShortRegPropertyPanics) {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };

    b.BeginNode("");
    b.BeginNode("memory");
    b.Prop(reg, { 0, 0, 0, 0, 0, 0, 0, 0 });
    b.EndNode();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    EXPECT_DEATH(TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.ParseMemoryMap(), "");
}

TEST(DeviceTreeParser, FindsCompatibleUsingSpan) {
    DtbBuilder b;
    uint32_t compatible { b.AddString("compatible") };
    uint32_t reg { b.AddString("reg") };
    b.BeginNode("");
    b.BeginNode("device@1000");
    b.PropString(compatible, "test,device");
    b.PropReg32(reg, 0x1000, 0x100);
    b.EndNode();
    b.EndNode();
    b.end();

    auto blob { b.Build() };
    constexpr std::string_view wanted[] { "test,device" };
    DeviceNode node {
        TreeParser { reinterpret_cast<uintptr_t>(blob.data()) }.FindCompatible(wanted)
    };

    ASSERT_TRUE(node.found);
    ASSERT_EQ(node.regionCount, 1U);
    EXPECT_EQ(node.regions[0].base, 0x1000U);
    EXPECT_EQ(node.regions[0].size, 0x100U);
}
