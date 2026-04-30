/**
 * @file test_bootpkg.cpp
 * @brief Unit tests for HyperBerry guest boot package validation.
 */

#include <gtest/gtest.h>

#include "core/bootpkg/bootpkg.h"
#include "core/mm/pmm/pmm.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

static uint64_t gAllocPagesReturn = 0x10000000ULL;
static uint32_t gAllocPagesOrder = UINT32_MAX;

static constexpr uint64_t kHeaderSize = bootpkg::HV_GUEST_BOOT_PKG_HEADER_SIZE;
static constexpr uint64_t kKernelSize = 0x1800;
static constexpr uint64_t kDtbSize = 0x800;
static constexpr uint64_t kInitrdSize = 0x2800;

uint64_t align4k(uint64_t value) {
  return (value + 4095ULL) & ~4095ULL;
}

void writeLe16(std::vector<uint8_t>& data, uint64_t off, uint16_t value) {
  data[off + 0] = static_cast<uint8_t>(value);
  data[off + 1] = static_cast<uint8_t>(value >> 8);
}

void writeLe32(std::vector<uint8_t>& data, uint64_t off, uint32_t value) {
  data[off + 0] = static_cast<uint8_t>(value);
  data[off + 1] = static_cast<uint8_t>(value >> 8);
  data[off + 2] = static_cast<uint8_t>(value >> 16);
  data[off + 3] = static_cast<uint8_t>(value >> 24);
}

void writeLe64(std::vector<uint8_t>& data, uint64_t off, uint64_t value) {
  writeLe32(data, off, static_cast<uint32_t>(value));
  writeLe32(data, off + 4, static_cast<uint32_t>(value >> 32));
}

void pushBE32(std::vector<uint8_t>& data, uint32_t value) {
  data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >>  8) & 0xFF));
  data.push_back(static_cast<uint8_t>( value        & 0xFF));
}

uint32_t readBe32(const std::vector<uint8_t>& data, uint64_t off) {
  return (static_cast<uint32_t>(data[off]) << 24)
       | (static_cast<uint32_t>(data[off + 1]) << 16)
       | (static_cast<uint32_t>(data[off + 2]) << 8)
       | static_cast<uint32_t>(data[off + 3]);
}

uint64_t readBe64Cells(const std::vector<uint8_t>& data, uint64_t off) {
  return (static_cast<uint64_t>(readBe32(data, off)) << 32)
       | static_cast<uint64_t>(readBe32(data, off + 4));
}

void fillRange(std::vector<uint8_t>& data, uint64_t off, uint64_t size, uint8_t value) {
  for (uint64_t i {}; i < size; i++) {
    data[off + i] = static_cast<uint8_t>(value + i);
  }
}

class DtbBuilder {
  std::vector<uint8_t> m_structs;
  std::vector<char>    m_strings;

public:
  uint32_t addString(const char* str) {
    uint32_t off = static_cast<uint32_t>(m_strings.size());
    while (*str) {
      m_strings.push_back(*str);
      str++;
    }
    m_strings.push_back('\0');
    return off;
  }

  void beginNode(const char* name) {
    pushBE32(m_structs, 1);
    while (*name) {
      m_structs.push_back(static_cast<uint8_t>(*name));
      name++;
    }
    m_structs.push_back(0);
    while ((m_structs.size() & 3U) != 0)
      m_structs.push_back(0);
  }

  void endNode() {
    pushBE32(m_structs, 2);
  }

  void propReg64(uint32_t nameOff, uint64_t base, uint64_t size) {
    pushBE32(m_structs, 3);
    pushBE32(m_structs, 16);
    pushBE32(m_structs, nameOff);
    pushBE32(m_structs, static_cast<uint32_t>(base >> 32));
    pushBE32(m_structs, static_cast<uint32_t>(base));
    pushBE32(m_structs, static_cast<uint32_t>(size >> 32));
    pushBE32(m_structs, static_cast<uint32_t>(size));
  }

  void propU64Cells(uint32_t nameOff, uint64_t value) {
    pushBE32(m_structs, 3);
    pushBE32(m_structs, 8);
    pushBE32(m_structs, nameOff);
    pushBE32(m_structs, static_cast<uint32_t>(value >> 32));
    pushBE32(m_structs, static_cast<uint32_t>(value));
  }

  void end() {
    pushBE32(m_structs, 9);
  }

