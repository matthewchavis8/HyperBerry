/**
 * @file bootpkg.cpp
 * @brief HyperBerry guest boot package validation.
 * @ingroup core
 */

#include "bootpkg.h"

#include <stddef.h>

namespace {

static constexpr uint64_t ALIGN_4K = 4096;

static constexpr uint64_t OFF_MAGIC = 0;
static constexpr uint64_t OFF_VERSION = 4;
static constexpr uint64_t OFF_HEADER_SIZE = 6;
static constexpr uint64_t OFF_TOTAL_SIZE = 8;
static constexpr uint64_t OFF_HEADER_CRC32 = 16;
static constexpr uint64_t OFF_PAYLOAD_CRC32 = 20;
static constexpr uint64_t OFF_BOOT_PROTOCOL = 24;
static constexpr uint64_t OFF_FLAGS = 28;
static constexpr uint64_t OFF_KERNEL_OFFSET = 32;
static constexpr uint64_t OFF_KERNEL_SIZE = 40;
static constexpr uint64_t OFF_DTB_OFFSET = 48;
static constexpr uint64_t OFF_DTB_SIZE = 56;
static constexpr uint64_t OFF_INITRD_OFFSET = 64;
static constexpr uint64_t OFF_INITRD_SIZE = 72;
static constexpr uint64_t OFF_ENTRY_OFFSET = 80;
static constexpr uint64_t OFF_BUILD_ID = 88;

static constexpr uint64_t CHECKSUM_FIELDS_START = OFF_HEADER_CRC32;
static constexpr uint64_t CHECKSUM_FIELDS_END = OFF_PAYLOAD_CRC32 + sizeof(uint32_t);

uint16_t readLe16(const uint8_t* data, uint64_t off) {
  return static_cast<uint16_t>(data[off])
       | static_cast<uint16_t>(data[off + 1] << 8);
}

uint32_t readLe32(const uint8_t* data, uint64_t off) {
  return static_cast<uint32_t>(data[off])
       | (static_cast<uint32_t>(data[off + 1]) << 8)
       | (static_cast<uint32_t>(data[off + 2]) << 16)
       | (static_cast<uint32_t>(data[off + 3]) << 24);
}

uint64_t readLe64(const uint8_t* data, uint64_t off) {
  return static_cast<uint64_t>(readLe32(data, off))
       | (static_cast<uint64_t>(readLe32(data, off + 4)) << 32);
}

uint64_t align4k(uint64_t value) {
  return (value + ALIGN_4K - 1) & ~(ALIGN_4K - 1);
}

bool addOverflows(uint64_t a, uint64_t b) {
  return a > UINT64_MAX - b;
}

bool rangeInBounds(uint64_t offset, uint64_t componentSize, uint64_t totalSize) {
  if (componentSize == 0)
    return false;
  if (offset >= totalSize)
    return false;
  if (addOverflows(offset, componentSize))
    return false;
  return offset + componentSize <= totalSize;
}

uint32_t crc32Update(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (uint32_t bit {}; bit < 8; bit++) {
    uint32_t mask = 0U - (crc & 1U);
    crc = (crc >> 1) ^ (0xEDB88320U & mask);
  }

  return crc;
}

uint32_t headerCrc32(const uint8_t* data) {
  uint32_t crc = 0xFFFFFFFFU;

  for (uint64_t i {}; i < bootpkg::HGBP_HEADER_SIZE; i++) {
    uint8_t byte = data[i];
    if (i >= CHECKSUM_FIELDS_START && i < CHECKSUM_FIELDS_END)
      byte = 0;
    crc = crc32Update(crc, byte);
  }

  return ~crc;
}

bootpkg::ValidateResult fail(bootpkg::ValidateError error) {
  bootpkg::ValidateResult result = {};
  result.error = error;
  return result;
}

} // namespace

