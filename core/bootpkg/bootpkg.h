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

/** @brief Magic number identifying a v1 HyperBerry guest boot package (`HGBP`). */
static constexpr uint32_t HV_GUEST_BOOT_PKG_MAGIC = 0x50424748U;
/** @brief Package format version supported by this loader. */
static constexpr uint16_t HV_GUEST_BOOT_PKG_VERSION = 1;
/** @brief Fixed header region size in bytes; payload begins at this offset. */
static constexpr uint16_t HV_GUEST_BOOT_PKG_HEADER_SIZE = 4096;

/** @brief Boot protocol value for a Linux arm64 guest. */
static constexpr uint32_t HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64 = 1;
/** @brief Boot protocol value reserved for a bare-metal AArch64 payload (unsupported in v1). */
static constexpr uint32_t HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_BARE_METAL_AARCH64 = 2;

/** @brief Flags bit indicating an initrd component is present in the package. */
static constexpr uint32_t HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT = (1U << 0);
/** @brief Mask of all flag bits recognised by the v1 loader; unknown bits are rejected. */
static constexpr uint32_t HV_GUEST_BOOT_PKG_KNOWN_FLAGS = HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT;

/** @brief Fixed guest IPA base used for all v1 Linux guests. */
static constexpr uint64_t GUEST_IPA_BASE = 0x0ULL;
/** @brief Fixed guest RAM allocation size (256 MiB) for all v1 Linux guests. */
static constexpr uint64_t GUEST_RAM_SIZE = 256ULL * 1024ULL * 1024ULL;
/** @brief IPA at which the Linux kernel Image is loaded inside guest RAM. */
static constexpr uint64_t LINUX_KERNEL_LOAD_IPA = 0x80000ULL;

/**
 * @brief Decoded view of a validated v1 package header.
 * @ingroup core
 *
 * Populated by @ref validate() when the package passes all checks.
 * Offsets are byte offsets from the start of the firmware-loaded region.
 */
struct PackageView {
  uint64_t totalSize;      /**< Total firmware-loaded byte count. */
  uint32_t bootProtocol;   /**< Boot protocol identifier (e.g. `HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64`). */
  uint32_t flags;          /**< Package modifier flags. */
  uint64_t kernelOffset;   /**< Byte offset of the kernel component from the package start. */
  uint64_t kernelSize;     /**< Size in bytes of the kernel component. */
  uint64_t dtbOffset;      /**< Byte offset of the guest DTB component. */
  uint64_t dtbSize;        /**< Size in bytes of the guest DTB component. */
  uint64_t initrdOffset;   /**< Byte offset of the initrd component, or zero if absent. */
  uint64_t initrdSize;     /**< Size in bytes of the initrd component, or zero if absent. */
  uint64_t entryOffset;    /**< Entry point offset from the kernel load IPA. */
  const char* buildId;     /**< NUL-terminated build identifier string from the header, or empty string. */
};

/**
 * @brief Resolved guest IPA layout for a validated v1 package.
 * @ingroup core
 *
 * Computed by @ref calculateGuestLayout() from a @ref PackageView.
 * All addresses are guest IPAs within the fixed 256 MiB guest RAM region.
 */
struct GuestLayout {
  uint64_t guestIpaBase;  /**< Guest IPA base (always `GUEST_IPA_BASE` in v1). */
  uint64_t guestRamSize;  /**< Total guest RAM size (always `GUEST_RAM_SIZE` in v1). */
  uint64_t kernelIpa;     /**< IPA at which the kernel Image is loaded. */
  uint64_t kernelSize;    /**< Size in bytes of the kernel component. */
  uint64_t entryIpa;      /**< Guest entry IPA (`kernelIpa + entryOffset`). */
  uint64_t dtbIpa;        /**< IPA at which the guest DTB is placed. */
  uint64_t dtbSize;       /**< Size in bytes of the guest DTB component. */
  uint64_t initrdIpa;     /**< IPA at which the initrd is placed, or zero if absent. */
  uint64_t initrdSize;    /**< Size in bytes of the initrd, or zero if absent. */
};