  std::vector<uint8_t> build() {
    constexpr uint32_t headerSize = 40;
    constexpr uint32_t reserveSize = 16;

    uint32_t structOff = headerSize + reserveSize;
    uint32_t structSize = static_cast<uint32_t>(m_structs.size());
    uint32_t stringsOff = structOff + structSize;
    uint32_t stringsSize = static_cast<uint32_t>(m_strings.size());
    uint32_t totalSize = stringsOff + stringsSize;

    std::vector<uint8_t> blob(totalSize, 0);
    auto writeHeader = [&](uint32_t off, uint32_t value) {
      blob[off + 0] = static_cast<uint8_t>((value >> 24) & 0xFF);
      blob[off + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
      blob[off + 2] = static_cast<uint8_t>((value >>  8) & 0xFF);
      blob[off + 3] = static_cast<uint8_t>( value        & 0xFF);
    };

    writeHeader(0, 0xD00DFEED);
    writeHeader(4, totalSize);
    writeHeader(8, structOff);
    writeHeader(12, stringsOff);
    writeHeader(16, headerSize);
    writeHeader(20, 17);
    writeHeader(24, 16);
    writeHeader(28, 0);
    writeHeader(32, stringsSize);
    writeHeader(36, structSize);

    std::copy(m_structs.begin(), m_structs.end(), blob.begin() + structOff);
    std::copy(m_strings.begin(), m_strings.end(), blob.begin() + stringsOff);
    return blob;
  }
};

std::vector<uint8_t> buildGuestDtb(bool includeInitrdPlaceholders = true) {
  DtbBuilder b;
  uint32_t reg = b.addString("reg");
  uint32_t initrdStart = b.addString("linux,initrd-start");
  uint32_t initrdEnd = b.addString("linux,initrd-end");

  b.beginNode("");
    b.beginNode("memory@0");
      b.propReg64(reg, 0, 0);
    b.endNode();
    b.beginNode("chosen");
      if (includeInitrdPlaceholders) {
        b.propU64Cells(initrdStart, 0);
        b.propU64Cells(initrdEnd, 0);
      }
    b.endNode();
  b.endNode();
  b.end();

  auto dtb = b.build();
  dtb.resize(kDtbSize, 0);
  return dtb;
}

bool strEq(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    if (*str1 != *str2)
      return false;
    str1++;
    str2++;
  }

  return *str1 == *str2;
}

uint64_t alignStruct(uint64_t off, uint32_t bytes) {
  return (off + bytes + 3ULL) & ~3ULL;
}

uint64_t findPropData(std::vector<uint8_t>& dtb, const char* wanted) {
  uint64_t tok = readBe32(dtb, 8);
  const char* strings = reinterpret_cast<const char*>(dtb.data() + readBe32(dtb, 12));

  while (true) {
    uint32_t token = readBe32(dtb, tok);
    tok += 4;

    if (token == 1) {
      uint64_t name = tok;
      while (dtb[tok] != 0)
        tok++;
      tok = alignStruct(name, static_cast<uint32_t>(tok - name + 1));
    } else if (token == 2 || token == 4) {
      continue;
    } else if (token == 3) {
      uint32_t dataLen = readBe32(dtb, tok);
      uint32_t nameOff = readBe32(dtb, tok + 4);
      uint64_t dataOff = tok + 8;
      if (strEq(strings + nameOff, wanted))
        return dataOff;
      tok = alignStruct(dataOff, dataLen);
    } else if (token == 9) {
      return 0;
    } else {
      return 0;
    }
  }
}

void writeChecksums(std::vector<uint8_t>& data) {
  writeLe32(data, 16, 0);
  writeLe32(data, 20, 0);
  uint32_t headerCrc = bootpkg::crc32(data.data(), kHeaderSize);
  uint32_t payloadCrc = bootpkg::crc32(data.data() + kHeaderSize,
                                       data.size() - kHeaderSize);
  writeLe32(data, 16, headerCrc);
  writeLe32(data, 20, payloadCrc);
}