namespace bootpkg {

uint32_t crc32(const void* data, uint64_t size) {
  if (data == nullptr && size != 0)
    return 0;

  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFU;

  for (uint64_t i {}; i < size; i++) {
    crc = crc32Update(crc, bytes[i]);
  }

  return ~crc;
}

ValidateResult validate(const void* package, uint64_t size) {
  if (package == nullptr)
    return fail(ValidateError::NullPackage);
  if (size < HGBP_HEADER_SIZE)
    return fail(ValidateError::TooSmall);

  const auto* bytes = static_cast<const uint8_t*>(package);

  if (readLe32(bytes, OFF_MAGIC) != HGBP_MAGIC)
    return fail(ValidateError::BadMagic);
  if (readLe16(bytes, OFF_VERSION) != HGBP_VERSION)
    return fail(ValidateError::BadVersion);
  if (readLe16(bytes, OFF_HEADER_SIZE) != HGBP_HEADER_SIZE)
    return fail(ValidateError::BadHeaderSize);

  PackageView view = {};
  view.totalSize = readLe64(bytes, OFF_TOTAL_SIZE);
  view.bootProtocol = readLe32(bytes, OFF_BOOT_PROTOCOL);
  view.flags = readLe32(bytes, OFF_FLAGS);
  view.kernelOffset = readLe64(bytes, OFF_KERNEL_OFFSET);
  view.kernelSize = readLe64(bytes, OFF_KERNEL_SIZE);
  view.dtbOffset = readLe64(bytes, OFF_DTB_OFFSET);
  view.dtbSize = readLe64(bytes, OFF_DTB_SIZE);
  view.initrdOffset = readLe64(bytes, OFF_INITRD_OFFSET);
  view.initrdSize = readLe64(bytes, OFF_INITRD_SIZE);
  view.entryOffset = readLe64(bytes, OFF_ENTRY_OFFSET);
  view.buildId = reinterpret_cast<const char*>(bytes + OFF_BUILD_ID);

  if (view.totalSize != size || view.totalSize < HGBP_HEADER_SIZE)
    return fail(ValidateError::BadTotalSize);

  uint32_t expectedHeaderCrc = readLe32(bytes, OFF_HEADER_CRC32);
  if (headerCrc32(bytes) != expectedHeaderCrc)
    return fail(ValidateError::BadHeaderCrc);

  uint32_t expectedPayloadCrc = readLe32(bytes, OFF_PAYLOAD_CRC32);
  if (crc32(bytes + HGBP_HEADER_SIZE, view.totalSize - HGBP_HEADER_SIZE) != expectedPayloadCrc)
    return fail(ValidateError::BadPayloadCrc);

  if (view.bootProtocol != HGBP_BOOT_PROTOCOL_LINUX_ARM64)
    return fail(ValidateError::UnsupportedBootProtocol);
  if ((view.flags & ~HGBP_KNOWN_FLAGS) != 0)
    return fail(ValidateError::UnknownFlags);
  if (view.kernelSize == 0)
    return fail(ValidateError::MissingKernel);
  if (view.dtbSize == 0)
    return fail(ValidateError::MissingDtb);

  bool hasInitrdFlag = (view.flags & HGBP_FLAG_INITRD_PRESENT) != 0;
  bool hasInitrd = view.initrdSize != 0;
  if (hasInitrdFlag != hasInitrd)
    return fail(ValidateError::BadInitrdFlag);
  if (!hasInitrd && view.initrdOffset != 0)
    return fail(ValidateError::BadInitrdFlag);

  if (view.kernelOffset != HGBP_HEADER_SIZE)
    return fail(ValidateError::BadKernelOffset);

  uint64_t expectedDtbOffset = align4k(view.kernelOffset + view.kernelSize);
  if (addOverflows(view.kernelOffset, view.kernelSize)
      || view.dtbOffset != expectedDtbOffset) {
    return fail(ValidateError::BadDtbOffset);
  }

  if (!rangeInBounds(view.kernelOffset, view.kernelSize, view.totalSize)
      || !rangeInBounds(view.dtbOffset, view.dtbSize, view.totalSize)) {
    return fail(ValidateError::ComponentOutOfBounds);
  }

  uint64_t expectedTotalSize {};
  if (hasInitrd) {
    uint64_t expectedInitrdOffset = align4k(view.dtbOffset + view.dtbSize);
    if (addOverflows(view.dtbOffset, view.dtbSize)
        || view.initrdOffset != expectedInitrdOffset) {
      return fail(ValidateError::BadInitrdOffset);
    }
    if (!rangeInBounds(view.initrdOffset, view.initrdSize, view.totalSize))
      return fail(ValidateError::ComponentOutOfBounds);
    if (addOverflows(view.initrdOffset, view.initrdSize))
      return fail(ValidateError::BadTotalLayout);
    expectedTotalSize = align4k(view.initrdOffset + view.initrdSize);
  } else {
    if (addOverflows(view.dtbOffset, view.dtbSize))
      return fail(ValidateError::BadTotalLayout);
    expectedTotalSize = align4k(view.dtbOffset + view.dtbSize);
  }

  if (expectedTotalSize != view.totalSize)
    return fail(ValidateError::BadTotalLayout);

  ValidateResult result = {};
  result.isValid = true;
  result.error = ValidateError::None;
  result.package = view;
  return result;
}

} // namespace bootpkg
