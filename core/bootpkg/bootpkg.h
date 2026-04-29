/**
 * @file bootpkg.h
 * @brief HyperBerry guest boot package validation.
 * @ingroup core
 *
 * Defines the v1 package ABI used by the Raspberry Pi firmware initramfs
 * handoff. The loader consumes one firmware-loaded package and turns it into
 * the Linux guest payload in later boot stages.
 */

#ifndef __BOOTPKG_H__
#define __BOOTPKG_H__

#include <stdint.h>

namespace bootpkg {

static constexpr uint32_t HGBP_MAGIC = 0x50424748U; // "HGBP"
static constexpr uint16_t HGBP_VERSION = 1;
static constexpr uint16_t HGBP_HEADER_SIZE = 4096;

static constexpr uint32_t HGBP_BOOT_PROTOCOL_LINUX_ARM64 = 1;
static constexpr uint32_t HGBP_BOOT_PROTOCOL_BARE_METAL_AARCH64 = 2;

static constexpr uint32_t HGBP_FLAG_INITRD_PRESENT = (1U << 0);
static constexpr uint32_t HGBP_KNOWN_FLAGS = HGBP_FLAG_INITRD_PRESENT;

struct PackageView {
  uint64_t totalSize;
  uint32_t bootProtocol;
  uint32_t flags;
  uint64_t kernelOffset;
  uint64_t kernelSize;
  uint64_t dtbOffset;
  uint64_t dtbSize;
  uint64_t initrdOffset;
  uint64_t initrdSize;
  uint64_t entryOffset;
  const char* buildId;
};

enum class ValidateError : uint32_t {
  None = 0,
  NullPackage,
  TooSmall,
  BadMagic,
  BadVersion,
  BadHeaderSize,
  BadTotalSize,
  BadHeaderCrc,
  BadPayloadCrc,
  UnsupportedBootProtocol,
  UnknownFlags,
  MissingKernel,
  MissingDtb,
  BadInitrdFlag,
  BadKernelOffset,
  BadDtbOffset,
  BadInitrdOffset,
  BadTotalLayout,
  ComponentOutOfBounds,
};

struct ValidateResult {
  bool isValid;
  ValidateError error;
  PackageView package;
};

/**
 * @brief Validate a firmware-loaded HyperBerry guest boot package.
 * @param package Base address of the package bytes.
 * @param size Size of the firmware-loaded package region.
 * @return Parsed package metadata when valid; otherwise the first error found.
 */
ValidateResult validate(const void* package, uint64_t size);

/**
 * @brief Compute IEEE CRC32 over a byte range.
 * @param data Base address of the bytes.
 * @param size Number of bytes to include.
 * @return CRC32 value.
 */
uint32_t crc32(const void* data, uint64_t size);

} // namespace bootpkg

#endif // __BOOTPKG_H__