std::vector<uint8_t> buildPackage(bool withInitrd = true) {
  auto guestDtb = buildGuestDtb();
  uint64_t kernelOffset = kHeaderSize;
  uint64_t dtbOffset = align4k(kernelOffset + kKernelSize);
  uint64_t initrdOffset = withInitrd ? align4k(dtbOffset + kDtbSize) : 0;
  uint64_t totalSize = withInitrd
      ? align4k(initrdOffset + kInitrdSize)
      : align4k(dtbOffset + kDtbSize);

  std::vector<uint8_t> data(totalSize, 0);

  writeLe32(data, 0, bootpkg::HV_GUEST_BOOT_PKG_MAGIC);
  writeLe16(data, 4, bootpkg::HV_GUEST_BOOT_PKG_VERSION);
  writeLe16(data, 6, bootpkg::HV_GUEST_BOOT_PKG_HEADER_SIZE);
  writeLe64(data, 8, totalSize);
  writeLe32(data, 24, bootpkg::HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64);
  writeLe32(data, 28, withInitrd ? bootpkg::HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT : 0);
  writeLe64(data, 32, kernelOffset);
  writeLe64(data, 40, kKernelSize);
  writeLe64(data, 48, dtbOffset);
  writeLe64(data, 56, kDtbSize);
  writeLe64(data, 64, initrdOffset);
  writeLe64(data, 72, withInitrd ? kInitrdSize : 0);
  writeLe64(data, 80, 0);

  fillRange(data, kernelOffset, kKernelSize, 0x10);
  std::copy(guestDtb.begin(), guestDtb.end(), data.begin() + dtbOffset);
  if (withInitrd)
    fillRange(data, initrdOffset, kInitrdSize, 0x50);

  writeChecksums(data);
  return data;
}

bootpkg::ValidateError validateError(std::vector<uint8_t>& data) {
  writeChecksums(data);
  return bootpkg::validate(data.data(), data.size()).error;
}

} // namespace

namespace pmm {

uint64_t allocPages(uint32_t order) {
  gAllocPagesOrder = order;
  return gAllocPagesReturn;
}

} // namespace pmm

TEST(BootPkg, ValidPackageWithInitrdParsesMetadata) {
  auto data = buildPackage();

  auto result = bootpkg::validate(data.data(), data.size());

  ASSERT_TRUE(result.isValid);
  EXPECT_EQ(result.error, bootpkg::ValidateError::None);
  EXPECT_EQ(result.package.kernelOffset, kHeaderSize);
  EXPECT_EQ(result.package.kernelSize, kKernelSize);
  EXPECT_EQ(result.package.dtbSize, kDtbSize);
  EXPECT_EQ(result.package.initrdSize, kInitrdSize);
  EXPECT_EQ(result.package.flags, bootpkg::HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT);
}

TEST(BootPkg, ValidPackageWithoutInitrdParsesMetadata) {
  auto data = buildPackage(false);

  auto result = bootpkg::validate(data.data(), data.size());

  ASSERT_TRUE(result.isValid);
  EXPECT_EQ(result.package.initrdOffset, 0ULL);
  EXPECT_EQ(result.package.initrdSize, 0ULL);
  EXPECT_EQ(result.package.flags, 0U);
}

TEST(BootPkg, RejectsBadMagic) {
  auto data = buildPackage();
  writeLe32(data, 0, 0xBAD00000U);
  writeChecksums(data);

  EXPECT_EQ(bootpkg::validate(data.data(), data.size()).error,
            bootpkg::ValidateError::BadMagic);
}

TEST(BootPkg, RejectsBadVersion) {
  auto data = buildPackage();
  writeLe16(data, 4, 2);
  writeChecksums(data);

  EXPECT_EQ(bootpkg::validate(data.data(), data.size()).error,
            bootpkg::ValidateError::BadVersion);
}

TEST(BootPkg, RejectsBadHeaderSize) {
  auto data = buildPackage();
  writeLe16(data, 6, 2048);
  writeChecksums(data);

  EXPECT_EQ(bootpkg::validate(data.data(), data.size()).error,
            bootpkg::ValidateError::BadHeaderSize);
}

TEST(BootPkg, RejectsTruncatedFirmwareLoadedSize) {
  auto data = buildPackage();

  EXPECT_EQ(bootpkg::validate(data.data(), data.size() - 1).error,
            bootpkg::ValidateError::BadTotalSize);
}

TEST(BootPkg, AcceptsTrailingFirmwarePadding) {
  auto data = buildPackage();
  uint64_t packageSize = data.size();
  data.resize(data.size() + 4096, 0);

  auto result = bootpkg::validate(data.data(), data.size());

  ASSERT_TRUE(result.isValid);
  EXPECT_EQ(result.package.totalSize, packageSize);
}