/**
 * @brief Hypervisor-facing descriptor for a successfully loaded Linux guest.
 * @ingroup core
 *
 * Returned by @ref loadLinuxGuest() and consumed by @ref Vm::init() to
 * configure stage-2 mappings and seed the vCPU entry state.
 */
struct LoadedGuest {
  uint64_t guestRamHostPa;  /**< Host physical base of the allocated guest RAM block. */
  uint64_t guestIpaBase;    /**< Guest IPA base (mirrors `GuestLayout::guestIpaBase`). */
  uint64_t guestRamSize;    /**< Size in bytes of the guest RAM allocation. */
  uint64_t entryIpa;        /**< Guest IPA of the first instruction executed by the vCPU. */
  uint64_t dtbIpa;          /**< Guest IPA of the DTB passed to Linux in x0. */
};

/**
 * @brief Error codes returned by @ref validate().
 * @ingroup core
 */
enum class ValidateError : uint32_t {
  None = 0,                   /**< No error; package is valid. */
  NullPackage,                /**< Null package pointer passed. */
  TooSmall,                   /**< Region is smaller than the minimum header size. */
  BadMagic,                   /**< `magic` field does not match `HV_GUEST_BOOT_PKG_MAGIC`. */
  BadVersion,                 /**< `version` field is not `HV_GUEST_BOOT_PKG_VERSION`. */
  BadHeaderSize,              /**< `header_size` field is not `HV_GUEST_BOOT_PKG_HEADER_SIZE`. */
  BadTotalSize,               /**< `total_size` does not match the firmware-reported size. */
  BadHeaderCrc,               /**< Header CRC32 check failed. */
  BadPayloadCrc,              /**< Payload CRC32 check failed. */
  UnsupportedBootProtocol,    /**< `boot_protocol` is not `HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64`. */
  UnknownFlags,               /**< `flags` contains bits not in `HV_GUEST_BOOT_PKG_KNOWN_FLAGS`. */
  MissingKernel,              /**< `kernel_size` is zero. */
  MissingDtb,                 /**< `dtb_size` is zero. */
  BadInitrdFlag,              /**< `INITRD_PRESENT` flag is inconsistent with `initrd_size`. */
  BadKernelOffset,            /**< `kernel_offset` is not `HV_GUEST_BOOT_PKG_HEADER_SIZE`. */
  BadDtbOffset,               /**< `dtb_offset` is not 4 KiB aligned. */
  BadInitrdOffset,            /**< `initrd_offset` is not 4 KiB aligned. */
  BadTotalLayout,             /**< Component offsets/sizes do not sum to `total_size`. */
  ComponentOutOfBounds,       /**< A component range extends past `total_size`. */
  GuestLayoutOverflow,        /**< Components do not fit in the fixed guest RAM layout. */
};

/**
 * @brief Error codes returned by @ref loadLinuxGuest().
 * @ingroup core
 */
enum class LoadError : uint32_t {
  None = 0,                   /**< No error; guest loaded successfully. */
  MissingFirmwarePackage,     /**< `MemoryMap::bootPackageBase` or `bootPackageSize` is zero. */
  InvalidPackage,             /**< Package validation failed; see `validateError` for detail. */
  GuestLayoutOverflow,        /**< IPA layout calculation failed for the validated package. */
  GuestRamAllocationFailed,   /**< PMM could not allocate a contiguous 256 MiB block. */
  GuestDtbPatchFailed,        /**< Guest DTB placeholder patching failed. */
};

/**
 * @brief Result of a @ref validate() call.
 * @ingroup core
 */
struct ValidateResult {
  bool          isValid;   /**< True only when validation succeeded and @p package is populated. */
  ValidateError error;     /**< First validation error encountered, or `ValidateError::None`. */
  PackageView   package;   /**< Decoded package metadata; valid only when @p isValid is true. */
};

/**
 * @brief Result of a @ref loadLinuxGuest() call.
 * @ingroup core
 */
struct LoadResult {
  bool          isLoaded;      /**< True only when the guest was loaded and @p guest is populated. */
  LoadError     error;         /**< High-level load error, or `LoadError::None`. */
  ValidateError validateError; /**< Validation error when @p error is `LoadError::InvalidPackage`. */
  LoadedGuest   guest;         /**< Loaded guest descriptor; valid only when @p isLoaded is true. */
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
