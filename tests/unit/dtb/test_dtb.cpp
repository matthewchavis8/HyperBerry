#include <gtest/gtest.h>
#include "core/dtb/dtb.h"

#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>

// ---------------------------------------------------------------------------
// DtbBuilder — constructs valid FDT blobs in memory for testing.
// All multi-byte fields are big-endian per the devicetree spec.
// ---------------------------------------------------------------------------
class DtbBuilder {
  std::vector<uint8_t> m_structs;
  std::vector<char>    m_strings;

  void pushBE32(uint32_t v) {
    m_structs.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    m_structs.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    m_structs.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
    m_structs.push_back(static_cast<uint8_t>( v        & 0xFF));
  }

public:
  uint32_t addString(const char* s) {
    uint32_t off = static_cast<uint32_t>(m_strings.size());
    while (*s) { m_strings.push_back(*s++); }
    m_strings.push_back('\0');
    return off;
  }

  void beginNode(const char* name) {
    pushBE32(1); // FDT_BEGIN_NODE
    while (*name) { m_structs.push_back(static_cast<uint8_t>(*name++)); }
    m_structs.push_back(0); // null terminator
    while (m_structs.size() % 4 != 0) { m_structs.push_back(0); } // pad
  }

  void endNode() { pushBE32(2); }

  // Generic property with raw data bytes.
  void prop(uint32_t nameOff, const std::vector<uint8_t>& data) {
    pushBE32(3); // FDT_PROP
    pushBE32(static_cast<uint32_t>(data.size()));
    pushBE32(nameOff);
    m_structs.insert(m_structs.end(), data.begin(), data.end());
    while (m_structs.size() % 4 != 0) { m_structs.push_back(0); }
  }

  // 16-byte "reg" property: two 64-bit big-endian cells (base, size).
  void propReg64(uint32_t nameOff, uint64_t base, uint64_t size) {
    pushBE32(3);  // FDT_PROP
    pushBE32(16); // dataLen
    pushBE32(nameOff);
    pushBE32(static_cast<uint32_t>(base >> 32));
    pushBE32(static_cast<uint32_t>(base));
    pushBE32(static_cast<uint32_t>(size >> 32));
    pushBE32(static_cast<uint32_t>(size));
  }

  void nop() { pushBE32(4); }
  void end() { pushBE32(9); }

  std::vector<uint8_t> build() {
    constexpr uint32_t HEADER_SIZE = 40;
    constexpr uint32_t MEMRSV_SIZE = 16; // just the (0,0) terminator

    uint32_t structOff  = HEADER_SIZE + MEMRSV_SIZE;
    uint32_t structSz   = static_cast<uint32_t>(m_structs.size());
    uint32_t stringsOff = structOff + structSz;
    uint32_t stringsSz  = static_cast<uint32_t>(m_strings.size());
    uint32_t total      = stringsOff + stringsSz;

    std::vector<uint8_t> blob(total, 0);

    auto w = [&](size_t off, uint32_t v) {
      blob[off + 0] = static_cast<uint8_t>((v >> 24) & 0xFF);
      blob[off + 1] = static_cast<uint8_t>((v >> 16) & 0xFF);
      blob[off + 2] = static_cast<uint8_t>((v >>  8) & 0xFF);
      blob[off + 3] = static_cast<uint8_t>( v        & 0xFF);
    };

    w(0,  0xD00DFEED);     // magic
    w(4,  total);           // totalSize
    w(8,  structOff);       // structOff
    w(12, stringsOff);      // stringsOff
    w(16, HEADER_SIZE);     // memRsvMapOff
    w(20, 17);              // version
    w(24, 16);              // lastCompVersion
    w(28, 0);               // bootCpuId
    w(32, stringsSz);       // sizeStrings
    w(36, structSz);        // sizeStructs

    // memRsvMap: 16 zero bytes (terminator) — already zeroed.

    std::copy(m_structs.begin(), m_structs.end(), blob.begin() + structOff);
    std::copy(m_strings.begin(), m_strings.end(), blob.begin() + stringsOff);

    return blob;
  }
};