TEST(BootPkg, RejectsHeaderCrcMismatch) {
  auto data = buildPackage();
  data[100] ^= 0x1U;

  EXPECT_EQ(bootpkg::validate(data.data(), data.size()).error,
            bootpkg::ValidateError::BadHeaderCrc);
}

TEST(BootPkg, RejectsPayloadCrcMismatch) {
  auto data = buildPackage();
  data[kHeaderSize] ^= 0x1U;

  EXPECT_EQ(bootpkg::validate(data.data(), data.size()).error,
            bootpkg::ValidateError::BadPayloadCrc);
}

TEST(BootPkg, RejectsUnsupportedBootProtocol) {
  auto data = buildPackage();
  writeLe32(data, 24, bootpkg::HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_BARE_METAL_AARCH64);

  EXPECT_EQ(validateError(data),
            bootpkg::ValidateError::UnsupportedBootProtocol);
}

TEST(BootPkg, RejectsUnknownFlags) {
  auto data = buildPackage();
  writeLe32(data, 28, bootpkg::HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT | (1U << 8));

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::UnknownFlags);
}

TEST(BootPkg, RejectsMissingKernel) {
  auto data = buildPackage();
  writeLe64(data, 40, 0);

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::MissingKernel);
}

TEST(BootPkg, RejectsMissingDtb) {
  auto data = buildPackage();
  writeLe64(data, 56, 0);

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::MissingDtb);
}

TEST(BootPkg, RejectsInitrdFlagMismatch) {
  auto data = buildPackage();
  writeLe32(data, 28, 0);

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::BadInitrdFlag);
}

TEST(BootPkg, RejectsNonCanonicalKernelOffset) {
  auto data = buildPackage();
  writeLe64(data, 32, kHeaderSize + 4096);

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::BadKernelOffset);
}

TEST(BootPkg, RejectsNonCanonicalDtbOffset) {
  auto data = buildPackage();
  writeLe64(data, 48, kHeaderSize + kKernelSize);

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::BadDtbOffset);
}

TEST(BootPkg, RejectsNonCanonicalInitrdOffset) {
  auto data = buildPackage();
  writeLe64(data, 64, align4k(kHeaderSize + kKernelSize) + kDtbSize);

  EXPECT_EQ(validateError(data), bootpkg::ValidateError::BadInitrdOffset);
}

TEST(BootPkg, RejectsBadTotalLayout) {
  auto data = buildPackage();
  data.resize(data.size() + 4096, 0);
  writeLe64(data, 8, data.size());
  writeChecksums(data);

  EXPECT_EQ(bootpkg::validate(data.data(), data.size()).error,
            bootpkg::ValidateError::BadTotalLayout);
}

TEST(BootPkg, CalculatesGuestLayoutWithInitrd) {
  auto data = buildPackage();
  auto parsed = bootpkg::validate(data.data(), data.size());
  ASSERT_TRUE(parsed.isValid);

  bootpkg::GuestLayout layout = {};
  ASSERT_TRUE(bootpkg::calculateGuestLayout(parsed.package, layout));

  EXPECT_EQ(layout.guestIpaBase, bootpkg::GUEST_IPA_BASE);
  EXPECT_EQ(layout.guestRamSize, bootpkg::GUEST_RAM_SIZE);
  EXPECT_EQ(layout.kernelIpa, bootpkg::LINUX_KERNEL_LOAD_IPA);
  EXPECT_EQ(layout.kernelSize, kKernelSize);
  EXPECT_EQ(layout.entryIpa, bootpkg::LINUX_KERNEL_LOAD_IPA);
  EXPECT_EQ(layout.initrdSize, kInitrdSize);
  EXPECT_EQ(layout.initrdIpa & (0x200000ULL - 1), 0ULL);
  EXPECT_EQ(layout.dtbSize, kDtbSize);
  EXPECT_EQ(layout.dtbIpa & (0x10000ULL - 1), 0ULL);
  EXPECT_LT(layout.dtbIpa, layout.initrdIpa);
}

