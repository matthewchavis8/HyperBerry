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
    uint64_t memBase;         ///< Base physical address of the main RAM region.
    uint64_t memSize;         ///< Size in bytes of the main RAM region.
    uint64_t atfBase;         ///< Base physical address of the TF-A reserved region.
    uint64_t atfSize;         ///< Size in bytes of the TF-A reserved region.
    uint64_t dtbBase;         ///< Base physical address of the DTB blob.
    uint64_t dtbSize;         ///< Total size in bytes of the DTB blob.
    uint64_t bootPackageBase; ///< Base PA of the firmware-loaded guest package; zero if absent.
    uint64_t bootPackageSize; ///< Size of the firmware-loaded guest package; zero if absent.
    bool isValid;             ///< True only if all fields were successfully parsed.
};

/**
 * @brief Parse a Flattened Device Tree blob and extract key memory regions.
 * @ingroup core
 *
 * Walks the DTB structure block to locate the main RAM node and selected
 * reserved-memory children used by TF-A or secure monitor firmware.
 *
 * @param dtb Physical address of the DTB blob passed in at boot.
 * @return Parsed MemoryMap. @c isValid is set only when the main memory
 *         region was found and decoded successfully. @c bootPackageBase and
 *         @c bootPackageSize are populated from the `/chosen/linux,initrd-start`
 *         and `/chosen/linux,initrd-end` properties when present (the Raspberry
 *         Pi firmware writes these after loading the guest package).
 */
MemoryMap parseDtb(uintptr_t dtb);

/// Maximum `reg` regions recorded per discovered device.
constexpr uint32_t DT_MAX_REGIONS = 4;

/**
 * @brief One `reg` entry, translated to a CPU-physical address.
 * @ingroup core
 */
struct DeviceRegion {
    uint64_t base; ///< CPU-physical base address.
    uint64_t size; ///< Region size in bytes.
};

/**
 * @brief A device located in the DTB by compatible string.
 * @ingroup core
 */
struct DeviceNode {
    DeviceRegion regions[DT_MAX_REGIONS];
    uint32_t regionCount; ///< Number of regions decoded, capped at DT_MAX_REGIONS.
    bool found;           ///< True when a node matched one of the compatible strings.
};

/**
 * @brief Locate a device by compatible string and decode its `reg`.
 * @ingroup core
 *
 * Unlike @ref parseDtb, which matches fixed top-level node names, this walks
 * the whole tree matching the `compatible` property. Addresses honour the
 * parent's `#address-cells`/`#size-cells` and are translated through every
 * ancestor's `ranges`, which is required on boards where peripherals sit on a
 * child bus (the Pi's `/soc@107c000000`) and raw `reg` values are bus-local.
 *
 * @param dtb Physical address of the DTB blob.
 * @param compatibles Array of compatible strings, tried in order.
 * @param count Number of entries in @p compatibles.
 * @return The first matching node; @c found is false when none matched.
 */
DeviceNode dtbFindCompatible(uintptr_t dtb, const char* const* compatibles, uint32_t count);

#endif // __DTB_H__
