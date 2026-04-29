/**
 * @file test_bootpkg.cpp
 * @brief Unit tests for HyperBerry guest boot package validation.
 */

#include <gtest/gtest.h>

#include "core/bootpkg/bootpkg.h"

#include <cstdint>
#include <vector>

namespace {

static constexpr uint64_t kHeaderSize = bootpkg::HGBP_HEADER_SIZE;
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

void fillRange(std::vector<uint8_t>& data, uint64_t off, uint64_t size, uint8_t value) {
  for (uint64_t i {}; i < size; i++) {
    data[off + i] = static_cast<uint8_t>(value + i);
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
  uint64_t kernelOffset = kHeaderSize;
  uint64_t dtbOffset = align4k(kernelOffset + kKernelSize);
  uint64_t initrdOffset = withInitrd ? align4k(dtbOffset + kDtbSize) : 0;
  uint64_t totalSize = withInitrd
      ? align4k(initrdOffset + kInitrdSize)
      : align4k(dtbOffset + kDtbSize);

  std::vector<uint8_t> data(totalSize, 0);

  writeLe32(data, 0, bootpkg::HGBP_MAGIC);
  writeLe16(data, 4, bootpkg::HGBP_VERSION);
  writeLe16(data, 6, bootpkg::HGBP_HEADER_SIZE);
  writeLe64(data, 8, totalSize);
  writeLe32(data, 24, bootpkg::HGBP_BOOT_PROTOCOL_LINUX_ARM64);
  writeLe32(data, 28, withInitrd ? bootpkg::HGBP_FLAG_INITRD_PRESENT : 0);
  writeLe64(data, 32, kernelOffset);
  writeLe64(data, 40, kKernelSize);
  writeLe64(data, 48, dtbOffset);
  writeLe64(data, 56, kDtbSize);
  writeLe64(data, 64, initrdOffset);
  writeLe64(data, 72, withInitrd ? kInitrdSize : 0);
  writeLe64(data, 80, 0);

  fillRange(data, kernelOffset, kKernelSize, 0x10);
  fillRange(data, dtbOffset, kDtbSize, 0x30);
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

TEST(BootPkg, ValidPackageWithInitrdParsesMetadata) {
  auto data = buildPackage();

  auto result = bootpkg::validate(data.data(), data.size());

  ASSERT_TRUE(result.isValid);
  EXPECT_EQ(result.error, bootpkg::ValidateError::None);
  EXPECT_EQ(result.package.kernelOffset, kHeaderSize);
  EXPECT_EQ(result.package.kernelSize, kKernelSize);
  EXPECT_EQ(result.package.dtbSize, kDtbSize);
  EXPECT_EQ(result.package.initrdSize, kInitrdSize);
  EXPECT_EQ(result.package.flags, bootpkg::HGBP_FLAG_INITRD_PRESENT);
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

TEST(BootPkg, RejectsWrongFirmwareLoadedSize) {
  auto data = buildPackage();

  EXPECT_EQ(bootpkg::validate(data.data(), data.size() - 1).error,
            bootpkg::ValidateError::BadTotalSize);
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
  writeLe32(data, 24, bootpkg::HGBP_BOOT_PROTOCOL_BARE_METAL_AARCH64);

  EXPECT_EQ(validateError(data),
            bootpkg::ValidateError::UnsupportedBootProtocol);
}

TEST(BootPkg, RejectsUnknownFlags) {
  auto data = buildPackage();
  writeLe32(data, 28, bootpkg::HGBP_FLAG_INITRD_PRESENT | (1U << 8));

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

