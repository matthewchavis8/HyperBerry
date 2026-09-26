// @file deviceTree.h
// @brief Flattened Device Tree parsing interface.
// @ingroup core
//
// Defines the boot-time memory map extracted from the firmware-provided
// DTB and exposes the parser used during early hypervisor initialisation.

#ifndef __DEVICE_TREE_H__
#define __DEVICE_TREE_H__
#include <cstdint>
#include <span>
#include <string_view>
#include "core/mm/mmu/mmioMap.h"

// @brief Boot-time physical memory layout parsed from the DTB.
// @ingroup core
//
// Contains the primary RAM range plus reserved regions needed by the
// hypervisor before the MMU and dynamic allocation are available.
struct alignas(16) MemoryMap {
    uint64_t memBase;         // Base physical address of the main RAM region.
    uint64_t memSize;         // Size in bytes of the main RAM region.
    uint64_t atfBase;         // Base physical address of the TF-A reserved region.
    uint64_t atfSize;         // Size in bytes of the TF-A reserved region.
    uint64_t dtbBase;         // Base physical address of the DTB blob.
    uint64_t dtbSize;         // Total size in bytes of the DTB blob.
    uint64_t cpioArchiveBase; // Base PA of the firmware-loaded guest archive; zero if absent.
    uint64_t cpioArchiveSize; // Size of the firmware-loaded guest archive; zero if absent.
};

// Maximum `reg` regions recorded per discovered device.
constexpr uint32_t DT_MAX_REGIONS { 8 };

// @brief One `reg` entry, translated to a CPU-physical address.
// @ingroup core
struct DeviceRegion {
    uint64_t base; // CPU-physical base address.
    uint64_t size; // Region size in bytes.
};

// @brief A device located in the DTB by compatible string.
// @ingroup core
struct alignas(16) DeviceNode {
    DeviceRegion regions[DT_MAX_REGIONS];
    uint32_t regionCount; // Number of regions decoded, capped at DT_MAX_REGIONS.
    bool isFound;         // True when a node matched one of the compatible strings.
};

/// @brief Parses a firmware provided Flattened Device Tree blob.
/// @ingroup core
class TreeParser {
private:
    uintptr_t m_dtb {}; // Physical address of the bound DTB.

    /// @brief Validate the DTB header and structure block.
    /// @return None. Panics when the DTB is invalid.
    void validateHeader() const;

public:
    /// @brief Bind the parser to a readable Flattened Device Tree blob.
    /// @param dtb Physical address of the DTB.
    constexpr explicit TreeParser(uintptr_t dtb) : m_dtb { dtb } {}

    /// @brief Parse the boot memory layout from the DTB.
    /// @return The parsed memory map. Panics when the DTB is invalid or lacks memory.
    [[nodiscard]] MemoryMap ParseMemoryMap() const;

    /// @brief Find the first device matching a compatible string.
    /// @param devices Compatible strings to match.
    /// @return The matched device, or a node with @c isFound clear when absent.
    [[nodiscard]] DeviceNode FindDevice(std::span<const std::string_view> devices) const;

    /// @brief Build and validate host MMIO mappings.
    /// @return The host MMIO map. Panics when required hardware is absent or unmappable.
    [[nodiscard]] MmioMap GetHostMmio() const;

    /// @brief Build guest MMIO mappings.
    /// @return The guest MMIO map. Optional absent devices produce warnings.
    [[nodiscard]] MmioMap GetGuestMmio() const;
};

#endif // __DEVICE_TREE_H__
