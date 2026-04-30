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

#include "core/dtb/dtb.h"

namespace bootpkg {

static constexpr uint32_t HV_GUEST_BOOT_PKG_MAGIC = 0x50424748U;
static constexpr uint16_t HV_GUEST_BOOT_PKG_VERSION = 1;
static constexpr uint16_t HV_GUEST_BOOT_PKG_HEADER_SIZE = 4096;

static constexpr uint32_t HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64 = 1;
static constexpr uint32_t HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_BARE_METAL_AARCH64 = 2;

static constexpr uint32_t HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT = (1U << 0);
static constexpr uint32_t HV_GUEST_BOOT_PKG_KNOWN_FLAGS = HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT;

static constexpr uint64_t GUEST_IPA_BASE = 0x0ULL;
static constexpr uint64_t GUEST_RAM_SIZE = 256ULL * 1024ULL * 1024ULL;
static constexpr uint64_t LINUX_KERNEL_LOAD_IPA = 0x80000ULL;

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

struct GuestLayout {
  uint64_t guestIpaBase;
  uint64_t guestRamSize;
  uint64_t kernelIpa;
  uint64_t kernelSize;
  uint64_t entryIpa;
  uint64_t dtbIpa;
  uint64_t dtbSize;
  uint64_t initrdIpa;
  uint64_t initrdSize;
};

struct LoadedGuest {
  uint64_t guestRamHostPa;
  uint64_t guestIpaBase;
  uint64_t guestRamSize;
  uint64_t entryIpa;
  uint64_t dtbIpa;
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
  GuestLayoutOverflow,
};

enum class LoadError : uint32_t {
  None = 0,
  MissingFirmwarePackage,
  InvalidPackage,
  GuestLayoutOverflow,
  GuestRamAllocationFailed,
  GuestDtbPatchFailed,
};

struct ValidateResult {
  bool isValid;
  ValidateError error;
  PackageView package;
};

struct LoadResult {
  bool isLoaded;
  LoadError error;
  ValidateError validateError;
  LoadedGuest guest;
};

/**
 * @brief Validate a firmware-loaded HyperBerry guest boot package.
 * @param package Base address of the package bytes.
 * @param size Size of the firmware-loaded package region.
 * @return Parsed package metadata when valid; otherwise the first error found.
 */
ValidateResult validate(const void* package, uint64_t size);

/**
 * @brief Calculate the v1 Linux guest IPA layout for a validated package.
 * @param package Validated package metadata.
 * @param out Output guest layout.
 * @return True when all components fit in the fixed v1 guest RAM region.
 */
bool calculateGuestLayout(const PackageView& package, GuestLayout& out);

/**
 * @brief Validate and copy a firmware package into newly allocated guest RAM.
 * @param map Boot-time host memory map including the firmware package region.
 * @return Loaded guest metadata when successful.
 */
LoadResult loadLinuxGuest(const MemoryMap& map);

/**
 * @brief Compute IEEE CRC32 over a byte range.
 * @param data Base address of the bytes.
 * @param size Number of bytes to include.
 * @return CRC32 value.
 */
uint32_t crc32(const void* data, uint64_t size);

} // namespace bootpkg

#endif // __BOOTPKG_H__