TEST(BootPkg, CalculatesGuestLayoutWithoutInitrd) {
  auto data = buildPackage(false);
  auto parsed = bootpkg::validate(data.data(), data.size());
  ASSERT_TRUE(parsed.isValid);

  bootpkg::GuestLayout layout = {};
  ASSERT_TRUE(bootpkg::calculateGuestLayout(parsed.package, layout));

  EXPECT_EQ(layout.initrdIpa, 0ULL);
  EXPECT_EQ(layout.initrdSize, 0ULL);
  EXPECT_EQ(layout.dtbIpa & (0x10000ULL - 1), 0ULL);
  EXPECT_GT(layout.dtbIpa, layout.kernelIpa + layout.kernelSize);
}

TEST(BootPkg, GuestEntryOffsetMustStayInsideKernel) {
  auto data = buildPackage();
  auto parsed = bootpkg::validate(data.data(), data.size());
  ASSERT_TRUE(parsed.isValid);
  parsed.package.entryOffset = parsed.package.kernelSize;

  bootpkg::GuestLayout layout = {};
  EXPECT_FALSE(bootpkg::calculateGuestLayout(parsed.package, layout));
}

TEST(BootPkg, GuestLayoutRejectsOversizedKernel) {
  auto data = buildPackage();
  auto parsed = bootpkg::validate(data.data(), data.size());
  ASSERT_TRUE(parsed.isValid);
  parsed.package.kernelSize = bootpkg::GUEST_RAM_SIZE;

  bootpkg::GuestLayout layout = {};
  EXPECT_FALSE(bootpkg::calculateGuestLayout(parsed.package, layout));
}

TEST(BootPkg, LoadLinuxGuestCopiesPackageComponents) {
  std::vector<uint8_t> guestRam(bootpkg::GUEST_RAM_SIZE);
  auto data = buildPackage();
  auto parsed = bootpkg::validate(data.data(), data.size());
  ASSERT_TRUE(parsed.isValid);
  bootpkg::GuestLayout layout = {};
  ASSERT_TRUE(bootpkg::calculateGuestLayout(parsed.package, layout));

  gAllocPagesReturn = reinterpret_cast<uint64_t>(guestRam.data());
  gAllocPagesOrder = UINT32_MAX;

  MemoryMap map = {};
  map.bootPackageBase = reinterpret_cast<uint64_t>(data.data());
  map.bootPackageSize = data.size();

  auto result = bootpkg::loadLinuxGuest(map);

  ASSERT_TRUE(result.isLoaded);
  EXPECT_EQ(result.error, bootpkg::LoadError::None);
  EXPECT_EQ(gAllocPagesOrder, 16U);
  EXPECT_EQ(result.guest.guestRamHostPa, reinterpret_cast<uint64_t>(guestRam.data()));
  EXPECT_EQ(result.guest.guestIpaBase, bootpkg::GUEST_IPA_BASE);
  EXPECT_EQ(result.guest.guestRamSize, bootpkg::GUEST_RAM_SIZE);
  EXPECT_EQ(result.guest.entryIpa, bootpkg::LINUX_KERNEL_LOAD_IPA);
  EXPECT_EQ(result.guest.dtbIpa, layout.dtbIpa);

  uint64_t kernelOff = layout.kernelIpa - bootpkg::GUEST_IPA_BASE;
  uint64_t dtbOff = layout.dtbIpa - bootpkg::GUEST_IPA_BASE;
  uint64_t initrdOff = layout.initrdIpa - bootpkg::GUEST_IPA_BASE;

  EXPECT_EQ(guestRam[kernelOff], data[parsed.package.kernelOffset]);
  EXPECT_EQ(guestRam[kernelOff + kKernelSize - 1],
            data[parsed.package.kernelOffset + kKernelSize - 1]);
  EXPECT_EQ(guestRam[dtbOff], data[parsed.package.dtbOffset]);
  EXPECT_EQ(guestRam[dtbOff + kDtbSize - 1],
            data[parsed.package.dtbOffset + kDtbSize - 1]);
  EXPECT_EQ(guestRam[initrdOff], data[parsed.package.initrdOffset]);
  EXPECT_EQ(guestRam[initrdOff + kInitrdSize - 1],
            data[parsed.package.initrdOffset + kInitrdSize - 1]);
}