// ---------------------------------------------------------------------------
// Helper: build a standard DTB with memory + reserved-memory/atf nodes.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> buildStandardDtb(
    uint64_t memBase, uint64_t memSize,
    uint64_t atfBase, uint64_t atfSize,
    const char* atfNodeName = "atf") {
  DtbBuilder b;
  uint32_t reg = b.addString("reg");

  b.beginNode("");  // root
    b.beginNode("memory");
      b.propReg64(reg, memBase, memSize);
    b.endNode();
    b.beginNode("reserved-memory");
      b.beginNode(atfNodeName);
        b.propReg64(reg, atfBase, atfSize);
      b.endNode();
    b.endNode();
  b.endNode();
  b.end();

  return b.build();
}

// ===========================================================================
// Tests
// ===========================================================================

TEST(DtbParser, NullDtbReturnsInvalid) {
  MemoryMap map = parseDtb(0);
  EXPECT_FALSE(map.isValid);
}

TEST(DtbParser, BadMagicReturnsInvalid) {
  uint8_t junk[64] = {};
  junk[0] = 0xDE;
  junk[1] = 0xAD;
  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(junk));
  EXPECT_FALSE(map.isValid);
}

TEST(DtbParser, ValidMemoryAndAtf) {
  auto blob = buildStandardDtb(
      0x80000000ULL, 0x40000000ULL,   // 2GB RAM at 0x80000000
      0x80000000ULL, 0x00080000ULL);  // 512KB ATF

  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(blob.data()));

  EXPECT_TRUE(map.isValid);
  EXPECT_EQ(map.memBase, 0x80000000ULL);
  EXPECT_EQ(map.memSize, 0x40000000ULL);
  EXPECT_EQ(map.atfBase, 0x80000000ULL);
  EXPECT_EQ(map.atfSize, 0x00080000ULL);
  EXPECT_EQ(map.dtbBase, reinterpret_cast<uint64_t>(blob.data()));
}

TEST(DtbParser, MemoryOnlyNoAtf) {
  DtbBuilder b;
  uint32_t reg = b.addString("reg");

  b.beginNode("");
    b.beginNode("memory");
      b.propReg64(reg, 0x40000000ULL, 0x20000000ULL);
    b.endNode();
  b.endNode();
  b.end();

  auto blob = b.build();
  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(blob.data()));

  EXPECT_TRUE(map.isValid);
  EXPECT_EQ(map.memBase, 0x40000000ULL);
  EXPECT_EQ(map.memSize, 0x20000000ULL);
  EXPECT_EQ(map.atfBase, 0ULL);
  EXPECT_EQ(map.atfSize, 0ULL);
}

TEST(DtbParser, Bl31NameMatchesAtf) {
  auto blob = buildStandardDtb(
      0x80000000ULL, 0x40000000ULL,
      0x80000000ULL, 0x00080000ULL,
      "bl31");

  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(blob.data()));

  EXPECT_TRUE(map.isValid);
  EXPECT_EQ(map.atfBase, 0x80000000ULL);
  EXPECT_EQ(map.atfSize, 0x00080000ULL);
}

TEST(DtbParser, NoMemoryNodeInvalid) {
  DtbBuilder b;
  b.addString("reg");

  b.beginNode("");
    b.beginNode("cpus");
    b.endNode();
  b.endNode();
  b.end();

  auto blob = b.build();
  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(blob.data()));
  EXPECT_FALSE(map.isValid);
}

TEST(DtbParser, NopTokensSkipped) {
  DtbBuilder b;
  uint32_t reg = b.addString("reg");

  b.beginNode("");
    b.nop();
    b.beginNode("memory");
      b.nop();
      b.propReg64(reg, 0x80000000ULL, 0x10000000ULL);
      b.nop();
    b.endNode();
    b.nop();
  b.endNode();
  b.end();

  auto blob = b.build();
  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(blob.data()));

  EXPECT_TRUE(map.isValid);
  EXPECT_EQ(map.memBase, 0x80000000ULL);
  EXPECT_EQ(map.memSize, 0x10000000ULL);
}

TEST(DtbParser, ShortRegPropertySkipped) {
  DtbBuilder b;
  uint32_t reg = b.addString("reg");

  b.beginNode("");
    b.beginNode("memory");
      // 8-byte reg — too short (needs >= 16), should be skipped
      b.prop(reg, {0, 0, 0, 0, 0, 0, 0, 0});
    b.endNode();
  b.endNode();
  b.end();

  auto blob = b.build();
  MemoryMap map = parseDtb(reinterpret_cast<uintptr_t>(blob.data()));
  EXPECT_FALSE(map.isValid);
}
