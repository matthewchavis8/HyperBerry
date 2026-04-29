/**
 * @file dtb.h
 * @brief Flattened Device Tree parsing interface.
 * @ingroup core
 *
 * Defines the boot-time memory map extracted from the firmware-provided
 * DTB and exposes the parser used during early hypervisor initialisation.
 */

#ifndef __DTB_H__
#define __DTB_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Boot-time physical memory layout parsed from the DTB.
 * @ingroup core
 *
 * Contains the primary RAM range plus reserved regions needed by the
 * hypervisor before the MMU and dynamic allocation are available.
 */
struct alignas(16) MemoryMap {
  uint64_t memBase;  /**< Base physical address of the main RAM region.       */
  uint64_t memSize;  /**< Size in bytes of the main RAM region.               */
  uint64_t atfBase;  /**< Base physical address of the TF-A reserved region.  */
  uint64_t atfSize;  /**< Size in bytes of the TF-A reserved region.          */
  uint64_t dtbBase;  /**< Base physical address of the DTB blob.              */
  uint64_t dtbSize;  /**< Total size in bytes of the DTB blob.                */
  uint64_t bootPackageBase; /**< Base address of firmware-loaded guest package. */
  uint64_t bootPackageSize; /**< Size in bytes of firmware-loaded guest package. */
  bool     isValid;    /**< True only if all fields were successfully parsed.   */
};

/**
 * @brief Parse a Flattened Device Tree blob and extract key memory regions.
 * @ingroup core
 *
 * Walks the DTB structure block to locate the main RAM node and selected
 * reserved-memory children used by TF-A or secure monitor firmware.
 *
 * @param dtb Physical address of the DTB blob passed in at boot.
 * @return Parsed MemoryMap. The returned map has @c isValid set only when
 *         the main memory region was found and decoded successfully.
 */
MemoryMap parseDtb(uintptr_t dtb);

#endif // __DTB_H__