TEST(BootPkg, LoadLinuxGuestPatchesGuestDtb) {
  std::vector<uint8_t> guestRam(bootpkg::GUEST_RAM_SIZE);
  auto data = buildPackage();
  auto parsed = bootpkg::validate(data.data(), data.size());
  ASSERT_TRUE(parsed.isValid);

  bootpkg::GuestLayout layout = {};
  ASSERT_TRUE(bootpkg::calculateGuestLayout(parsed.package, layout));

  gAllocPagesReturn = reinterpret_cast<uint64_t>(guestRam.data());

  MemoryMap map = {};
  map.bootPackageBase = reinterpret_cast<uint64_t>(data.data());
  map.bootPackageSize = data.size();

  auto result = bootpkg::loadLinuxGuest(map);

  ASSERT_TRUE(result.isLoaded);

  uint64_t dtbOff = layout.dtbIpa - bootpkg::GUEST_IPA_BASE;
  std::vector<uint8_t> patchedDtb(
      guestRam.begin() + dtbOff,
      guestRam.begin() + dtbOff + parsed.package.dtbSize);

  uint64_t memoryReg = findPropData(patchedDtb, "reg");
  ASSERT_NE(memoryReg, 0ULL);
  EXPECT_EQ(readBe64Cells(patchedDtb, memoryReg), bootpkg::GUEST_IPA_BASE);
  EXPECT_EQ(readBe64Cells(patchedDtb, memoryReg + 8), bootpkg::GUEST_RAM_SIZE);

  uint64_t initrdStart = findPropData(patchedDtb, "linux,initrd-start");
  uint64_t initrdEnd = findPropData(patchedDtb, "linux,initrd-end");
  ASSERT_NE(initrdStart, 0ULL);
  ASSERT_NE(initrdEnd, 0ULL);
  EXPECT_EQ(readBe64Cells(patchedDtb, initrdStart), layout.initrdIpa);
  EXPECT_EQ(readBe64Cells(patchedDtb, initrdEnd), layout.initrdIpa + layout.initrdSize);
}

TEST(BootPkg, LoadLinuxGuestFailsWhenGuestDtbPlaceholdersAreMissing) {
  auto data = buildPackage();
  auto badDtb = buildGuestDtb(false);
  uint64_t dtbOffset = 0x3000;
  std::fill(data.begin() + dtbOffset, data.begin() + dtbOffset + kDtbSize, 0);
  std::copy(badDtb.begin(), badDtb.end(), data.begin() + dtbOffset);
  writeChecksums(data);

  std::vector<uint8_t> guestRam(bootpkg::GUEST_RAM_SIZE);
  gAllocPagesReturn = reinterpret_cast<uint64_t>(guestRam.data());

  MemoryMap map = {};
  map.bootPackageBase = reinterpret_cast<uint64_t>(data.data());
  map.bootPackageSize = data.size();

  auto result = bootpkg::loadLinuxGuest(map);

  EXPECT_FALSE(result.isLoaded);
  EXPECT_EQ(result.error, bootpkg::LoadError::GuestDtbPatchFailed);
}

TEST(BootPkg, LoadLinuxGuestRejectsMissingFirmwarePackage) {
  MemoryMap map = {};

  auto result = bootpkg::loadLinuxGuest(map);

  EXPECT_FALSE(result.isLoaded);
  EXPECT_EQ(result.error, bootpkg::LoadError::MissingFirmwarePackage);
}

TEST(BootPkg, LoadLinuxGuestPropagatesValidationError) {
  auto data = buildPackage();
  writeLe32(data, 0, 0xBAD00000U);
  writeChecksums(data);

  MemoryMap map = {};
  map.bootPackageBase = reinterpret_cast<uint64_t>(data.data());
  map.bootPackageSize = data.size();

  auto result = bootpkg::loadLinuxGuest(map);

  EXPECT_FALSE(result.isLoaded);
  EXPECT_EQ(result.error, bootpkg::LoadError::InvalidPackage);
  EXPECT_EQ(result.validateError, bootpkg::ValidateError::BadMagic);
}

TEST(BootPkg, LoadLinuxGuestReportsAllocationFailure) {
  auto data = buildPackage();
  gAllocPagesReturn = 0;

  MemoryMap map = {};
  map.bootPackageBase = reinterpret_cast<uint64_t>(data.data());
  map.bootPackageSize = data.size();

  auto result = bootpkg::loadLinuxGuest(map);

  EXPECT_FALSE(result.isLoaded);
  EXPECT_EQ(result.error, bootpkg::LoadError::GuestRamAllocationFailed);
  gAllocPagesReturn = 0x10000000ULL;
}
